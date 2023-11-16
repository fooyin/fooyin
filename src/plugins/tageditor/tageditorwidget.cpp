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

#include "tageditorwidget.h"

#include "tageditormodel.h"

#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/actions/widgetcontext.h>
#include <utils/settings/settingsmanager.h>

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIODevice>
#include <QMenu>
#include <QSettings>
#include <QTableView>

constexpr auto TagEditorState = "TagEditor/State";

namespace {
void resizeTable(QTableView* view)
{
    view->resizeRowsToContents();
};

void handleNewRow(QTableView* view)
{
    resizeTable(view);
    const QModelIndex index = view->model()->index(view->model()->rowCount({}) - 1, 0);
    if(index.isValid()) {
        view->edit(index);
    }
};
} // namespace

namespace Fooyin::TagEditor {
TagEditorView::TagEditorView(QWidget* parent)
    : ExtendableTableView{parent}
{ }

int TagEditorView::sizeHintForRow(int row) const
{
    if(!model()->hasIndex(row, 0, {})) {
        return 0;
    }

    // Only resize rows based on first column
    return model()->index(row, 0, {}).data(Qt::SizeHintRole).toSize().height();
}

struct TagEditorWidget::Private
{
    TagEditorWidget* self;

    ActionManager* actionManager;
    TrackSelectionController* trackSelection;
    SettingsManager* settings;

    WidgetContext* context;

    QAction* remove;
    Command* removeCommand;

    TagEditorView* view;
    TagEditorModel* model;

    Private(TagEditorWidget* self, ActionManager* actionManager, TrackSelectionController* trackSelection,
            SettingsManager* settings)
        : self{self}
        , actionManager{actionManager}
        , trackSelection{trackSelection}
        , settings{settings}
        , context{new WidgetContext(self, Context{"Context.TagEditor"}, self)}
        , remove{new QAction(tr("Remove"), self)}
        , removeCommand{actionManager->registerAction(remove, Constants::Actions::Remove, context->context())}
        , view{new TagEditorView(self)}
        , model{new TagEditorModel(settings, self)}
    {
        auto* layout = new QHBoxLayout(self);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(view);

        view->setModel(model);

        view->horizontalHeader()->setStretchLastSection(true);
        view->horizontalHeader()->setSectionsClickable(false);
        view->verticalHeader()->setVisible(false);
        view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

        removeCommand->setDefaultShortcut(QKeySequence::Delete);
        actionManager->addContextObject(context);
    }

    void saveState() const
    {
        const QByteArray state = view->horizontalHeader()->saveState();
        settings->settingsFile()->setValue(TagEditorState, state);
    }

    void restoreState() const
    {
        const QByteArray state = settings->settingsFile()->value(TagEditorState).toByteArray();

        if(state.isEmpty()) {
            return;
        }

        view->horizontalHeader()->restoreState(state);
    };
};

TagEditorWidget::TagEditorWidget(ActionManager* actionManager, TrackSelectionController* trackSelection,
                                 SettingsManager* settings, QWidget* parent)
    : PropertiesTabWidget{parent}
    , p{std::make_unique<Private>(this, actionManager, trackSelection, settings)}
{
    setObjectName(TagEditorWidget::name());
    setWindowFlags(Qt::CustomizeWindowHint | Qt::Dialog);
    resize(600, 720);
    setMinimumSize(300, 400);

    const QFontMetrics fontMetrics{font()};
    const int width = fontMetrics.horizontalAdvance(p->model->defaultFieldText()) + 15;

    p->view->setColumnWidth(0, width);
    resizeTable(p->view);

    QObject::connect(p->trackSelection, &TrackSelectionController::selectionChanged, this,
                     [this](const TrackList& tracks) { p->model->reset(tracks); });
    QObject::connect(p->view, &ExtendableTableView::newRowClicked, p->model, &TagEditorModel::addNewRow);
    QObject::connect(p->model, &QAbstractItemModel::modelReset, this, [this]() { resizeTable(p->view); });
    QObject::connect(p->model, &TagEditorModel::trackMetadataChanged, this, &TagEditorWidget::trackMetadataChanged);
    QObject::connect(p->model, &TagEditorModel::newPendingRow, this, [this]() { handleNewRow(p->view); });
    QObject::connect(p->model, &TagEditorModel::pendingRowAdded, p->view, &ExtendableTableView::rowAdded);
    QObject::connect(
        p->model, &TagEditorModel::pendingRowCancelled, this,
        [this]() {
            // We can't remove the row in setData, so we use a queued signal to remove it once setData returns
            p->model->removePendingRow();
            p->view->rowAdded();
        },
        Qt::QueuedConnection);

    QObject::connect(p->view->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        const QModelIndexList selected = p->view->selectionModel()->selectedIndexes();
        const bool canRemove           = std::ranges::any_of(std::as_const(selected), [](const QModelIndex& index) {
            return !index.data(TagEditorItem::IsDefault).toBool();
        });
        p->remove->setEnabled(canRemove);
    });
    QObject::connect(p->remove, &QAction::triggered, this, [this]() {
        const QModelIndexList selected = p->view->selectionModel()->selectedIndexes();
        for(const QModelIndex& index : selected) {
            p->model->removeRow(index.row());
        }
    });

    if(trackSelection->hasTracks()) {
        p->model->reset(trackSelection->selectedTracks());
    }

    p->restoreState();
}

TagEditorWidget::~TagEditorWidget()
{
    p->saveState();
}

QString TagEditorWidget::name() const
{
    return QStringLiteral("Tag Editor");
}

QString TagEditorWidget::layoutName() const
{
    return QStringLiteral("TagEditor");
}

void TagEditorWidget::apply()
{
    p->model->processQueue();
}

void TagEditorWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(p->removeCommand->action());

    menu->popup(event->globalPos());
}
} // namespace Fooyin::TagEditor

#include "moc_tageditorwidget.cpp"
