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
#include <QGridLayout>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTextEdit>
#include <QTimer>
#include <QTreeView>

#include <chrono>

using namespace std::chrono_literals;

namespace Fooyin {
struct SandboxDialog::Private
{
    SandboxDialog* m_self;

    TrackSelectionController* m_trackSelection;
    SettingsManager* m_settings;

    QSplitter* m_mainSplitter;
    QSplitter* m_documentSplitter;

    QPlainTextEdit* m_editor;
    QTextEdit* m_results;
    ScriptHighlighter m_highlighter;

    QTreeView* m_expressionTree;
    ExpressionTreeModel m_model;

    QTimer* m_textChangeTimer;

    ScriptRegistry m_registry;
    ScriptParser m_parser;

    ParsedScript m_currentScript;

    explicit Private(SandboxDialog* self, TrackSelectionController* trackSelection, SettingsManager* settings)
        : m_self{self}
        , m_trackSelection{trackSelection}
        , m_settings{settings}
        , m_mainSplitter{new QSplitter(Qt::Horizontal, m_self)}
        , m_documentSplitter{new QSplitter(Qt::Vertical, m_self)}
        , m_editor{new QPlainTextEdit(m_self)}
        , m_results{new QTextEdit(m_self)}
        , m_highlighter{m_editor->document()}
        , m_expressionTree{new QTreeView(m_self)}
        , m_textChangeTimer{new QTimer(m_self)}
        , m_parser{&m_registry}
    {
        m_expressionTree->setModel(&m_model);
        m_expressionTree->setHeaderHidden(true);
        m_expressionTree->setSelectionMode(QAbstractItemView::SingleSelection);

        m_textChangeTimer->setSingleShot(true);
    }

    void updateResults()
    {
        if(m_model.rowCount({}) > 0) {
            if(const auto* item = static_cast<ExpressionTreeItem*>(m_model.index(0, 0, {}).internalPointer())) {
                updateResults(item->expression());
            }
        }
    }

    void updateResults(const Expression& expression)
    {
        ParsedScript script;
        script.expressions = {expression};

        Track track;
        if(m_trackSelection->hasTracks()) {
            track = m_trackSelection->selectedTracks().front();
        }

        const auto result = m_parser.evaluate(script, track);
        m_results->setText(result);
    }

    void selectionChanged()
    {
        const auto indexes = m_expressionTree->selectionModel()->selectedIndexes();
        if(indexes.empty()) {
            return;
        }

        const auto* item = static_cast<ExpressionTreeItem*>(indexes.front().internalPointer());

        updateResults(item->expression());
    }

    void textChanged()
    {
        m_textChangeTimer->start(1500ms);
        m_results->clear();

        const Track track = m_trackSelection->hasTracks() ? m_trackSelection->selectedTracks().front() : Track{};
        m_currentScript   = m_parser.parse(m_editor->toPlainText(), track);

        m_model.populate(m_currentScript.expressions);
        updateResults();
    }

    void showErrors() const
    {
        const auto errors = m_currentScript.errors;
        for(const ScriptError& error : errors) {
            m_results->append(error.message);
        }
    }

    void restoreState()
    {
        QByteArray byteArray = m_settings->fileValue(QStringLiteral("Interface/ScriptSandboxState")).toByteArray();

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

        m_self->resize(dialogSize);
        m_mainSplitter->restoreState(mainSplitterState);
        m_documentSplitter->restoreState(documentSplitterState);
        m_editor->setPlainText(editorText);
        m_editor->moveCursor(QTextCursor::End);

        textChanged();
        m_expressionTree->expandAll();

        updateResults();
    }

    void saveState() const
    {
        QByteArray byteArray;
        QDataStream out(&byteArray, QIODevice::WriteOnly);

        out << m_self->size();
        out << m_mainSplitter->saveState();
        out << m_documentSplitter->saveState();
        out << m_editor->toPlainText();

        byteArray = qCompress(byteArray, 9);

        m_settings->fileSet(QStringLiteral("Interface/ScriptSandboxState"), byteArray);
    }
};

SandboxDialog::SandboxDialog(TrackSelectionController* trackSelection, SettingsManager* settings, QWidget* parent)
    : QDialog{parent}
    , p{std::make_unique<Private>(this, trackSelection, settings)}
{
    setWindowTitle(tr("Script Sandbox"));

    auto* mainLayout = new QGridLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    p->m_documentSplitter->addWidget(p->m_editor);
    p->m_documentSplitter->addWidget(p->m_results);

    p->m_mainSplitter->addWidget(p->m_documentSplitter);
    p->m_mainSplitter->addWidget(p->m_expressionTree);

    p->m_documentSplitter->setStretchFactor(0, 3);
    p->m_documentSplitter->setStretchFactor(1, 1);

    p->m_mainSplitter->setStretchFactor(0, 4);
    p->m_mainSplitter->setStretchFactor(1, 2);

    mainLayout->addWidget(p->m_mainSplitter);

    p->restoreState();

    QObject::connect(p->m_editor, &QPlainTextEdit::textChanged, this, [this]() { p->textChanged(); });
    QObject::connect(p->m_textChangeTimer, &QTimer::timeout, this, [this]() { p->showErrors(); });
    QObject::connect(&p->m_model, &QAbstractItemModel::modelReset, p->m_expressionTree, &QTreeView::expandAll);
    QObject::connect(p->m_expressionTree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this]() { p->selectionChanged(); });
    QObject::connect(p->m_trackSelection, &TrackSelectionController::selectionChanged, this,
                     [this]() { p->updateResults(); });
}

SandboxDialog::~SandboxDialog()
{
    p->m_editor->disconnect();

    p->m_textChangeTimer->disconnect();
    p->m_textChangeTimer->stop();
    p->m_textChangeTimer->deleteLater();

    p->saveState();
}
} // namespace Fooyin

#include "moc_sandboxdialog.cpp"
