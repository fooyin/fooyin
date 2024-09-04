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
#include "filterconstants.h"
#include "filterdelegate.h"
#include "filterfwd.h"
#include "filteritem.h"
#include "filtermodel.h"
#include "settings/filtersettings.h"

#include <core/track.h>
#include <utils/actions/widgetcontext.h>
#include <utils/async.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/signalthrottler.h>
#include <utils/tooltipfilter.h>
#include <utils/utils.h>
#include <utils/widgets/autoheaderview.h>
#include <utils/widgets/expandedtreeview.h>

#include <QActionGroup>
#include <QApplication>
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
class FilterView : public ExpandedTreeView
{
    Q_OBJECT

public:
    using ExpandedTreeView::ExpandedTreeView;
};

FilterWidget::FilterWidget(FilterColumnRegistry* columnRegistry, LibraryManager* libraryManager,
                           CoverProvider* coverProvider, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_columnRegistry{columnRegistry}
    , m_settings{settings}
    , m_view{new FilterView(this)}
    , m_header{new AutoHeaderView(Qt::Horizontal, this)}
    , m_model{new FilterModel(libraryManager, coverProvider, m_settings, this)}
    , m_sortProxy{new FilterSortModel(this)}
    , m_resetThrottler{new SignalThrottler(this)}
    , m_widgetContext{new WidgetContext(this, Context{Id{"Fooyin.Context.FilterWidget."}.append(id())}, this)}
{
    setObjectName(FilterWidget::name());
    setFeature(FyWidget::Search);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    m_sortProxy->setSourceModel(m_model);
    m_view->setModel(m_sortProxy);
    m_view->setHeader(m_header);
    m_view->setItemDelegate(new FilterDelegate(this));
    m_view->viewport()->installEventFilter(new ToolTipFilter(this));

    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setTextElideMode(Qt::ElideRight);
    m_view->setDragEnabled(true);
    m_view->setDragDropMode(QAbstractItemView::DragOnly);
    m_view->setDefaultDropAction(Qt::CopyAction);
    m_view->setDropIndicatorShown(true);
    m_view->setUniformRowHeights(true);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    m_view->setSelectBeforeDrag(true);

    m_header->setStretchEnabled(true);
    m_header->setSortIndicatorShown(true);
    m_header->setSectionsMovable(true);
    m_header->setFirstSectionMovable(true);
    m_header->setSectionsClickable(true);
    m_header->setContextMenuPolicy(Qt::CustomContextMenu);

    if(const auto column = m_columnRegistry->itemByIndex(0)) {
        m_columns = {column.value()};
    }

    m_model->setRowHeight(m_settings->value<Settings::Filters::FilterRowHeight>());
    hideHeader(!m_settings->value<Settings::Filters::FilterHeader>());
    setScrollbarEnabled(m_settings->value<Settings::Filters::FilterScrollBar>());
    m_view->setAlternatingRowColors(m_settings->value<Settings::Filters::FilterAltColours>());
    m_view->changeIconSize(m_settings->value<Settings::Filters::FilterIconSize>().toSize());

    setupConnections();
}

FilterWidget::~FilterWidget()
{
    QObject::disconnect(m_resetThrottler, nullptr, this, nullptr);
    emit filterDeleted();
}

Id FilterWidget::group() const
{
    return m_group;
}

int FilterWidget::index() const
{
    return m_index;
}

bool FilterWidget::multipleColumns() const
{
    return m_multipleColumns;
}

bool FilterWidget::isActive() const
{
    return !m_filteredTracks.empty();
}

TrackList FilterWidget::tracks() const
{
    return m_tracks;
}

TrackList FilterWidget::filteredTracks() const
{
    return m_filteredTracks;
}

QString FilterWidget::searchFilter() const
{
    return m_searchStr;
}

WidgetContext* FilterWidget::widgetContext() const
{
    return m_widgetContext;
}

void FilterWidget::setGroup(const Id& group)
{
    m_group = group;
}

void FilterWidget::setIndex(int index)
{
    m_index = index;
}

void FilterWidget::refetchFilteredTracks()
{
    refreshFilteredTracks();
}

void FilterWidget::setFilteredTracks(const TrackList& tracks)
{
    m_filteredTracks = tracks;
}

void FilterWidget::clearFilteredTracks()
{
    m_filteredTracks.clear();
}

void FilterWidget::reset(const TrackList& tracks)
{
    m_tracks = tracks;
    m_resetThrottler->throttle();
}

void FilterWidget::softReset(const TrackList& tracks)
{
    m_updating = true;

    std::vector<Md5Hash> selected;
    const QModelIndexList selectedRows = m_view->selectionModel()->selectedRows();
    for(const QModelIndex& index : selectedRows) {
        selected.emplace_back(index.data(FilterItem::Key).toByteArray());
    }

    reset(tracks);

    QObject::connect(
        m_model, &FilterModel::modelUpdated, this,
        [this, selected]() {
            const QModelIndexList selectedIndexes = m_model->indexesForKeys(selected);

            QItemSelection indexesToSelect;
            indexesToSelect.reserve(selectedIndexes.size());

            const int columnCount = static_cast<int>(m_columns.size());

            for(const QModelIndex& index : selectedIndexes) {
                if(index.isValid()) {
                    const QModelIndex proxyIndex = m_sortProxy->mapFromSource(index);
                    const QModelIndex last       = proxyIndex.siblingAtColumn(columnCount - 1);
                    indexesToSelect.append({proxyIndex, last.isValid() ? last : proxyIndex});
                }
            }

            m_view->selectionModel()->select(indexesToSelect, QItemSelectionModel::ClearAndSelect);
            m_updating = false;
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

    for(int i{0}; const auto& column : m_columns) {
        const auto alignment = m_model->columnAlignment(i++);
        QString colStr       = QString::number(column.id);

        if(alignment != Qt::AlignLeft) {
            colStr += QStringLiteral(":") + QString::number(alignment.toInt());
        }

        columns.push_back(colStr);
    }

    layout[u"Columns"]     = columns.join(QStringLiteral("|"));
    layout[u"Display"]     = static_cast<int>(m_view->viewMode());
    layout[u"Captions"]    = static_cast<int>(m_view->captionDisplay());
    layout[u"Artwork"]     = static_cast<int>(m_model->coverType());
    layout[u"ShowSummary"] = m_model->showSummary();

    QByteArray state = m_header->saveHeaderState();
    state            = qCompress(state, 9);

    layout[u"Group"] = m_group.name();
    layout[u"Index"] = m_index;
    layout[u"State"] = QString::fromUtf8(state.toBase64());
}

void FilterWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(u"Columns")) {
        m_columns.clear();

        const QString columnData    = layout.value(u"Columns").toString();
        const QStringList columnIds = columnData.split(u'|');

        for(int i{0}; const auto& columnId : columnIds) {
            const auto column = columnId.split(u':');

            if(const auto columnItem = m_columnRegistry->itemById(column.at(0).toInt())) {
                m_columns.push_back(columnItem.value());

                if(column.size() > 1) {
                    m_model->changeColumnAlignment(i, static_cast<Qt::Alignment>(column.at(1).toInt()));
                }
                else {
                    m_model->changeColumnAlignment(i, Qt::AlignLeft);
                }
            }
            ++i;
        }
    }

    if(layout.contains(u"Display")) {
        m_view->setViewMode(static_cast<ExpandedTreeView::ViewMode>(layout.value(u"Display").toInt()));
    }

    if(layout.contains(u"Captions")) {
        updateCaptions(static_cast<ExpandedTreeView::CaptionDisplay>(layout.value(u"Captions").toInt()));
    }

    if(layout.contains(u"Artwork")) {
        m_model->setCoverType(static_cast<Track::Cover>(layout.value(u"Artwork").toInt()));
    }

    if(layout.contains(u"ShowSummary")) {
        m_model->setShowSummary(layout.value(u"ShowSummary").toBool());
    }

    if(layout.contains(u"Group")) {
        m_group = Id{layout.value(u"Group").toString()};
    }

    if(layout.contains(u"Index")) {
        m_index = layout.value(u"Index").toInt();
    }

    emit filterUpdated();

    if(layout.contains(u"State")) {
        const auto headerState = layout.value(u"State").toString().toUtf8();

        if(!headerState.isEmpty()
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
           && headerState.isValidUtf8()
#endif
        ) {
            const auto state = QByteArray::fromBase64(headerState);
            m_headerState    = qUncompress(state);
        }
    }
}

void FilterWidget::finalise()
{
    m_multipleColumns = m_columns.size() > 1;

    if(!m_columns.empty()) {
        if(m_headerState.isEmpty()) {
            m_header->setSortIndicator(0, Qt::AscendingOrder);
        }
        else {
            QObject::connect(
                m_header, &AutoHeaderView::stateRestored, this,
                [this]() { m_sortProxy->sort(m_header->sortIndicatorSection(), m_header->sortIndicatorOrder()); },
                Qt::SingleShotConnection);

            m_header->restoreHeaderState(m_headerState);
        }
    }
}

void FilterWidget::searchEvent(const QString& search)
{
    m_filteredTracks.clear();
    emit requestSearch(search);
    m_searchStr = search;
}

void FilterWidget::tracksAdded(const TrackList& tracks)
{
    m_model->addTracks(tracks);
}

void FilterWidget::tracksChanged(const TrackList& tracks)
{
    if(tracks.empty()) {
        emit finishedUpdating();
        return;
    }

    m_updating = true;

    const QModelIndexList selectedRows = m_view->selectionModel()->selectedRows();

    std::vector<Md5Hash> selected;
    for(const QModelIndex& index : selectedRows) {
        if(index.isValid()) {
            selected.emplace_back(index.data(FilterItem::Key).toByteArray());
        }
    }

    m_model->updateTracks(tracks);

    QObject::connect(
        m_model, &FilterModel::modelUpdated, this,
        [this, selected]() {
            const QModelIndexList selectedIndexes = m_model->indexesForKeys(selected);

            QItemSelection indexesToSelect;
            indexesToSelect.reserve(selectedIndexes.size());

            const int columnCount = static_cast<int>(m_columns.size());

            for(const QModelIndex& index : selectedIndexes) {
                if(index.isValid()) {
                    const QModelIndex proxyIndex = m_sortProxy->mapFromSource(index);
                    const QModelIndex last       = proxyIndex.siblingAtColumn(columnCount - 1);
                    indexesToSelect.append({proxyIndex, last.isValid() ? last : proxyIndex});
                }
            }

            m_view->selectionModel()->select(indexesToSelect, QItemSelectionModel::ClearAndSelect);
            m_updating = false;
            emit finishedUpdating();
        },
        Qt::SingleShotConnection);
}

void FilterWidget::tracksUpdated(const TrackList& tracks)
{
    m_model->refreshTracks(tracks);
}

void FilterWidget::tracksRemoved(const TrackList& tracks)
{
    m_model->removeTracks(tracks);
}

void FilterWidget::contextMenuEvent(QContextMenuEvent* event)
{
    if(m_view->selectionModel()->selectedRows().empty()) {
        return;
    }

    emit requestContextMenu(event->globalPos());
}

void FilterWidget::keyPressEvent(QKeyEvent* event)
{
    const auto key = event->key();
    if(key == Qt::Key_Enter || key == Qt::Key_Return) {
        emit doubleClicked();
    }

    FyWidget::keyPressEvent(event);
}

void FilterWidget::setupConnections()
{
    QObject::connect(m_resetThrottler, &SignalThrottler::triggered, this,
                     [this]() { m_model->reset(m_columns, m_tracks); });

    QObject::connect(m_columnRegistry, &FilterColumnRegistry::columnChanged, this, &FilterWidget::columnChanged);
    QObject::connect(m_columnRegistry, &FilterColumnRegistry::itemRemoved, this, &FilterWidget::columnRemoved);

    QObject::connect(m_header, &QHeaderView::sectionCountChanged, this, [this]() {
        if(m_view->viewMode() == ExpandedTreeView::ViewMode::Icon) {
            m_model->setColumnOrder(Utils::logicalIndexOrder(m_header));
        }
    });
    QObject::connect(m_header, &AutoHeaderView::sectionVisiblityChanged, this, [this]() {
        if(m_view->viewMode() == ExpandedTreeView::ViewMode::Icon) {
            m_model->setColumnOrder(Utils::logicalIndexOrder(m_header));
        }
    });
    QObject::connect(m_header, &QHeaderView::sectionMoved, this,
                     [this]() { m_model->setColumnOrder(Utils::logicalIndexOrder(m_header)); });
    QObject::connect(m_header, &QHeaderView::sortIndicatorChanged, m_sortProxy, &QSortFilterProxyModel::sort);
    QObject::connect(m_header, &ExpandedTreeView::customContextMenuRequested, this, &FilterWidget::filterHeaderMenu);
    QObject::connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &FilterWidget::handleSelectionChanged);
    QObject::connect(m_view, &ExpandedTreeView::viewModeChanged, this, [this](ExpandedTreeView::ViewMode mode) {
        m_model->setShowDecoration(mode == ExpandedTreeView::ViewMode::Icon);
    });
    QObject::connect(m_view, &QAbstractItemView::iconSizeChanged, this,
                     [this](const QSize& size) { m_settings->set<Settings::Filters::FilterIconSize>(size); });
    QObject::connect(m_view, &ExpandedTreeView::doubleClicked, this, &FilterWidget::doubleClicked);
    QObject::connect(m_view, &ExpandedTreeView::middleClicked, this, &FilterWidget::middleClicked);

    m_settings->subscribe<Settings::Filters::FilterAltColours>(m_view, &QAbstractItemView::setAlternatingRowColors);
    m_settings->subscribe<Settings::Filters::FilterHeader>(this, [this](bool enabled) { hideHeader(!enabled); });
    m_settings->subscribe<Settings::Filters::FilterScrollBar>(this,
                                                              [this](bool enabled) { setScrollbarEnabled(enabled); });
    m_settings->subscribe<Settings::Filters::FilterRowHeight>(this, [this](const int height) {
        m_model->setRowHeight(height);
        QMetaObject::invokeMethod(m_view->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));
    });
    m_settings->subscribe<Settings::Filters::FilterIconSize>(
        this, [this](const auto& size) { m_view->changeIconSize(size.toSize()); });
}

void FilterWidget::refreshFilteredTracks()
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

void FilterWidget::handleSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    if(m_searching || m_updating) {
        return;
    }

    if(selected.indexes().empty() && deselected.indexes().empty()) {
        return;
    }

    refreshFilteredTracks();

    emit selectionChanged();
}

void FilterWidget::updateViewMode(ExpandedTreeView::ViewMode mode)
{
    m_view->setViewMode(mode);

    if(mode == ExpandedTreeView::ViewMode::Tree) {
        m_model->setColumnOrder({});
    }
    else {
        m_model->setColumnOrder(Utils::logicalIndexOrder(m_header));
    }
}

void FilterWidget::updateCaptions(ExpandedTreeView::CaptionDisplay captions)
{
    if(captions == ExpandedTreeView::CaptionDisplay::None) {
        m_model->setShowLabels(false);
    }
    else {
        m_model->setShowLabels(true);
    }

    m_view->setCaptionDisplay(captions);
}

void FilterWidget::hideHeader(bool hide)
{
    m_header->setFixedHeight(hide ? 0 : QWIDGETSIZE_MAX);
    m_header->adjustSize();
}

void FilterWidget::setScrollbarEnabled(bool enabled)
{
    m_view->setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
}

void FilterWidget::addDisplayMenu(QMenu* menu)
{
    auto* displayMenu  = new QMenu(FilterWidget::tr("Display"), menu);
    auto* displayGroup = new QActionGroup(displayMenu);

    auto* displayList      = new QAction(FilterWidget::tr("Columns"), displayGroup);
    auto* displayArtBottom = new QAction(FilterWidget::tr("Artwork (bottom labels)"), displayGroup);
    auto* displayArtLeft   = new QAction(FilterWidget::tr("Artwork (right labels)"), displayGroup);
    auto* displayArtNone   = new QAction(FilterWidget::tr("Artwork (no labels)"), displayGroup);

    displayList->setCheckable(true);
    displayArtBottom->setCheckable(true);
    displayArtLeft->setCheckable(true);
    displayArtNone->setCheckable(true);

    const auto currentMode     = m_view->viewMode();
    const auto currentCaptions = m_view->captionDisplay();

    using ViewMode       = ExpandedTreeView::ViewMode;
    using CaptionDisplay = ExpandedTreeView::CaptionDisplay;

    if(currentMode == ExpandedTreeView::ViewMode::Tree) {
        displayList->setChecked(true);
    }
    else if(currentCaptions == CaptionDisplay::Bottom) {
        displayArtBottom->setChecked(true);
    }
    else if(currentCaptions == CaptionDisplay::Right) {
        displayArtLeft->setChecked(true);
    }
    else {
        displayArtNone->setChecked(true);
    }

    QObject::connect(displayList, &QAction::triggered, this, [this]() {
        updateViewMode(ViewMode::Tree);
        updateCaptions(CaptionDisplay::Bottom);
    });
    QObject::connect(displayArtBottom, &QAction::triggered, this, [this]() {
        updateViewMode(ViewMode::Icon);
        updateCaptions(CaptionDisplay::Bottom);
    });
    QObject::connect(displayArtLeft, &QAction::triggered, this, [this]() {
        updateViewMode(ViewMode::Icon);
        updateCaptions(CaptionDisplay::Right);
    });
    QObject::connect(displayArtNone, &QAction::triggered, this, [this]() {
        updateViewMode(ViewMode::Icon);
        updateCaptions(CaptionDisplay::None);
    });

    auto* displaySummary = new QAction(FilterWidget::tr("Summary item"), displayMenu);
    displaySummary->setCheckable(true);
    displaySummary->setChecked(m_model->showSummary());
    QObject::connect(displaySummary, &QAction::triggered, m_model, &FilterModel::setShowSummary);

    auto* coverGroup = new QActionGroup(displayMenu);

    auto* coverFront  = new QAction(FilterWidget::tr("Front cover"), coverGroup);
    auto* coverBack   = new QAction(FilterWidget::tr("Back cover"), coverGroup);
    auto* coverArtist = new QAction(FilterWidget::tr("Artist"), coverGroup);

    coverFront->setCheckable(true);
    coverBack->setCheckable(true);
    coverArtist->setCheckable(true);

    const auto currentType = m_model->coverType();
    if(currentType == Track::Cover::Front) {
        coverFront->setChecked(true);
    }
    else if(currentType == Track::Cover::Back) {
        coverBack->setChecked(true);
    }
    else {
        coverArtist->setChecked(true);
    }

    QObject::connect(coverFront, &QAction::triggered, this, [this]() { m_model->setCoverType(Track::Cover::Front); });
    QObject::connect(coverBack, &QAction::triggered, this, [this]() { m_model->setCoverType(Track::Cover::Back); });
    QObject::connect(coverArtist, &QAction::triggered, this, [this]() { m_model->setCoverType(Track::Cover::Artist); });

    displayMenu->addAction(displayList);
    displayMenu->addAction(displayArtBottom);
    displayMenu->addAction(displayArtLeft);
    displayMenu->addAction(displayArtNone);
    displayMenu->addSeparator();
    displayMenu->addAction(displaySummary);
    displayMenu->addSeparator();
    displayMenu->addAction(coverFront);
    displayMenu->addAction(coverBack);
    displayMenu->addAction(coverArtist);

    menu->addMenu(displayMenu);
}

void FilterWidget::filterHeaderMenu(const QPoint& pos)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* columnsMenu = new QMenu(FilterWidget::tr("Columns"), menu);
    auto* columnGroup = new QActionGroup{menu};
    columnGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::None);

    for(const auto& column : m_columnRegistry->items()) {
        auto* columnAction = new QAction(column.name, menu);
        columnAction->setData(column.id);
        columnAction->setCheckable(true);
        columnAction->setChecked(hasColumn(column.id));
        columnAction->setEnabled(!hasColumn(column.id) || m_columns.size() > 1);
        columnsMenu->addAction(columnAction);
        columnGroup->addAction(columnAction);
    }

    menu->setDefaultAction(columnGroup->checkedAction());
    QObject::connect(columnGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        const int columnId = action->data().toInt();
        if(action->isChecked()) {
            if(const auto column = m_columnRegistry->itemById(action->data().toInt())) {
                if(m_multipleColumns) {
                    m_columns.push_back(column.value());
                }
                else {
                    m_columns = {column.value()};
                }
            }
        }
        else {
            auto colIt
                = std::ranges::find_if(m_columns, [columnId](const FilterColumn& col) { return col.id == columnId; });
            if(colIt != m_columns.end()) {
                const int removedIndex = static_cast<int>(std::distance(m_columns.begin(), colIt));
                if(m_model->removeColumn(removedIndex)) {
                    m_columns.erase(colIt);
                }
            }
        }

        m_tracks.clear();
        emit filterUpdated();
    });

    auto* multiColAction = new QAction(FilterWidget::tr("Multiple columns"), menu);
    multiColAction->setCheckable(true);
    multiColAction->setChecked(m_multipleColumns);
    multiColAction->setEnabled(m_columns.size() <= 1);
    QObject::connect(multiColAction, &QAction::triggered, this, [this](bool checked) { m_multipleColumns = checked; });
    columnsMenu->addSeparator();
    columnsMenu->addAction(multiColAction);

    auto* moreSettings = new QAction(FilterWidget::tr("More…"), columnsMenu);
    QObject::connect(moreSettings, &QAction::triggered, this,
                     [this]() { m_settings->settingsDialog()->openAtPage(Constants::Page::FiltersFields); });
    columnsMenu->addSeparator();
    columnsMenu->addAction(moreSettings);

    menu->addMenu(columnsMenu);
    menu->addSeparator();
    m_header->addHeaderContextMenu(menu, mapToGlobal(pos));
    menu->addSeparator();
    m_header->addHeaderAlignmentMenu(menu, mapToGlobal(pos));

    addDisplayMenu(menu);

    menu->addSeparator();
    auto* manageConnections = new QAction(FilterWidget::tr("Manage groups"), menu);
    QObject::connect(manageConnections, &QAction::triggered, this, &FilterWidget::requestEditConnections);
    menu->addAction(manageConnections);

    menu->popup(mapToGlobal(pos));
}

[[nodiscard]] bool FilterWidget::hasColumn(int id) const
{
    return std::ranges::any_of(m_columns, [id](const FilterColumn& column) { return column.id == id; });
}

void FilterWidget::columnChanged(const FilterColumn& changedColumn)
{
    auto existingIt = std::find_if(m_columns.begin(), m_columns.end(), [&changedColumn](const auto& column) {
        return (column.isDefault && changedColumn.isDefault && column.name == changedColumn.name)
            || column.id == changedColumn.id;
    });

    if(existingIt != m_columns.end()) {
        *existingIt = changedColumn;
        emit filterUpdated();
    }
}

void FilterWidget::columnRemoved(int id)
{
    FilterColumnList columns;
    std::ranges::copy_if(m_columns, std::back_inserter(columns), [id](const auto& column) { return column.id != id; });
    if(std::exchange(m_columns, columns) != columns) {
        emit filterUpdated();
    }
}
} // namespace Fooyin::Filters

#include "filterwidget.moc"
#include "moc_filterwidget.cpp"
