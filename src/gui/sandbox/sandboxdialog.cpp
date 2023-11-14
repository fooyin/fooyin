/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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
#include <QSettings>
#include <QSplitter>
#include <QTextEdit>
#include <QTimer>
#include <QTreeView>

#include <chrono>

using namespace std::chrono_literals;

constexpr auto SandboxState = "Interface/ScriptSandboxState";

namespace Fy::Gui::Sandbox {
struct SandboxDialog::Private
{
    SandboxDialog* self;

    TrackSelectionController* trackSelection;
    Utils::SettingsManager* settings;

    QSplitter* mainSplitter;
    QSplitter* documentSplitter;

    QPlainTextEdit* editor;
    QTextEdit* results;
    ScriptHighlighter highlighter;

    QTreeView* expressionTree;
    ExpressionTreeModel model;

    QTimer* textChangeTimer;

    Core::Scripting::Registry registry;
    Core::Scripting::Parser parser;

    Core::Scripting::ParsedScript currentScript;

    explicit Private(SandboxDialog* self, TrackSelectionController* trackSelection, Utils::SettingsManager* settings)
        : self{self}
        , trackSelection{trackSelection}
        , settings{settings}
        , mainSplitter{new QSplitter(Qt::Horizontal, self)}
        , documentSplitter{new QSplitter(Qt::Vertical, self)}
        , editor{new QPlainTextEdit(self)}
        , results{new QTextEdit(self)}
        , highlighter{editor->document()}
        , expressionTree{new QTreeView(self)}
        , textChangeTimer{new QTimer(self)}
        , parser{&registry}
    {
        expressionTree->setModel(&model);
        expressionTree->setHeaderHidden(true);
        expressionTree->setSelectionMode(QAbstractItemView::SingleSelection);

        textChangeTimer->setSingleShot(true);
    }

    void updateResults()
    {
        if(model.rowCount({}) > 0) {
            if(const auto* item = static_cast<ExpressionTreeItem*>(model.index(0, 0, {}).internalPointer())) {
                updateResults(item->expression());
            }
        }
    }

    void updateResults(const Core::Scripting::Expression& expression)
    {
        if(!trackSelection->hasTracks()) {
            return;
        }

        const auto track  = trackSelection->selectedTracks().front();
        const auto result = parser.evaluate(expression, track);

        results->setText(result);
    }

    void selectionChanged()
    {
        const auto indexes = expressionTree->selectionModel()->selectedIndexes();
        if(indexes.empty()) {
            return;
        }

        const auto* item = static_cast<ExpressionTreeItem*>(indexes.front().internalPointer());

        updateResults(item->expression());
    }

    void textChanged()
    {
        textChangeTimer->start(1500ms);
        results->clear();

        currentScript = parser.parse(editor->toPlainText());

        model.populate(currentScript.expressions);
        updateResults();
    }

    void showErrors() const
    {
        const auto errors = currentScript.errors;
        for(const Core::Scripting::Error& error : errors) {
            results->append(error.message);
        }
    }

    void restoreState()
    {
        QByteArray byteArray = settings->settingsFile()->value(SandboxState).toByteArray();

        if(byteArray.isEmpty()) {
            return;
        }

        byteArray = qUncompress(byteArray);

        QDataStream in(&byteArray, QIODevice::ReadOnly);

        QByteArray dialogGeometry;
        QByteArray mainSplitterState;
        QByteArray documentSplitterState;
        QString editorText;

        in >> dialogGeometry;
        in >> mainSplitterState;
        in >> documentSplitterState;
        in >> editorText;

        self->restoreGeometry(dialogGeometry);
        mainSplitter->restoreState(mainSplitterState);
        documentSplitter->restoreState(documentSplitterState);
        editor->setPlainText(editorText);
        editor->moveCursor(QTextCursor::End);

        textChanged();
        expressionTree->expandAll();

        updateResults();
    }

    void saveState() const
    {
        QByteArray byteArray;
        QDataStream out(&byteArray, QIODevice::WriteOnly);

        out << self->saveGeometry();
        out << mainSplitter->saveState();
        out << documentSplitter->saveState();
        out << editor->toPlainText();

        byteArray = qCompress(byteArray, 9);

        settings->settingsFile()->setValue(SandboxState, byteArray);
    }
};

SandboxDialog::SandboxDialog(TrackSelectionController* trackSelection, Utils::SettingsManager* settings,
                             QWidget* parent)
    : QDialog{parent}
    , p{std::make_unique<Private>(this, trackSelection, settings)}
{
    setWindowTitle(tr("Script Sandbox"));

    auto* mainLayout = new QGridLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    p->documentSplitter->addWidget(p->editor);
    p->documentSplitter->addWidget(p->results);

    p->mainSplitter->addWidget(p->documentSplitter);
    p->mainSplitter->addWidget(p->expressionTree);

    p->documentSplitter->setStretchFactor(0, 3);
    p->documentSplitter->setStretchFactor(1, 1);

    p->mainSplitter->setStretchFactor(0, 4);
    p->mainSplitter->setStretchFactor(1, 2);

    mainLayout->addWidget(p->mainSplitter);

    p->restoreState();

    QObject::connect(p->editor, &QPlainTextEdit::textChanged, this, [this]() { p->textChanged(); });
    QObject::connect(p->textChangeTimer, &QTimer::timeout, this, [this]() { p->showErrors(); });
    QObject::connect(&p->model, &QAbstractItemModel::modelReset, p->expressionTree, &QTreeView::expandAll);
    QObject::connect(p->expressionTree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this]() { p->selectionChanged(); });
    QObject::connect(p->trackSelection, &TrackSelectionController::selectionChanged, this,
                     [this]() { p->updateResults(); });
}

SandboxDialog::~SandboxDialog()
{
    p->editor->disconnect();

    p->textChangeTimer->disconnect();
    p->textChangeTimer->stop();
    p->textChangeTimer->deleteLater();

    p->saveState();
}
} // namespace Fy::Gui::Sandbox

#include "moc_sandboxdialog.cpp"
