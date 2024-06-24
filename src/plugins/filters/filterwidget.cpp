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
#include <utils/enum.h>
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
Fooyin::TrackList fetchAllTracks(QAbstractItemView* view)
{
    std::set<int> ids;
    Fooyin::TrackList tracks;

    const QModelIndex parent;
    const int rowCount = view->model()->rowCount(parent);

    for(int row{0}; row < rowCount; ++row) {
        const QModelIndex index = view->model()->index(row, 0, parent);
        if(!index.data(Fooyin::Filters::FilterItem::IsSummary).toBool()) {
            const auto indexTracks = index.data(Fooyin::Filters::FilterItem::Tracks).value<Fooyin::TrackList>();

            for(const Fooyin::Track& track : indexTracks) {
                const int id = track.id();
                if(!ids.contains(id)) {
                    ids.emplace(id);
                    tracks.push_back(track);
                }
            }
        }
    }

    return tracks;
}
} // namespace

namespace Fooyin::Filters {
struct FilterWidget::Private
{
    FilterWidget* m_self;

    FilterColumnRegistry m_columnRegistry;
    SettingsManager* m_settings;

    FilterView* m_view;
    AutoHeaderView* m_header;
    FilterModel* m_model;

    Id m_group;
    int m_index{-1};
    FilterColumnList m_columns;
    bool m_multipleColumns{false};
    TrackList m_tracks;
    TrackList m_filteredTracks;

    WidgetContext* m_widgetContext;

    QString m_searchStr;
    bool m_searching{false};
    bool m_updating{false};

    QByteArray m_headerState;

    Private(FilterWidget* self, SettingsManager* settings)
        : m_self{self}
        , m_columnRegistry{settings}
        , m_settings{settings}
        , m_view{new FilterView(m_self)}
        , m_header{new AutoHeaderView(Qt::Horizontal, m_self)}
        , m_model{new FilterModel(m_settings, m_self)}
        , m_widgetContext{
              new WidgetContext(m_self, Context{Id{"Fooyin.Context.FilterWidget."}.append(m_self->id())}, m_self)}
    {
        m_view->setModel(m_model);
        m_view->setHeader(m_header);
        m_view->setUniformRowHeights(true);
        m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_view->setItemDelegate(new FilterDelegate(m_self));
        m_view->viewport()->installEventFilter(new ToolTipFilter(m_self));

        m_header->setStretchEnabled(true);
        m_header->setSortIndicatorShown(true);
        m_header->setSectionsMovable(true);
        m_header->setFirstSectionMovable(true);
        m_header->setSectionsClickable(true);
        m_header->setContextMenuPolicy(Qt::CustomContextMenu);

        if(const auto column = m_columnRegistry.itemByIndex(0)) {
            m_columns = {column.value()};
        }

        m_model->setFont(m_settings->value<Settings::Filters::FilterFont>());
        m_model->setColour(m_settings->value<Settings::Filters::FilterColour>());
        m_model->setRowHeight(m_settings->value<Settings::Filters::FilterRowHeight>());

        hideHeader(!m_settings->value<Settings::Filters::FilterHeader>());
        setScrollbarEnabled(m_settings->value<Settings::Filters::FilterScrollBar>());
        m_view->setAlternatingRowColors(m_settings->value<Settings::Filters::FilterAltColours>());
        m_view->setItemWidth(m_settings->value<Settings::Filters::FilterIconItemWidth>());
        m_view->setIconSize(m_settings->value<Settings::Filters::FilterIconSize>().toSize());

        setupConnections();

        m_settings->subscribe<Settings::Filters::FilterAltColours>(m_view, &QAbstractItemView::setAlternatingRowColors);
        m_settings->subscribe<Settings::Filters::FilterHeader>(m_self, [this](bool enabled) { hideHeader(!enabled); });
        m_settings->subscribe<Settings::Filters::FilterScrollBar>(
            m_self, [this](bool enabled) { setScrollbarEnabled(enabled); });
        m_settings->subscribe<Settings::Filters::FilterFont>(m_self,
                                                             [this](const QString& font) { m_model->setFont(font); });
        m_settings->subscribe<Settings::Filters::FilterColour>(
            m_self, [this](const QString& colour) { m_model->setColour(colour); });
        m_settings->subscribe<Settings::Filters::FilterRowHeight>(m_self, [this](const int height) {
            m_model->setRowHeight(height);
            QMetaObject::invokeMethod(m_view->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));
        });
        m_settings->subscribe<Settings::Filters::FilterIconItemWidth>(
            m_self, [this](const int width) { m_view->setItemWidth(width); });
        m_settings->subscribe<Settings::Filters::FilterIconSize>(
            m_self, [this](const auto& size) { m_view->setIconSize(size.toSize()); });
    }

    void setupConnections()
    {
        QObject::connect(&m_columnRegistry, &FilterColumnRegistry::columnChanged, m_self,
                         [this](const Filters::FilterColumn& column) { columnChanged(column); });

        QObject::connect(m_header, &QHeaderView::sortIndicatorChanged, m_model, &FilterModel::sortOnColumn);
        QObject::connect(m_header, &FilterView::customContextMenuRequested, m_self,
                         [this](const QPoint& pos) { filterHeaderMenu(pos); });
        QObject::connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, m_self,
                         [this]() { selectionChanged(); });
        QObject::connect(m_view, &ExpandedTreeView::viewModeChanged, m_self, [this](ExpandedTreeView::ViewMode mode) {
            m_model->setShowDecoration(mode == ExpandedTreeView::ViewMode::Icon);
        });
        QObject::connect(m_view, &FilterView::doubleClicked, m_self,
                         [this]() { emit m_self->doubleClicked(playlistNameFromSelection()); });
        QObject::connect(m_view, &FilterView::middleClicked, m_self,
                         [this]() { emit m_self->middleClicked(playlistNameFromSelection()); });
    }

    [[nodiscard]] QString playlistNameFromSelection() const
    {
        QStringList titles;
        const QModelIndexList selectedIndexes = m_view->selectionModel()->selectedIndexes();
        std::ranges::transform(selectedIndexes, std::back_inserter(titles),
                               [](const QModelIndex& selectedIndex) { return selectedIndex.data().toString(); });
        return titles.join(QStringLiteral(", "));
    }

    void refreshFilteredTracks()
    {
        m_filteredTracks.clear();

        const QModelIndexList selected = m_view->selectionModel()->selectedRows();

        if(selected.empty()) {
            return;
        }

        TrackList selectedTracks;

        for(const auto& selectedIndex : selected) {
            if(selectedIndex.data(FilterItem::IsSummary).toBool()) {
                selectedTracks = fetchAllTracks(m_view);
                break;
            }
            const auto newTracks = selectedIndex.data(FilterItem::Tracks).value<TrackList>();
            std::ranges::copy(newTracks, std::back_inserter(selectedTracks));
        }

        m_filteredTracks = selectedTracks;
    }

    void selectionChanged()
    {
        if(m_searching || m_updating) {
            return;
        }

        refreshFilteredTracks();

        emit m_self->selectionChanged(playlistNameFromSelection());
    }

    void hideHeader(bool hide) const
    {
        m_header->setFixedHeight(hide ? 0 : QWIDGETSIZE_MAX);
        m_header->adjustSize();
    }

    void setScrollbarEnabled(bool enabled) const
    {
        m_view->setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    }

    void addDisplayMenu(QMenu* menu)
    {
        auto* displayMenu  = new QMenu(tr("Display"), menu);
        auto* displayGroup = new QActionGroup(displayMenu);

        auto* displayList = new QAction(tr("Columns"), displayGroup);
        auto* displayIcon = new QAction(tr("Artwork"), displayGroup);

        displayList->setCheckable(true);
        displayIcon->setCheckable(true);

        const auto currentMode = m_view->viewMode();
        if(currentMode == ExpandedTreeView::ViewMode::Tree) {
            displayList->setChecked(true);
        }
        else {
            displayIcon->setChecked(true);
        }

        QObject::connect(displayList, &QAction::triggered, m_self,
                         [this]() { m_view->setViewMode(ExpandedTreeView::ViewMode::Tree); });
        QObject::connect(displayIcon, &QAction::triggered, m_self,
                         [this]() { m_view->setViewMode(ExpandedTreeView::ViewMode::Icon); });

        auto* displaySummary = new QAction(tr("Summary item"), displayMenu);
        displaySummary->setCheckable(true);
        displaySummary->setChecked(m_model->showSummary());
        QObject::connect(displaySummary, &QAction::triggered, m_self,
                         [this](bool checked) { m_model->setShowSummary(checked); });

        displayMenu->addAction(displayList);
        displayMenu->addAction(displayIcon);
        displayMenu->addSeparator();
        displayMenu->addAction(displaySummary);

        menu->addMenu(displayMenu);
    }

    void filterHeaderMenu(const QPoint& pos)
    {
        auto* menu = new QMenu(m_self);
        menu->setAttribute(Qt::WA_DeleteOnClose);

        auto* filterList = new QActionGroup{menu};
        filterList->setExclusionPolicy(QActionGroup::ExclusionPolicy::None);

        for(const auto& column : m_columnRegistry.items()) {
            auto* columnAction = new QAction(column.name, menu);
            columnAction->setData(column.id);
            columnAction->setCheckable(true);
            columnAction->setChecked(hasColumn(column.id));
            columnAction->setEnabled(!hasColumn(column.id) || m_columns.size() > 1);
            menu->addAction(columnAction);
            filterList->addAction(columnAction);
        }

        menu->setDefaultAction(filterList->checkedAction());
        QObject::connect(filterList, &QActionGroup::triggered, m_self, [this](QAction* action) {
            const int columnId = action->data().toInt();
            if(action->isChecked()) {
                if(const auto column = m_columnRegistry.itemById(action->data().toInt())) {
                    if(m_multipleColumns) {
                        m_columns.push_back(column.value());
                    }
                    else {
                        m_columns = {column.value()};
                    }
                }
            }
            else {
                auto colIt = std::ranges::find_if(m_columns,
                                                  [columnId](const FilterColumn& col) { return col.id == columnId; });
                if(colIt != m_columns.end()) {
                    const int removedIndex = static_cast<int>(std::distance(m_columns.begin(), colIt));
                    if(m_model->removeColumn(removedIndex)) {
                        m_columns.erase(colIt);
                    }
                }
            }

            m_tracks.clear();
            QMetaObject::invokeMethod(m_self, &FilterWidget::filterUpdated);
        });

        menu->addSeparator();
        auto* multiColAction = new QAction(tr("Multiple Columns"), menu);
        multiColAction->setCheckable(true);
        multiColAction->setChecked(m_multipleColumns);
        multiColAction->setEnabled(m_columns.size() <= 1);
        QObject::connect(multiColAction, &QAction::triggered, m_self,
                         [this](bool checked) { m_multipleColumns = checked; });
        menu->addAction(multiColAction);

        menu->addSeparator();
        m_header->addHeaderContextMenu(menu, m_self->mapToGlobal(pos));
        menu->addSeparator();
        m_header->addHeaderAlignmentMenu(menu, m_self->mapToGlobal(pos));

        addDisplayMenu(menu);

        menu->addSeparator();
        auto* manageConnections = new QAction(tr("Manage Groups"), menu);
        QObject::connect(manageConnections, &QAction::triggered, m_self,
                         [this]() { QMetaObject::invokeMethod(m_self, &FilterWidget::requestEditConnections); });
        menu->addAction(manageConnections);

        menu->popup(m_self->mapToGlobal(pos));
    }

    [[nodiscard]] bool hasColumn(int id) const
    {
        return std::ranges::any_of(m_columns, [id](const FilterColumn& column) { return column.id == id; });
    }

    void columnChanged(const Filters::FilterColumn& column)
    {
        if(hasColumn(column.id)) {
            std::ranges::replace_if(
                m_columns, [&column](const FilterColumn& filterCol) { return filterCol.id == column.id; }, column);

            QMetaObject::invokeMethod(m_self, &FilterWidget::filterUpdated);
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

    layout->addWidget(p->m_view);
}

FilterWidget::~FilterWidget()
{
    emit filterDeleted();
}

Id FilterWidget::group() const
{
    return p->m_group;
}

int FilterWidget::index() const
{
    return p->m_index;
}

bool FilterWidget::multipleColumns() const
{
    return p->m_multipleColumns;
}

bool FilterWidget::isActive() const
{
    return !p->m_filteredTracks.empty();
}

TrackList FilterWidget::tracks() const
{
    return p->m_tracks;
}

TrackList FilterWidget::filteredTracks() const
{
    return p->m_filteredTracks;
}

QString FilterWidget::searchFilter() const
{
    return p->m_searchStr;
}

WidgetContext* FilterWidget::widgetContext() const
{
    return p->m_widgetContext;
}

void FilterWidget::setGroup(const Id& group)
{
    p->m_group = group;
}

void FilterWidget::setIndex(int index)
{
    p->m_index = index;
}

void FilterWidget::refetchFilteredTracks()
{
    p->refreshFilteredTracks();
}

void FilterWidget::setFilteredTracks(const TrackList& tracks)
{
    p->m_filteredTracks = tracks;
}

void FilterWidget::clearFilteredTracks()
{
    p->m_filteredTracks.clear();
}

void FilterWidget::reset(const TrackList& tracks)
{
    p->m_tracks = tracks;
    p->m_model->reset(p->m_columns, tracks);
}

void FilterWidget::softReset(const TrackList& tracks)
{
    p->m_updating = true;

    QStringList selected;
    const QModelIndexList selectedRows = p->m_view->selectionModel()->selectedRows();
    for(const QModelIndex& index : selectedRows) {
        selected.emplace_back(index.data(Qt::DisplayRole).toString());
    }

    reset(tracks);

    QObject::connect(
        p->m_model, &FilterModel::modelUpdated, this,
        [this, selected]() {
            const QModelIndexList selectedIndexes = p->m_model->indexesForValues(selected);

            QItemSelection indexesToSelect;
            indexesToSelect.reserve(selectedIndexes.size());

            const int columnCount = static_cast<int>(p->m_columns.size());

            for(const QModelIndex& index : selectedIndexes) {
                if(index.isValid()) {
                    const QModelIndex last = index.siblingAtColumn(columnCount - 1);
                    indexesToSelect.append({index, last.isValid() ? last : index});
                }
            }

            p->m_view->selectionModel()->select(indexesToSelect, QItemSelectionModel::ClearAndSelect);
            p->m_updating = false;
        },
        Qt::SingleShotConnection);
}

QString FilterWidget::name() const
{
    return tr("Library Filter");
}

QString FilterWidget::layoutName() const
{
    return QStringLiteral("LibraryFilter");
}

void FilterWidget::saveLayoutData(QJsonObject& layout)
{
    QStringList columns;

    for(int i{0}; const auto& column : p->m_columns) {
        const auto alignment = p->m_model->columnAlignment(i++);
        QString colStr       = QString::number(column.id);

        if(alignment != Qt::AlignLeft) {
            colStr += QStringLiteral(":") + QString::number(alignment.toInt());
        }

        columns.push_back(colStr);
    }

    layout[u"Columns"]     = columns.join(QStringLiteral("|"));
    layout[u"Display"]     = static_cast<int>(p->m_view->viewMode());
    layout[u"ShowSummary"] = p->m_model->showSummary();

    QByteArray state = p->m_header->saveHeaderState();
    state            = qCompress(state, 9);

    layout[u"Group"] = p->m_group.name();
    layout[u"Index"] = p->m_index;
    layout[u"State"] = QString::fromUtf8(state.toBase64());
}

void FilterWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(u"Columns")) {
        p->m_columns.clear();

        const QString columnData    = layout.value(u"Columns").toString();
        const QStringList columnIds = columnData.split(QStringLiteral("|"));

        for(int i{0}; const auto& columnId : columnIds) {
            const auto column = columnId.split(QStringLiteral(":"));

            if(const auto columnItem = p->m_columnRegistry.itemById(column.at(0).toInt())) {
                p->m_columns.push_back(columnItem.value());

                if(column.size() > 1) {
                    p->m_model->changeColumnAlignment(i, static_cast<Qt::Alignment>(column.at(1).toInt()));
                }
                else {
                    p->m_model->changeColumnAlignment(i, Qt::AlignLeft);
                }
            }
            ++i;
        }
    }

    if(layout.contains(u"Display")) {
        p->m_view->setViewMode(static_cast<ExpandedTreeView::ViewMode>(layout.value(u"Display").toInt()));
    }

    if(layout.contains(u"ShowSummary")) {
        p->m_model->setShowSummary(layout.value(u"ShowSummary").toBool());
    }

    if(layout.contains(u"Group")) {
        p->m_group = Id{layout.value(u"Group").toString()};
    }

    if(layout.contains(u"Index")) {
        p->m_index = layout.value(u"Index").toInt();
    }

    emit filterUpdated();

    if(layout.contains(u"State")) {
        auto state = QByteArray::fromBase64(layout.value(u"State").toString().toUtf8());

        if(state.isEmpty()) {
            return;
        }

        p->m_headerState = qUncompress(state);
    }
}

void FilterWidget::finalise()
{
    p->m_multipleColumns = p->m_columns.size() > 1;

    if(!p->m_columns.empty()) {
        if(p->m_headerState.isEmpty()) {
            p->m_header->setSortIndicator(0, Qt::AscendingOrder);
        }
        else {
            QObject::connect(
                p->m_model, &QAbstractItemModel::modelReset, this,
                [this]() {
                    p->m_header->restoreHeaderState(p->m_headerState);
                    p->m_model->sortOnColumn(p->m_header->sortIndicatorSection(), p->m_header->sortIndicatorOrder());
                },
                Qt::SingleShotConnection);
        }
    }
}

void FilterWidget::searchEvent(const QString& search)
{
    p->m_filteredTracks.clear();
    emit requestSearch(search);
    p->m_searchStr = search;
}

void FilterWidget::tracksAdded(const TrackList& tracks)
{
    p->m_model->addTracks(tracks);
}

void FilterWidget::tracksUpdated(const TrackList& tracks)
{
    if(tracks.empty()) {
        emit finishedUpdating();
        return;
    }

    p->m_updating = true;

    const QModelIndexList selectedRows = p->m_view->selectionModel()->selectedRows();

    QStringList selected;
    for(const QModelIndex& index : selectedRows) {
        if(!index.parent().isValid()) {
            selected.emplace_back(QStringLiteral(""));
        }
        else {
            selected.emplace_back(index.data(Qt::DisplayRole).toString());
        }
    }

    p->m_model->updateTracks(tracks);

    QObject::connect(
        p->m_model, &FilterModel::modelUpdated, this,
        [this, selected]() {
            const QModelIndexList selectedIndexes = p->m_model->indexesForValues(selected);

            QItemSelection indexesToSelect;
            indexesToSelect.reserve(selectedIndexes.size());

            const int columnCount = static_cast<int>(p->m_columns.size());

            for(const QModelIndex& index : selectedIndexes) {
                if(index.isValid()) {
                    const QModelIndex last = index.siblingAtColumn(columnCount - 1);
                    indexesToSelect.append({index, last.isValid() ? last : index});
                }
            }

            p->m_view->selectionModel()->select(indexesToSelect, QItemSelectionModel::ClearAndSelect);
            p->m_updating = false;
            emit finishedUpdating();
        },
        Qt::SingleShotConnection);
}

void FilterWidget::tracksPlayed(const TrackList& tracks)
{
    p->m_model->refreshTracks(tracks);
}

void FilterWidget::tracksRemoved(const TrackList& tracks)
{
    p->m_model->removeTracks(tracks);
}

void FilterWidget::contextMenuEvent(QContextMenuEvent* event)
{
    if(p->m_view->selectionModel()->selectedRows().empty()) {
        return;
    }

    emit requestContextMenu(event->globalPos());
}
} // namespace Fooyin::Filters

#include "moc_filterwidget.cpp"
