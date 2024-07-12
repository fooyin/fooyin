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

#include <core/library/trackfilter.h>
#include <core/library/tracksort.h>
#include <core/track.h>
#include <utils/actions/widgetcontext.h>
#include <utils/async.h>
#include <utils/enum.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/tooltipfilter.h>
#include <utils/utils.h>
#include <utils/widgets/autoheaderview.h>
#include <utils/widgets/expandedtreeview.h>

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

    FilterColumnRegistry* m_columnRegistry;
    SettingsManager* m_settings;

    ExpandedTreeView* m_view;
    AutoHeaderView* m_header;
    FilterModel* m_model;
    FilterSortModel* m_sortProxy;

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

    Private(FilterWidget* self, FilterColumnRegistry* columnRegistry, CoverProvider* coverProvider,
            SettingsManager* settings)
        : m_self{self}
        , m_columnRegistry{columnRegistry}
        , m_settings{settings}
        , m_view{new ExpandedTreeView(m_self)}
        , m_header{new AutoHeaderView(Qt::Horizontal, m_self)}
        , m_model{new FilterModel(coverProvider, m_settings, m_self)}
        , m_sortProxy{new FilterSortModel(m_self)}
        , m_widgetContext{
              new WidgetContext(m_self, Context{Id{"Fooyin.Context.FilterWidget."}.append(m_self->id())}, m_self)}
    {
        m_sortProxy->setSourceModel(m_model);
        m_view->setModel(m_sortProxy);
        m_view->setHeader(m_header);
        m_view->setItemDelegate(new FilterDelegate(m_self));
        m_view->viewport()->installEventFilter(new ToolTipFilter(m_self));

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

        m_model->setFont(m_settings->value<Settings::Filters::FilterFont>());
        m_model->setColour(m_settings->value<Settings::Filters::FilterColour>());
        m_model->setRowHeight(m_settings->value<Settings::Filters::FilterRowHeight>());

        hideHeader(!m_settings->value<Settings::Filters::FilterHeader>());
        setScrollbarEnabled(m_settings->value<Settings::Filters::FilterScrollBar>());
        m_view->setAlternatingRowColors(m_settings->value<Settings::Filters::FilterAltColours>());
        m_view->changeIconSize(m_settings->value<Settings::Filters::FilterIconSize>().toSize());

        setupConnections();
    }

    void setupConnections()
    {
        QObject::connect(m_columnRegistry, &FilterColumnRegistry::columnChanged, m_self,
                         [this](const Filters::FilterColumn& column) { columnChanged(column); });
        QObject::connect(m_columnRegistry, &FilterColumnRegistry::itemRemoved, m_self,
                         [this](int id) { columnRemoved(id); });

        QObject::connect(m_header, &QHeaderView::sectionCountChanged, m_self, [this]() {
            if(m_view->viewMode() == ExpandedTreeView::ViewMode::Icon) {
                m_model->setColumnOrder(Utils::logicalIndexOrder(m_header));
            }
        });
        QObject::connect(m_header, &QHeaderView::sectionMoved, m_self,
                         [this]() { m_model->setColumnOrder(Utils::logicalIndexOrder(m_header)); });
        QObject::connect(m_header, &QHeaderView::sortIndicatorChanged, m_sortProxy, &QSortFilterProxyModel::sort);
        QObject::connect(m_header, &ExpandedTreeView::customContextMenuRequested, m_self,
                         [this](const QPoint& pos) { filterHeaderMenu(pos); });
        QObject::connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, m_self,
                         [this](const QItemSelection& selected, const QItemSelection& deselected) {
                             selectionChanged(selected, deselected);
                         });
        QObject::connect(m_view, &ExpandedTreeView::viewModeChanged, m_self, [this](ExpandedTreeView::ViewMode mode) {
            m_model->setShowDecoration(mode == ExpandedTreeView::ViewMode::Icon);
        });
        QObject::connect(m_view, &QAbstractItemView::iconSizeChanged, m_self,
                         [this](const QSize& size) { m_settings->set<Settings::Filters::FilterIconSize>(size); });
        QObject::connect(m_view, &ExpandedTreeView::doubleClicked, m_self, [this](const QModelIndex& index) {
            if(index.isValid()) {
                emit m_self->doubleClicked();
            }
        });
        QObject::connect(m_view, &ExpandedTreeView::middleClicked, m_self, [this](const QModelIndex& index) {
            if(index.isValid()) {
                emit m_self->middleClicked();
            }
        });

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
        m_settings->subscribe<Settings::Filters::FilterIconSize>(
            m_self, [this](const auto& size) { m_view->changeIconSize(size.toSize()); });
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

    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
    {
        if(m_searching || m_updating) {
            return;
        }

        if(selected.indexes().empty() && deselected.indexes().empty()) {
            return;
        }

        refreshFilteredTracks();

        emit m_self->selectionChanged();
    }

    void updateViewMode(ExpandedTreeView::ViewMode mode) const
    {
        m_view->setViewMode(mode);

        if(mode == ExpandedTreeView::ViewMode::Tree) {
            m_model->setColumnOrder({});
        }
        else {
            m_model->setColumnOrder(Utils::logicalIndexOrder(m_header));
        }
    }

    void updateCaptions(ExpandedTreeView::CaptionDisplay captions) const
    {
        if(captions == ExpandedTreeView::CaptionDisplay::None) {
            m_model->setShowLabels(false);
        }
        else {
            m_model->setShowLabels(true);
        }

        m_view->setCaptionDisplay(captions);
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

        auto* displayList      = new QAction(tr("Columns"), displayGroup);
        auto* displayArtBottom = new QAction(tr("Artwork (bottom labels)"), displayGroup);
        auto* displayArtLeft   = new QAction(tr("Artwork (right labels)"), displayGroup);
        auto* displayArtNone   = new QAction(tr("Artwork (no labels)"), displayGroup);

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

        QObject::connect(displayList, &QAction::triggered, m_self, [this]() {
            updateViewMode(ViewMode::Tree);
            updateCaptions(CaptionDisplay::Bottom);
        });
        QObject::connect(displayArtBottom, &QAction::triggered, m_self, [this]() {
            updateViewMode(ViewMode::Icon);
            updateCaptions(CaptionDisplay::Bottom);
        });
        QObject::connect(displayArtLeft, &QAction::triggered, m_self, [this]() {
            updateViewMode(ViewMode::Icon);
            updateCaptions(CaptionDisplay::Right);
        });
        QObject::connect(displayArtNone, &QAction::triggered, m_self, [this]() {
            updateViewMode(ViewMode::Icon);
            updateCaptions(CaptionDisplay::None);
        });

        auto* displaySummary = new QAction(tr("Summary Item"), displayMenu);
        displaySummary->setCheckable(true);
        displaySummary->setChecked(m_model->showSummary());
        QObject::connect(displaySummary, &QAction::triggered, m_self,
                         [this](bool checked) { m_model->setShowSummary(checked); });

        auto* coverGroup = new QActionGroup(displayMenu);

        auto* coverFront  = new QAction(tr("Front Cover"), coverGroup);
        auto* coverBack   = new QAction(tr("Back Cover"), coverGroup);
        auto* coverArtist = new QAction(tr("Artist"), coverGroup);

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

        QObject::connect(coverFront, &QAction::triggered, m_self,
                         [this]() { m_model->setCoverType(Track::Cover::Front); });
        QObject::connect(coverBack, &QAction::triggered, m_self,
                         [this]() { m_model->setCoverType(Track::Cover::Back); });
        QObject::connect(coverArtist, &QAction::triggered, m_self,
                         [this]() { m_model->setCoverType(Track::Cover::Artist); });

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

    void filterHeaderMenu(const QPoint& pos)
    {
        auto* menu = new QMenu(m_self);
        menu->setAttribute(Qt::WA_DeleteOnClose);

        auto* columnsMenu = new QMenu(tr("Columns"), menu);
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
        QObject::connect(columnGroup, &QActionGroup::triggered, m_self, [this](QAction* action) {
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

        auto* multiColAction = new QAction(tr("Multiple Columns"), menu);
        multiColAction->setCheckable(true);
        multiColAction->setChecked(m_multipleColumns);
        multiColAction->setEnabled(m_columns.size() <= 1);
        QObject::connect(multiColAction, &QAction::triggered, m_self,
                         [this](bool checked) { m_multipleColumns = checked; });
        columnsMenu->addSeparator();
        columnsMenu->addAction(multiColAction);

        auto* moreSettings = new QAction(tr("More…"), columnsMenu);
        QObject::connect(moreSettings, &QAction::triggered, m_self,
                         [this]() { m_settings->settingsDialog()->openAtPage(Constants::Page::FiltersFields); });
        columnsMenu->addSeparator();
        columnsMenu->addAction(moreSettings);

        menu->addMenu(columnsMenu);
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

    void columnChanged(const FilterColumn& changedColumn)
    {
        auto existingIt = std::find_if(m_columns.begin(), m_columns.end(), [&changedColumn](const auto& column) {
            return (column.isDefault && changedColumn.isDefault && column.name == changedColumn.name)
                || column.id == changedColumn.id;
        });

        if(existingIt != m_columns.end()) {
            *existingIt = changedColumn;
            QMetaObject::invokeMethod(m_self, &FilterWidget::filterUpdated);
        }
    }

    void columnRemoved(int id)
    {
        FilterColumnList columns;
        std::ranges::copy_if(m_columns, std::back_inserter(columns),
                             [id](const auto& column) { return column.id != id; });
        if(std::exchange(m_columns, columns) != columns) {
            QMetaObject::invokeMethod(m_self, &FilterWidget::filterUpdated);
        }
    }
};

FilterWidget::FilterWidget(FilterColumnRegistry* columnRegistry, CoverProvider* coverProvider,
                           SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, columnRegistry, coverProvider, settings)}
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

    std::vector<Md5Hash> selected;
    const QModelIndexList selectedRows = p->m_view->selectionModel()->selectedRows();
    for(const QModelIndex& index : selectedRows) {
        selected.emplace_back(index.data(FilterItem::Key).toByteArray());
    }

    reset(tracks);

    QObject::connect(
        p->m_model, &FilterModel::modelUpdated, this,
        [this, selected]() {
            const QModelIndexList selectedIndexes = p->m_model->indexesForKeys(selected);

            QItemSelection indexesToSelect;
            indexesToSelect.reserve(selectedIndexes.size());

            const int columnCount = static_cast<int>(p->m_columns.size());

            for(const QModelIndex& index : selectedIndexes) {
                if(index.isValid()) {
                    const QModelIndex proxyIndex = p->m_sortProxy->mapFromSource(index);
                    const QModelIndex last       = proxyIndex.siblingAtColumn(columnCount - 1);
                    indexesToSelect.append({proxyIndex, last.isValid() ? last : proxyIndex});
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
    layout[u"Captions"]    = static_cast<int>(p->m_view->captionDisplay());
    layout[u"Artwork"]     = static_cast<int>(p->m_model->coverType());
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

            if(const auto columnItem = p->m_columnRegistry->itemById(column.at(0).toInt())) {
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

    if(layout.contains(u"Captions")) {
        p->updateCaptions(static_cast<ExpandedTreeView::CaptionDisplay>(layout.value(u"Captions").toInt()));
    }

    if(layout.contains(u"Artwork")) {
        p->m_model->setCoverType(static_cast<Track::Cover>(layout.value(u"Artwork").toInt()));
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
        const auto headerState = layout.value(u"State").toString().toUtf8();

        if(!headerState.isEmpty()
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
           && headerState.isValidUtf8()
#endif
        ) {
            const auto state = QByteArray::fromBase64(headerState);
            p->m_headerState = qUncompress(state);
        }
    }
}

void FilterWidget::finalise()
{
    p->m_multipleColumns = p->m_columns.size() > 1;

    if(!p->m_columns.empty()) {
        if(p->m_headerState.isEmpty()) {
            p->updateViewMode(p->m_view->viewMode());
            p->m_header->setSortIndicator(0, Qt::AscendingOrder);
        }
        else {
            QObject::connect(
                p->m_header, &AutoHeaderView::stateRestored, this,
                [this]() {
                    p->updateViewMode(p->m_view->viewMode());
                    p->m_sortProxy->sort(p->m_header->sortIndicatorSection(), p->m_header->sortIndicatorOrder());
                },
                Qt::SingleShotConnection);

            p->m_header->restoreHeaderState(p->m_headerState);
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

    std::vector<Md5Hash> selected;
    for(const QModelIndex& index : selectedRows) {
        if(index.isValid()) {
            selected.emplace_back(index.data(FilterItem::Key).toByteArray());
        }
    }

    p->m_model->updateTracks(tracks);

    QObject::connect(
        p->m_model, &FilterModel::modelUpdated, this,
        [this, selected]() {
            const QModelIndexList selectedIndexes = p->m_model->indexesForKeys(selected);

            QItemSelection indexesToSelect;
            indexesToSelect.reserve(selectedIndexes.size());

            const int columnCount = static_cast<int>(p->m_columns.size());

            for(const QModelIndex& index : selectedIndexes) {
                if(index.isValid()) {
                    const QModelIndex proxyIndex = p->m_sortProxy->mapFromSource(index);
                    const QModelIndex last       = proxyIndex.siblingAtColumn(columnCount - 1);
                    indexesToSelect.append({proxyIndex, last.isValid() ? last : proxyIndex});
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

void FilterWidget::keyPressEvent(QKeyEvent* event)
{
    const auto key = event->key();
    if(key == Qt::Key_Enter || key == Qt::Key_Return) {
        emit doubleClicked();
    }

    FyWidget::keyPressEvent(event);
}
} // namespace Fooyin::Filters

#include "moc_filterwidget.cpp"
