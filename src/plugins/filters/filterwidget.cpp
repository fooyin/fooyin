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

#include "filterwidget.h"

#include "filtercolumnregistry.h"
#include "filterdelegate.h"
#include "filterfwd.h"
#include "filteritem.h"
#include "filtermodel.h"
#include "filterview.h"
#include "settings/filtersettings.h"

#include <core/track.h>
#include <core/library/tracksort.h>
#include <utils/async.h>
#include <utils/enum.h>
#include <utils/settings/settingsmanager.h>
#include <utils/widgets/autoheaderview.h>

#include <QCoroCore>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QMenu>

#include <set>

using namespace Qt::Literals::StringLiterals;

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

    FilterColumnRegistry* columnRegistry;
    SettingsManager* settings;

    FilterView* view;
    AutoHeaderView* header;
    FilterModel* model;

    Id group;
    int index{-1};
    FilterColumnList columns;
    bool multipleColumns{false};
    TrackList tracks;

    Private(FilterWidget* self, FilterColumnRegistry* columnRegistry, SettingsManager* settings)
        : self{self}
        , columnRegistry{columnRegistry}
        , settings{settings}
        , view{new FilterView(self)}
        , header{new AutoHeaderView(Qt::Horizontal, self)}
        , model{new FilterModel(self)}
    {
        view->setHeader(header);
        view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        header->setStretchEnabled(true);
        header->setSortIndicatorShown(true);
        header->setSectionsMovable(true);
        header->setFirstSectionMovable(true);
        header->setSectionsClickable(true);
        header->setContextMenuPolicy(Qt::CustomContextMenu);

        columns = {columnRegistry->itemByIndex(0)};

        header->restoreHeaderState({});
    }

    [[nodiscard]] QString playlistNameFromSelection() const
    {
        QStringList titles;
        const QModelIndexList selectedIndexes = view->selectionModel()->selectedIndexes();
        std::ranges::transform(selectedIndexes, std::back_inserter(titles),
                               [](const QModelIndex& index) { return index.data().toString(); });
        return titles.join(", "_L1);
    }

    QCoro::Task<void> selectionChanged()
    {
        tracks.clear();

        const QModelIndexList indexes = view->selectionModel()->selectedIndexes();
        QModelIndexList selectedRows;
        for(const QModelIndex& index : indexes) {
            if(index.column() == 0) {
                selectedRows.push_back(index);
            }
        }

        if(selectedRows.empty()) {
            co_return;
        }

        for(const auto& index : selectedRows) {
            if(!index.parent().isValid()) {
                tracks = fetchAllTracks(view);
                break;
            }
            const auto newTracks = index.data(FilterItem::Tracks).value<TrackList>();
            std::ranges::copy(newTracks, std::back_inserter(tracks));
        }

        tracks = co_await Utils::asyncExec([this]() { return Sorting::sortTracks(tracks); });

        QMetaObject::invokeMethod(self, "selectionChanged", Q_ARG(QString, playlistNameFromSelection()));
    }

    void updateAppearance(const QVariant& optionsVar) const
    {
        const auto options = optionsVar.value<FilterOptions>();
        model->setAppearance(options);
        QMetaObject::invokeMethod(view->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));
    }

    void hideHeader(bool hide) const
    {
        if(hide) {
            header->setFixedHeight(0);
        }
        else if(header->sizeHint().height() > 0) {
            header->setFixedHeight(header->sizeHint().height());
        }
    }

    void filterHeaderMenu(const QPoint& pos)
    {
        auto* menu = new QMenu(self);
        menu->setAttribute(Qt::WA_DeleteOnClose);

        auto* filterList = new QActionGroup{menu};
        filterList->setExclusionPolicy(QActionGroup::ExclusionPolicy::None);

        for(const auto& [filterIndex, column] : columnRegistry->items()) {
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
                const FilterColumn column = columnRegistry->itemById(action->data().toInt());
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
                std::erase_if(columns, [columnId](const FilterColumn& filterCol) { return filterCol.id == columnId; });
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

FilterWidget::FilterWidget(FilterColumnRegistry* columnRegistry, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, columnRegistry, settings)}
{
    setObjectName(FilterWidget::name());

    setFeature(FyWidget::Search);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    p->view->setModel(p->model);
    p->view->setItemDelegate(new FilterDelegate(this));

    layout->addWidget(p->view);

    p->hideHeader(!p->settings->value<Settings::Filters::FilterHeader>());
    setScrollbarEnabled(p->settings->value<Settings::Filters::FilterScrollBar>());
    p->view->setAlternatingRowColors(p->settings->value<Settings::Filters::FilterAltColours>());

    p->updateAppearance(p->settings->value<Settings::Filters::FilterAppearance>());

    p->settings->subscribe<Settings::Filters::FilterAltColours>(p->view, &QAbstractItemView::setAlternatingRowColors);
    p->settings->subscribe<Settings::Filters::FilterHeader>(this, [this](bool enabled) { p->hideHeader(!enabled); });
    p->settings->subscribe<Settings::Filters::FilterScrollBar>(this, &FilterWidget::setScrollbarEnabled);
    p->settings->subscribe<Settings::Filters::FilterAppearance>(
        this, [this](const QVariant& appearance) { p->updateAppearance(appearance); });

    QObject::connect(p->columnRegistry, &FilterColumnRegistry::columnChanged, this,
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

TrackList FilterWidget::tracks() const
{
    return p->tracks;
}

void FilterWidget::setGroup(const Id& group)
{
    p->group = group;
}

void FilterWidget::setIndex(int index)
{
    p->index = index;
}

void FilterWidget::setTracks(const TrackList& tracks)
{
    p->tracks = tracks;
}

void FilterWidget::clearTracks()
{
    p->tracks.clear();
}

void FilterWidget::reset(const TrackList& tracks)
{
    p->model->reset(p->columns, tracks);
}

void FilterWidget::setScrollbarEnabled(bool enabled)
{
    p->view->setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
}

QString FilterWidget::name() const
{
    return u"Library Filter"_s;
}

QString FilterWidget::layoutName() const
{
    return u"LibraryFilter"_s;
}

void FilterWidget::saveLayoutData(QJsonObject& layout)
{
    QStringList columns;
    std::ranges::transform(p->columns, std::back_inserter(columns),
                           [](const auto& column) { return QString::number(column.id); });

    QByteArray state = p->header->saveHeaderState();
    state            = qCompress(state, 9);

    layout["Columns"_L1] = columns.join("|"_L1);
    layout["Group"_L1]   = p->group.name();
    layout["Index"_L1]   = p->index;
    layout["State"_L1]   = QString::fromUtf8(state.toBase64());
}

void FilterWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains("Columns"_L1)) {
        p->columns.clear();
        const QString columnNames = layout.value("Columns"_L1).toString();
        const QStringList columns = columnNames.split("|"_L1);
        std::ranges::transform(columns, std::back_inserter(p->columns),
                               [this](const QString& column) { return p->columnRegistry->itemById(column.toInt()); });
    }

    if(layout.contains("Group"_L1)) {
        p->group = Id{layout.value("Group"_L1).toString()};
    }

    if(layout.contains("Index"_L1)) {
        p->index = layout.value("Index"_L1).toInt();
    }

    emit filterUpdated();

    if(layout.contains("State"_L1)) {
        auto state = QByteArray::fromBase64(layout.value("State"_L1).toString().toUtf8());

        if(state.isEmpty()) {
            return;
        }

        state = qUncompress(state);

        // Workaround to ensure QHeaderView section count is updated before restoring state
        QMetaObject::invokeMethod(p->model, "headerDataChanged", Q_ARG(Qt::Orientation, Qt::Horizontal), Q_ARG(int, 0),
                                  Q_ARG(int, 0));

        p->header->restoreHeaderState(state);
    }
}

void FilterWidget::finalise()
{
    p->model->sortOnColumn(p->header->sortIndicatorSection(), p->header->sortIndicatorOrder());
}

void FilterWidget::searchEvent(const QString& search)
{
    emit requestSearch(search);
}

void FilterWidget::tracksAdded(const TrackList& tracks)
{
    p->model->addTracks(tracks);
}

void FilterWidget::tracksUpdated(const TrackList& tracks)
{
    p->model->updateTracks(tracks);
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
