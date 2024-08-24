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

#include "sandboxdialog.h"

#include "expressiontreemodel.h"
#include "scripthighlighter.h"

#include <core/track.h>
#include <gui/trackselectioncontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QDir>
#include <QGridLayout>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTextEdit>
#include <QTimer>
#include <QTreeView>

#include <chrono>

using namespace std::chrono_literals;

constexpr auto DialogState = "Interface/ScriptSandboxState";

namespace Fooyin {
SandboxDialog::SandboxDialog(LibraryManager* libraryManager, TrackSelectionController* trackSelection, QWidget* parent)
    : QDialog{parent}
    , m_trackSelection{trackSelection}
    , m_mainSplitter{new QSplitter(Qt::Horizontal, this)}
    , m_documentSplitter{new QSplitter(Qt::Vertical, this)}
    , m_editor{new QTextEdit(this)}
    , m_results{new QTextEdit(this)}
    , m_highlighter{m_editor->document()}
    , m_expressionTree{new QTreeView(this)}
    , m_model{new ExpressionTreeModel(this)}
    , m_textChangeTimer{new QTimer(this)}
    , m_registry{libraryManager}
    , m_parser{&m_registry}
{
    setWindowTitle(tr("Script Sandbox"));

    auto* mainLayout = new QGridLayout(this);
    mainLayout->setContentsMargins({});

    m_expressionTree->setModel(m_model);
    m_expressionTree->setHeaderHidden(true);
    m_expressionTree->setSelectionMode(QAbstractItemView::SingleSelection);

    m_textChangeTimer->setSingleShot(true);

    m_documentSplitter->addWidget(m_editor);
    m_documentSplitter->addWidget(m_results);

    m_mainSplitter->addWidget(m_documentSplitter);
    m_mainSplitter->addWidget(m_expressionTree);

    m_documentSplitter->setStretchFactor(0, 3);
    m_documentSplitter->setStretchFactor(1, 1);

    m_mainSplitter->setStretchFactor(0, 4);
    m_mainSplitter->setStretchFactor(1, 2);

    mainLayout->addWidget(m_mainSplitter);

    restoreState();

    QObject::connect(m_editor, &QTextEdit::textChanged, this, &SandboxDialog::textChanged);
    QObject::connect(m_textChangeTimer, &QTimer::timeout, this, &SandboxDialog::showErrors);
    QObject::connect(m_model, &QAbstractItemModel::modelReset, m_expressionTree, &QTreeView::expandAll);
    QObject::connect(m_expressionTree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &SandboxDialog::selectionChanged);

    if(m_trackSelection) {
        QObject::connect(m_trackSelection, &TrackSelectionController::selectionChanged, this,
                         [this]() { updateResults(); });
    }

    m_placeholderTrack.setFilePath(QDir::homePath() + u"/placeholder.flac");
    m_placeholderTrack.setTitle(QStringLiteral("Title"));
    m_placeholderTrack.setAlbum(QStringLiteral("Album"));
    m_placeholderTrack.setAlbumArtists({QStringLiteral("Album Artist")});
    m_placeholderTrack.setArtists({QStringLiteral("Artist")});
    m_placeholderTrack.setDate(QStringLiteral("2024-08-24"));
    m_placeholderTrack.setTrackNumber(QStringLiteral("1"));
    m_placeholderTrack.setDiscNumber(QStringLiteral("1"));
    m_placeholderTrack.setBitDepth(16);
    m_placeholderTrack.setSampleRate(48000);
    m_placeholderTrack.setBitrate(850);
    m_placeholderTrack.setComment(QStringLiteral("Comment"));
    m_placeholderTrack.setComposer(QStringLiteral("Composer"));
    m_placeholderTrack.setPerformer(QStringLiteral("Performer"));
    m_placeholderTrack.setDuration(180000);
    m_placeholderTrack.setFileSize(34560000);
}

SandboxDialog::SandboxDialog(const QString& script, QWidget* parent)
    : SandboxDialog{nullptr, nullptr, parent}
{
    m_editor->setPlainText(script);
}

SandboxDialog::SandboxDialog(QWidget* parent)
    : SandboxDialog{nullptr, nullptr, parent}
{ }

SandboxDialog::~SandboxDialog()
{
    m_editor->disconnect();

    m_textChangeTimer->disconnect();
    m_textChangeTimer->stop();
    m_textChangeTimer->deleteLater();

    saveState();
}

void SandboxDialog::openSandbox(const QString& script, const std::function<void(const QString&)>& callback)
{
    auto* sandbox = new SandboxDialog(script);
    sandbox->setAttribute(Qt::WA_DeleteOnClose);
    sandbox->setModal(true);

    QObject::connect(sandbox, &QDialog::finished,
                     [sandbox, callback]() { callback(sandbox->m_editor->toPlainText()); });

    sandbox->show();
}

QSize SandboxDialog::sizeHint() const
{
    return {1280, 720};
}

void SandboxDialog::updateResults()
{
    if(m_model->rowCount({}) > 0) {
        if(const auto* item = static_cast<ExpressionTreeItem*>(m_model->index(0, 0, {}).internalPointer())) {
            updateResults(item->expression());
        }
    }
}

void SandboxDialog::updateResults(const Expression& expression)
{
    ParsedScript script;
    script.expressions = {expression};

    Track track{m_placeholderTrack};
    if(m_trackSelection && m_trackSelection->hasTracks()) {
        track = m_trackSelection->selectedTracks().front();
    }

    const auto result    = m_parser.evaluate(script, track);
    const auto formatted = m_formatter.evaluate(result);
    m_results->setText(formatted.joinedText());
}

void SandboxDialog::selectionChanged()
{
    const auto indexes = m_expressionTree->selectionModel()->selectedIndexes();
    if(indexes.empty()) {
        return;
    }

    const auto* item = static_cast<ExpressionTreeItem*>(indexes.front().internalPointer());

    updateResults(item->expression());
}

void SandboxDialog::textChanged()
{
    m_textChangeTimer->start(1500ms);
    m_results->clear();

    Track track{m_placeholderTrack};
    if(m_trackSelection && m_trackSelection->hasTracks()) {
        track = m_trackSelection->selectedTrack();
    }
    m_currentScript = m_parser.parse(m_editor->toPlainText(), track);

    m_model->populate(m_currentScript.expressions);
    updateResults();
}

void SandboxDialog::showErrors() const
{
    const auto errors = m_currentScript.errors;
    for(const ScriptError& error : errors) {
        m_results->append(error.message);
    }
}

void SandboxDialog::saveState()
{
    QByteArray byteArray;
    QDataStream out(&byteArray, QIODevice::WriteOnly);

    out << size();
    out << m_mainSplitter->saveState();
    out << m_documentSplitter->saveState();
    out << m_editor->toPlainText();

    byteArray = qCompress(byteArray, 9);

    m_settings.setValue(QLatin1String{DialogState}, byteArray);
}

void SandboxDialog::restoreState()
{
    QByteArray byteArray = m_settings.value(QLatin1String{DialogState}).toByteArray();

    static auto defaultScript = QStringLiteral("%track%. %title%");

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

    resize(dialogSize);
    m_mainSplitter->restoreState(mainSplitterState);
    m_documentSplitter->restoreState(documentSplitterState);
    m_editor->setPlainText(editorText);
    m_editor->moveCursor(QTextCursor::End);

    textChanged();
    m_expressionTree->expandAll();

    updateResults();
}
} // namespace Fooyin

#include "moc_sandboxdialog.cpp"
