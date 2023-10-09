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

#include <gui/guisettings.h>
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

namespace Fy::Gui::Sandbox {
using namespace std::chrono_literals;

struct SandboxDialog::Private : QObject
{
    SandboxDialog* self;

    TrackSelectionController* trackSelection;
    Utils::SettingsManager* settings;

    QSplitter* mainSplitter;
    QSplitter* documentSplitter;

    QPlainTextEdit* editor;
    QTextEdit* results;
    ScriptHighlighter highlighter;

    QTreeView* expressiontree;
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
        , expressiontree{new QTreeView(self)}
        , textChangeTimer{new QTimer(self)}
        , parser{&registry}
    {
        expressiontree->setModel(&model);
        expressiontree->setHeaderHidden(true);
        expressiontree->setSelectionMode(QAbstractItemView::SingleSelection);

        textChangeTimer->setSingleShot(true);

        QObject::connect(editor, &QPlainTextEdit::textChanged, this, &SandboxDialog::Private::textChanged);
        QObject::connect(textChangeTimer, &QTimer::timeout, this, &SandboxDialog::Private::showErrors);

        QObject::connect(&model, &QAbstractItemModel::modelReset, expressiontree, &QTreeView::expandAll);
        QObject::connect(expressiontree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                         &SandboxDialog::Private::selectionChanged);
        QObject::connect(trackSelection, &TrackSelectionController::selectionChanged, this,
                         &SandboxDialog::Private::selectionChanged);
    }

    void selectionChanged()
    {
        if(!trackSelection->hasTracks()) {
            return;
        }

        const auto indexes = expressiontree->selectionModel()->selectedIndexes();
        if(indexes.empty()) {
            return;
        }

        const auto track           = trackSelection->selectedTracks().front();
        const QModelIndex selected = indexes.front();
        auto* item                 = static_cast<ExpressionTreeItem*>(selected.internalPointer());

        const auto result = parser.evaluate(item->expression(), track);
        results->setText(result);
    }

    void textChanged()
    {
        textChangeTimer->start(1500ms);
        results->clear();

        currentScript = parser.parse(editor->toPlainText());

        model.populate(currentScript.expressions);
    }

    void showErrors()
    {
        const auto errors = currentScript.errors;
        for(const Core::Scripting::Error& error : errors) {
            results->append(error.message);
        }
    }

    void restoreState()
    {
        QByteArray byteArray = settings->value<Settings::ScriptSandboxState>();

        if(!byteArray.isEmpty()) {
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
        }
    }

    void saveState()
    {
        QByteArray byteArray;
        QDataStream out(&byteArray, QIODevice::WriteOnly);

        out << self->saveGeometry();
        out << mainSplitter->saveState();
        out << documentSplitter->saveState();
        out << editor->toPlainText();

        byteArray = qCompress(byteArray, 9);

        settings->set<Settings::ScriptSandboxState>(byteArray);
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
    p->mainSplitter->addWidget(p->expressiontree);

    p->documentSplitter->setStretchFactor(0, 3);
    p->documentSplitter->setStretchFactor(1, 1);

    p->mainSplitter->setStretchFactor(0, 4);
    p->mainSplitter->setStretchFactor(1, 2);

    mainLayout->addWidget(p->mainSplitter);

    p->restoreState();
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
