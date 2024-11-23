/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "expressiontreemodel.h"
#include "scripthighlighter.h"

#include <core/coresettings.h>
#include <core/scripting/scriptparser.h>
#include <core/scripting/scriptregistry.h>
#include <core/track.h>
#include <gui/scripting/scripteditor.h>
#include <gui/scripting/scriptformatter.h>
#include <gui/trackselectioncontroller.h>

#include <QApplication>
#include <QBasicTimer>
#include <QDir>
#include <QGridLayout>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTextEdit>
#include <QTimerEvent>
#include <QTreeView>

#include <chrono>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto TextChangeInterval = 1500ms;
#else
constexpr auto TextChangeInterval = 1500;
#endif

constexpr auto DialogState = "Interface/ScriptEditorState";

namespace Fooyin {
class ScriptEditorPrivate
{
public:
    ScriptEditorPrivate(ScriptEditor* self, LibraryManager* libraryManager, const Track& track);

    void setupConnections();
    void setupPlaceholder();

    void updateResults();
    void updateResults(const Expression& expression);

    void selectionChanged();
    void textChanged();

    void showErrors() const;

    void saveState();
    void restoreState();

    ScriptEditor* m_self;
    FySettings m_settings;
    Track m_track;
    Track m_placeholderTrack;

    QSplitter* m_mainSplitter;
    QSplitter* m_documentSplitter;

    QTextEdit* m_editor;
    QTextEdit* m_results;
    ScriptHighlighter m_highlighter;

    QTreeView* m_expressionTree;
    ExpressionTreeModel* m_model;

    QBasicTimer m_textChangeTimer;

    ScriptParser m_parser;
    ScriptFormatter m_formatter;

    ParsedScript m_currentScript;
};

ScriptEditorPrivate::ScriptEditorPrivate(ScriptEditor* self, LibraryManager* libraryManager, const Track& track)
    : m_self{self}
    , m_track{track}
    , m_mainSplitter{new QSplitter(Qt::Horizontal, m_self)}
    , m_documentSplitter{new QSplitter(Qt::Vertical, m_self)}
    , m_editor{new QTextEdit(m_self)}
    , m_results{new QTextEdit(m_self)}
    , m_highlighter{m_editor->document()}
    , m_expressionTree{new QTreeView(m_self)}
    , m_model{new ExpressionTreeModel(m_self)}
    , m_parser{new ScriptRegistry(libraryManager)}
{
    auto* mainLayout = new QGridLayout(m_self);
    mainLayout->setContentsMargins({});
    mainLayout->addWidget(m_mainSplitter);

    m_expressionTree->setModel(m_model);
    m_expressionTree->setHeaderHidden(true);
    m_expressionTree->setSelectionMode(QAbstractItemView::SingleSelection);

    m_documentSplitter->addWidget(m_editor);
    m_documentSplitter->addWidget(m_results);
    m_documentSplitter->setStretchFactor(0, 3);
    m_documentSplitter->setStretchFactor(1, 1);

    m_mainSplitter->addWidget(m_documentSplitter);
    m_mainSplitter->addWidget(m_expressionTree);
    m_mainSplitter->setStretchFactor(0, 4);
    m_mainSplitter->setStretchFactor(1, 2);

    setupConnections();
    setupPlaceholder();
    restoreState();
}

void ScriptEditorPrivate::setupConnections()
{
    QObject::connect(m_editor, &QTextEdit::textChanged, m_self, [this]() { textChanged(); });
    QObject::connect(m_model, &QAbstractItemModel::modelReset, m_expressionTree, &QTreeView::expandAll);
    QObject::connect(m_expressionTree->selectionModel(), &QItemSelectionModel::selectionChanged, m_self,
                     [this]() { selectionChanged(); });
}

void ScriptEditorPrivate::setupPlaceholder()
{
    m_placeholderTrack.setFilePath(QDir::homePath() + "/placeholder.flac"_L1);
    m_placeholderTrack.setTitle(u"Title"_s);
    m_placeholderTrack.setAlbum(u"Album"_s);
    m_placeholderTrack.setAlbumArtists({u"Album Artist 1"_s, u"Album Artist 2"_s});
    m_placeholderTrack.setArtists({u"Artist 1"_s, u"Artist 2"_s});
    m_placeholderTrack.setDate(u"2024-08-24"_s);
    m_placeholderTrack.setTrackNumber(u"1"_s);
    m_placeholderTrack.setDiscNumber(u"1"_s);
    m_placeholderTrack.setBitDepth(16);
    m_placeholderTrack.setSampleRate(48000);
    m_placeholderTrack.setBitrate(850);
    m_placeholderTrack.setComment(u"Comment"_s);
    m_placeholderTrack.setComposers({u"Composer 1"_s, u"Composer 2"_s});
    m_placeholderTrack.setPerformers({u"Performer 1"_s, u"Performer 2"_s});
    m_placeholderTrack.setDuration(180000);
    m_placeholderTrack.setFileSize(34560000);
}

void ScriptEditorPrivate::updateResults()
{
    if(m_model->rowCount({}) > 0) {
        if(const auto* item = static_cast<ExpressionTreeItem*>(m_model->index(0, 0, {}).internalPointer())) {
            updateResults(item->expression());
        }
    }
}

void ScriptEditorPrivate::updateResults(const Expression& expression)
{
    ParsedScript script;
    script.expressions = {expression};

    const Track track    = m_track.isValid() ? m_track : m_placeholderTrack;
    const auto result    = m_parser.evaluate(script, track);
    const auto formatted = m_formatter.evaluate(result);
    m_results->setText(formatted.joinedText());
}

void ScriptEditorPrivate::selectionChanged()
{
    const auto indexes = m_expressionTree->selectionModel()->selectedIndexes();
    if(indexes.empty()) {
        return;
    }

    const auto* item = static_cast<ExpressionTreeItem*>(indexes.front().internalPointer());

    updateResults(item->expression());
}

void ScriptEditorPrivate::textChanged()
{
    m_textChangeTimer.start(TextChangeInterval, m_self);
    m_results->clear();

    m_currentScript = m_parser.parse(m_editor->toPlainText());
    m_model->populate(m_currentScript.expressions);
    updateResults();
}

void ScriptEditorPrivate::showErrors() const
{
    const auto errors = m_currentScript.errors;
    for(const ScriptError& error : errors) {
        m_results->append(error.message);
    }
}

void ScriptEditorPrivate::saveState()
{
    QByteArray byteArray;
    QDataStream out(&byteArray, QIODevice::WriteOnly);

    out << m_self->size();
    out << m_mainSplitter->saveState();
    out << m_documentSplitter->saveState();
    out << m_editor->toPlainText();

    byteArray = qCompress(byteArray, 9);

    m_settings.setValue(DialogState, byteArray);
}

void ScriptEditorPrivate::restoreState()
{
    QByteArray byteArray = m_settings.value(DialogState).toByteArray();

    static auto defaultScript = u"%track%. %title%"_s;

    if(byteArray.isEmpty()) {
        m_editor->setPlainText(defaultScript);
        return;
    }

    byteArray = qUncompress(byteArray);

    QDataStream in(&byteArray, QIODevice::ReadOnly);

    QSize dialogSize;
    QByteArray mainSplitterState;
    QByteArray documentSplitterState;
    QString editorText;

    in >> dialogSize;
    in >> mainSplitterState;
    in >> documentSplitterState;
    in >> editorText;

    if(editorText.isEmpty()) {
        editorText = defaultScript;
    }

    m_self->resize(dialogSize);
    m_mainSplitter->restoreState(mainSplitterState);
    m_documentSplitter->restoreState(documentSplitterState);
    m_editor->setPlainText(editorText);
    m_editor->moveCursor(QTextCursor::End);

    textChanged();
    m_expressionTree->expandAll();

    updateResults();
}

ScriptEditor::ScriptEditor(LibraryManager* libraryManager, const Track& track, QWidget* parent)
    : QDialog{parent}
    , p{std::make_unique<ScriptEditorPrivate>(this, libraryManager, track)}
{
    setWindowTitle(tr("Script Editor"));
}

ScriptEditor::ScriptEditor(LibraryManager* libraryManager, QWidget* parent)
    : ScriptEditor{libraryManager, {}, parent}
{ }

ScriptEditor::ScriptEditor(const QString& script, const Track& track, QWidget* parent)
    : ScriptEditor{nullptr, track, parent}
{
    p->m_editor->setPlainText(script);
}

ScriptEditor::ScriptEditor(QWidget* parent)
    : ScriptEditor{nullptr, parent}
{ }

ScriptEditor::~ScriptEditor()
{
    p->m_editor->disconnect();
    p->saveState();
}

void ScriptEditor::openEditor(const QString& script, const std::function<void(const QString&)>& callback,
                              const Track& track)
{
    auto* editor = new ScriptEditor(script, track);
    editor->setAttribute(Qt::WA_DeleteOnClose);
    editor->setModal(true);

    QObject::connect(editor, &QDialog::finished,
                     [editor, callback]() { callback(editor->p->m_editor->toPlainText()); });

    editor->show();
}

QSize ScriptEditor::sizeHint() const
{
    return {800, 500};
}

void ScriptEditor::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == p->m_textChangeTimer.timerId()) {
        p->m_textChangeTimer.stop();
        p->showErrors();
    }
    QDialog::timerEvent(event);
}
} // namespace Fooyin

#include "gui/scripting/moc_scripteditor.cpp"
