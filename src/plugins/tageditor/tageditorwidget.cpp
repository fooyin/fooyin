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

#include "tageditoritem.h"
#include "tageditormodel.h"

#include <gui/trackselectioncontroller.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/widgetcontext.h>
#include <utils/settings/settingsmanager.h>

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QSettings>
#include <QTableView>

constexpr auto TagEditorState = "TagEditor/State";

namespace Fooyin::TagEditor {
TagEditorView::TagEditorView(ActionManager* actionManager, QWidget* parent)
    : ExtendableTableView{actionManager, parent}
{ }

void TagEditorView::handleNewRow()
{
    ExtendableTableView::handleNewRow();
    resizeRowsToContents();
}

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

    TagEditorView* view;
    TagEditorModel* model;

    Private(TagEditorWidget* self, ActionManager* actionManager, TrackSelectionController* trackSelection,
            SettingsManager* settings)
        : self{self}
        , actionManager{actionManager}
        , trackSelection{trackSelection}
        , settings{settings}
        , context{new WidgetContext(self, Context{"Context.TagEditor"}, self)}
        , view{new TagEditorView(actionManager, self)}
        , model{new TagEditorModel(settings, self)}
    {
        auto* layout = new QHBoxLayout(self);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(view);

        view->setExtendableModel(model);

        view->horizontalHeader()->setStretchLastSection(true);
        view->horizontalHeader()->setSectionsClickable(false);
        view->verticalHeader()->setVisible(false);
        view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

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
    const int width = fontMetrics.horizontalAdvance(TagEditorModel::defaultFieldText()) + 15;

    p->view->setColumnWidth(0, width);
    p->view->resizeRowsToContents();

    QObject::connect(p->trackSelection, &TrackSelectionController::selectionChanged, this,
                     [this](const TrackList& tracks) { p->model->reset(tracks); });
    QObject::connect(p->model, &QAbstractItemModel::modelReset, this, [this]() { p->view->resizeRowsToContents(); });
    QObject::connect(p->model, &TagEditorModel::trackMetadataChanged, this, &TagEditorWidget::trackMetadataChanged);

    QObject::connect(p->view->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        const QModelIndexList selected = p->view->selectionModel()->selectedIndexes();
        const bool canRemove           = std::ranges::any_of(std::as_const(selected), [](const QModelIndex& index) {
            return !index.data(TagEditorItem::IsDefault).toBool();
        });
        p->view->removeAction()->setEnabled(canRemove);
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

void TagEditorWidget::contextMenuEvent(QContextMenuEvent* /*event*/) { }
} // namespace Fooyin::TagEditor

#include "moc_tageditorwidget.cpp"
