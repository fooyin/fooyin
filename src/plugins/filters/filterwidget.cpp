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

#include "filterwidget.h"

#include "filtercolumnregistry.h"
#include "filterdelegate.h"
#include "filterfwd.h"
#include "filteritem.h"
#include "filtermodel.h"
#include "filterview.h"
#include "settings/filtersettings.h"

#include <core/library/trackfilter.h>
#include <core/library/tracksort.h>
#include <core/track.h>
#include <utils/actions/widgetcontext.h>
#include <utils/async.h>
#include <utils/settings/settingsmanager.h>
#include <utils/tooltipfilter.h>
#include <utils/widgets/autoheaderview.h>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QMenu>

#include <set>

namespace {
Fooyin::TrackList fetchAllTracks(QTreeView* view)
{
    std::set<int> ids;
    Fooyin::TrackList tracks;

    const QModelIndex parent = view->model()->index(0, 0, {});
    const int rowCount       = view->model()->rowCount(parent);

    for(int row = 0; row < rowCount; ++row) {
        const QModelIndex index = view->model()->index(row, 0, parent);
        const auto indexTracks  = index.data(Fooyin::Filters::FilterItem::Tracks).value<Fooyin::TrackList>();

        for(const Fooyin::Track& track : indexTracks) {
            const int id = track.id();
            if(!ids.contains(id)) {
                ids.emplace(id);
                tracks.push_back(track);
            }
        }
    }
    return tracks;
}
} // namespace

namespace Fooyin::Filters {
struct FilterWidget::Private
{
    FilterWidget* self;

    FilterColumnRegistry columnRegistry;
    SettingsManager* settings;

    FilterView* view;
    AutoHeaderView* header;
    FilterModel* model;

    Id group;
    int index{-1};
    FilterColumnList columns;
    bool multipleColumns{false};
    TrackList tracks;
    TrackList filteredTracks;

    WidgetContext* widgetContext;

    QString searchStr;
    bool searching{false};
    bool updating{false};

    QByteArray headerState;

    Private(FilterWidget* self_, SettingsManager* settings_)
        : self{self_}
        , columnRegistry{settings_}
        , settings{settings_}
        , view{new FilterView(self)}
        , header{new AutoHeaderView(Qt::Horizontal, self)}
        , model{new FilterModel(self)}
        , widgetContext{new WidgetContext(self, Context{Id{"Fooyin.Context.FilterWidget."}.append(self->id())}, self)}
    {
        view->setHeader(header);
        view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        header->setStretchEnabled(true);
        header->setSortIndicatorShown(true);
        header->setSectionsMovable(true);
        header->setFirstSectionMovable(true);
        header->setSectionsClickable(true);
        header->setContextMenuPolicy(Qt::CustomContextMenu);

        columns = {columnRegistry.itemByIndex(0)};

        model->setFont(settings->value<Settings::Filters::FilterFont>());
        model->setColour(settings->value<Settings::Filters::FilterColour>());
        model->setRowHeight(settings->value<Settings::Filters::FilterRowHeight>());
    }

    [[nodiscard]] QString playlistNameFromSelection() const
    {
        QStringList titles;
        const QModelIndexList selectedIndexes = view->selectionModel()->selectedIndexes();
        std::ranges::transform(selectedIndexes, std::back_inserter(titles),
                               [](const QModelIndex& selectedIndex) { return selectedIndex.data().toString(); });
        return titles.join(QStringLiteral(", "));
    }

    void refreshFilteredTracks()
    {
        filteredTracks.clear();

        const QModelIndexList selected = view->selectionModel()->selectedRows();

        if(selected.empty()) {
            return;
        }

        TrackList selectedTracks;

        for(const auto& selectedIndex : selected) {
            if(!selectedIndex.parent().isValid()) {
                selectedTracks = fetchAllTracks(view);
                break;
            }
            const auto newTracks = selectedIndex.data(FilterItem::Tracks).value<TrackList>();
            std::ranges::copy(newTracks, std::back_inserter(selectedTracks));
        }

        filteredTracks = selectedTracks;
    }

    void selectionChanged()
    {
        if(searching || updating) {
            return;
        }

        refreshFilteredTracks();

        emit self->selectionChanged(playlistNameFromSelection());
    }

    void hideHeader(bool hide) const
    {
        header->setFixedHeight(hide ? 0 : QWIDGETSIZE_MAX);
        header->adjustSize();
    }

    void filterHeaderMenu(const QPoint& pos)
    {
        auto* menu = new QMenu(self);
        menu->setAttribute(Qt::WA_DeleteOnClose);

        auto* filterList = new QActionGroup{menu};
        filterList->setExclusionPolicy(QActionGroup::ExclusionPolicy::None);

        for(const auto& column : columnRegistry.items()) {
            auto* columnAction = new QAction(column.name, menu);
            columnAction->setData(column.id);
            columnAction->setCheckable(true);
            columnAction->setChecked(hasColumn(column.id));
            columnAction->setEnabled(!hasColumn(column.id) || columns.size() > 1);
            menu->addAction(columnAction);
            filterList->addAction(columnAction);
        }

        menu->setDefaultAction(filterList->checkedAction());
        QObject::connect(filterList, &QActionGroup::triggered, self, [this](QAction* action) {
            const int columnId = action->data().toInt();
            if(action->isChecked()) {
                const FilterColumn column = columnRegistry.itemById(action->data().toInt());
                if(column.isValid()) {
                    if(multipleColumns) {
                        columns.push_back(column);
                    }
                    else {
                        columns = {column};
                    }
                }
            }
            else {
                auto colIt
                    = std::ranges::find_if(columns, [columnId](const FilterColumn& col) { return col.id == columnId; });
                if(colIt != columns.end()) {
                    const int removedIndex = static_cast<int>(std::distance(columns.begin(), colIt));
                    if(model->removeColumn(removedIndex)) {
                        columns.erase(colIt);
                    }
                }
            }

            tracks.clear();
            QMetaObject::invokeMethod(self, &FilterWidget::filterUpdated);
        });

        menu->addSeparator();
        auto* multiColAction = new QAction(tr("Multiple Columns"), menu);
        multiColAction->setCheckable(true);
        multiColAction->setChecked(multipleColumns);
        multiColAction->setEnabled(!(columns.size() > 1));
        QObject::connect(multiColAction, &QAction::triggered, self,
                         [this](bool checked) { multipleColumns = checked; });
        menu->addAction(multiColAction);

        menu->addSeparator();
        header->addHeaderContextMenu(menu, self->mapToGlobal(pos));
        menu->addSeparator();
        header->addHeaderAlignmentMenu(menu, self->mapToGlobal(pos));

        menu->addSeparator();
        auto* manageConnections = new QAction(tr("Manage Groups"), menu);
        QObject::connect(manageConnections, &QAction::triggered, self,
                         [this]() { QMetaObject::invokeMethod(self, &FilterWidget::requestEditConnections); });
        menu->addAction(manageConnections);

        menu->popup(self->mapToGlobal(pos));
    }

    bool hasColumn(int id) const
    {
        return std::ranges::any_of(columns, [id](const FilterColumn& column) { return column.id == id; });
    }

    void columnChanged(const Filters::FilterColumn& column)
    {
        if(hasColumn(column.id)) {
            std::ranges::replace_if(
                columns, [&column](const FilterColumn& filterCol) { return filterCol.id == column.id; }, column);

            QMetaObject::invokeMethod(self, &FilterWidget::filterUpdated);
        }
    }
};

FilterWidget::FilterWidget(SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, settings)}
{
    setObjectName(FilterWidget::name());

    setFeature(FyWidget::Search);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    p->view->setModel(p->model);
    p->view->setItemDelegate(new FilterDelegate(this));
    p->view->viewport()->installEventFilter(new ToolTipFilter(this));

    layout->addWidget(p->view);

    p->hideHeader(!p->settings->value<Settings::Filters::FilterHeader>());
    setScrollbarEnabled(p->settings->value<Settings::Filters::FilterScrollBar>());
    p->view->setAlternatingRowColors(p->settings->value<Settings::Filters::FilterAltColours>());

    QObject::connect(&p->columnRegistry, &FilterColumnRegistry::columnChanged, this,
                     [this](const Filters::FilterColumn& column) { p->columnChanged(column); });

    QObject::connect(p->model, &QAbstractItemModel::modelReset, this, [this]() { p->view->expandAll(); });
    QObject::connect(p->header, &QHeaderView::sortIndicatorChanged, p->model, &FilterModel::sortOnColumn);
    QObject::connect(p->header, &FilterView::customContextMenuRequested, this,
                     [this](const QPoint& pos) { p->filterHeaderMenu(pos); });
    QObject::connect(p->view->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this]() { p->selectionChanged(); });

    QObject::connect(p->view, &FilterView::doubleClicked, this,
                     [this]() { emit doubleClicked(p->playlistNameFromSelection()); });
    QObject::connect(p->view, &FilterView::middleClicked, this,
                     [this]() { emit middleClicked(p->playlistNameFromSelection()); });

    p->settings->subscribe<Settings::Filters::FilterAltColours>(p->view, &QAbstractItemView::setAlternatingRowColors);
    p->settings->subscribe<Settings::Filters::FilterHeader>(this, [this](bool enabled) { p->hideHeader(!enabled); });
    p->settings->subscribe<Settings::Filters::FilterScrollBar>(this, &FilterWidget::setScrollbarEnabled);
    p->settings->subscribe<Settings::Filters::FilterFont>(this,
                                                          [this](const QString& font) { p->model->setFont(font); });
    p->settings->subscribe<Settings::Filters::FilterColour>(
        this, [this](const QString& colour) { p->model->setColour(colour); });
    p->settings->subscribe<Settings::Filters::FilterRowHeight>(this, [this](const int height) {
        p->model->setRowHeight(height);
        QMetaObject::invokeMethod(p->view->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));
    });
}

FilterWidget::~FilterWidget()
{
    emit filterDeleted();
}

Id FilterWidget::group() const
{
    return p->group;
}

int FilterWidget::index() const
{
    return p->index;
}

bool FilterWidget::multipleColumns() const
{
    return p->multipleColumns;
}

bool FilterWidget::isActive() const
{
    return !p->filteredTracks.empty();
}

TrackList FilterWidget::tracks() const
{
    return p->tracks;
}

TrackList FilterWidget::filteredTracks() const
{
    return p->filteredTracks;
}

QString FilterWidget::searchFilter() const
{
    return p->searchStr;
}

WidgetContext* FilterWidget::widgetContext() const
{
    return p->widgetContext;
}

void FilterWidget::setGroup(const Id& group)
{
    p->group = group;
}

void FilterWidget::setIndex(int index)
{
    p->index = index;
}

void FilterWidget::refetchFilteredTracks()
{
    p->refreshFilteredTracks();
}

void FilterWidget::setFilteredTracks(const TrackList& tracks)
{
    p->filteredTracks = tracks;
}

void FilterWidget::clearFilteredTracks()
{
    p->filteredTracks.clear();
}

void FilterWidget::reset(const TrackList& tracks)
{
    p->tracks = tracks;
    p->model->reset(p->columns, tracks);
}

void FilterWidget::softReset(const TrackList& tracks)
{
    QStringList selected;
    const QModelIndexList selectedRows = p->view->selectionModel()->selectedRows();
    for(const QModelIndex& index : selectedRows) {
        selected.emplace_back(index.data(Qt::DisplayRole).toString());
    }

    reset(tracks);

    QObject::connect(
        p->model, &FilterModel::modelUpdated, this,
        [this, selected]() {
            const QModelIndexList selectedIndexes = p->model->indexesForValues(selected);

            QItemSelection indexesToSelect;
            indexesToSelect.reserve(selectedIndexes.size());

            const int columnCount = static_cast<int>(p->columns.size());

            for(const QModelIndex& index : selectedIndexes) {
                if(index.isValid()) {
                    const QModelIndex last = index.siblingAtColumn(columnCount - 1);
                    indexesToSelect.append({index, last.isValid() ? last : index});
                }
            }

            p->view->selectionModel()->select(indexesToSelect, QItemSelectionModel::ClearAndSelect);
            p->updating = false;

            if(indexesToSelect.empty()) {
                p->selectionChanged();
            }
        },
        Qt::SingleShotConnection);
}

void FilterWidget::setScrollbarEnabled(bool enabled)
{
    p->view->setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
}

QString FilterWidget::name() const
{
    return QStringLiteral("Library Filter");
}

QString FilterWidget::layoutName() const
{
    return QStringLiteral("LibraryFilter");
}

void FilterWidget::saveLayoutData(QJsonObject& layout)
{
    QStringList columns;

    for(int i{0}; const auto& column : p->columns) {
        const auto alignment = p->model->columnAlignment(i++);
        QString colStr       = QString::number(column.id);

        if(alignment != Qt::AlignLeft) {
            colStr += QStringLiteral(":") + QString::number(alignment.toInt());
        }

        columns.push_back(colStr);
    }

    layout[QStringLiteral("Columns")] = columns.join(QStringLiteral("|"));

    QByteArray state = p->header->saveHeaderState();
    state            = qCompress(state, 9);

    layout[QStringLiteral("Group")] = p->group.name();
    layout[QStringLiteral("Index")] = p->index;
    layout[QStringLiteral("State")] = QString::fromUtf8(state.toBase64());
}

void FilterWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(QStringLiteral("Columns"))) {
        p->columns.clear();

        const QString columnData    = layout.value(QStringLiteral("Columns")).toString();
        const QStringList columnIds = columnData.split(QStringLiteral("|"));

        for(int i{0}; const auto& columnId : columnIds) {
            const auto column     = columnId.split(QStringLiteral(":"));
            const auto columnItem = p->columnRegistry.itemById(column.at(0).toInt());

            if(columnItem.isValid()) {
                p->columns.push_back(columnItem);

                if(column.size() > 1) {
                    p->model->changeColumnAlignment(i, static_cast<Qt::Alignment>(column.at(1).toInt()));
                }
                else {
                    p->model->changeColumnAlignment(i, Qt::AlignLeft);
                }
            }
            ++i;
        }
    }

    if(layout.contains(QStringLiteral("Group"))) {
        p->group = Id{layout.value(QStringLiteral("Group")).toString()};
    }

    if(layout.contains(QStringLiteral("Index"))) {
        p->index = layout.value(QStringLiteral("Index")).toInt();
    }

    emit filterUpdated();

    if(layout.contains(QStringLiteral("State"))) {
        auto state = QByteArray::fromBase64(layout.value(QStringLiteral("State")).toString().toUtf8());

        if(state.isEmpty()) {
            return;
        }

        p->headerState = qUncompress(state);
    }
}

void FilterWidget::finalise()
{
    p->multipleColumns = p->columns.size() > 1;

    if(!p->columns.empty()) {
        if(!p->headerState.isEmpty()) {
            QObject::connect(
                p->model, &QAbstractItemModel::modelReset, this,
                [this]() {
                    p->header->restoreHeaderState(p->headerState);
                    p->model->sortOnColumn(p->header->sortIndicatorSection(), p->header->sortIndicatorOrder());
                },
                Qt::SingleShotConnection);
        }
    }
}

void FilterWidget::searchEvent(const QString& search)
{
    p->filteredTracks.clear();
    emit requestSearch(search);
    p->searchStr = search;
}

void FilterWidget::tracksAdded(const TrackList& tracks)
{
    p->model->addTracks(tracks);
}

void FilterWidget::tracksUpdated(const TrackList& tracks)
{
    if(tracks.empty()) {
        emit finishedUpdating();
        return;
    }

    p->updating = true;

    const QModelIndexList selectedRows = p->view->selectionModel()->selectedRows();

    QStringList selected;
    for(const QModelIndex& index : selectedRows) {
        if(!index.parent().isValid()) {
            selected.emplace_back(QStringLiteral(""));
        }
        else {
            selected.emplace_back(index.data(Qt::DisplayRole).toString());
        }
    }

    p->model->updateTracks(tracks);

    QObject::connect(
        p->model, &FilterModel::modelUpdated, this,
        [this, selected]() {
            const QModelIndexList selectedIndexes = p->model->indexesForValues(selected);

            QItemSelection indexesToSelect;
            indexesToSelect.reserve(selectedIndexes.size());

            const int columnCount = static_cast<int>(p->columns.size());

            for(const QModelIndex& index : selectedIndexes) {
                if(index.isValid()) {
                    const QModelIndex last = index.siblingAtColumn(columnCount - 1);
                    indexesToSelect.append({index, last.isValid() ? last : index});
                }
            }

            p->view->selectionModel()->select(indexesToSelect, QItemSelectionModel::ClearAndSelect);
            p->updating = false;
            emit finishedUpdating();
        },
        Qt::SingleShotConnection);
}

void FilterWidget::tracksRemoved(const TrackList& tracks)
{
    p->model->removeTracks(tracks);
}

void FilterWidget::contextMenuEvent(QContextMenuEvent* event)
{
    emit requestContextMenu(mapToGlobal(event->pos()));
}
} // namespace Fooyin::Filters

#include "moc_filterwidget.cpp"
