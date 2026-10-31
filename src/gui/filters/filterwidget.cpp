/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "filtercolumneditordialog.h"
#include "filtercolumnregistry.h"
#include "filterconfigwidget.h"
#include "filterdelegate.h"
#include "filterexpansionwidget.h"
#include "filteritem.h"
#include "filtermodel.h"
#include "filterview.h"

#include <core/coresettings.h>
#include <core/library/librarymanager.h>
#include <core/player/playercontroller.h>
#include <core/plugins/coreplugincontext.h>
#include <gui/coverprovider.h>
#include <gui/coverrepository.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/guiutils.h>
#include <gui/scripting/richtextutils.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/autoheaderview.h>
#include <gui/widgets/backgroundimagesource.h>
#include <gui/widgets/expandedtreeview.h>
#include <utils/actions/widgetcontext.h>
#include <utils/async.h>
#include <utils/settings/settingsmanager.h>
#include <utils/tooltipfilter.h>
#include <utils/utils.h>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMenu>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimerEvent>

#include <set>

Q_LOGGING_CATEGORY(FILTER_WIDGET_TRACE, "fy.trace.filters.widget", QtInfoMsg)

using namespace Qt::StringLiterals;

constexpr auto FilterDoubleClickKey       = u"Filters/DoubleClickBehaviour";
constexpr auto FilterMiddleClickKey       = u"Filters/MiddleClickBehaviour";
constexpr auto FilterPlaylistEnabledKey   = u"Filters/SelectionPlaylistEnabled";
constexpr auto FilterAutoSwitchKey        = u"Filters/AutoSwitchSelectionPlaylist";
constexpr auto FilterAutoPlaylistKey      = u"Filters/SelectionPlaylistName";
constexpr auto FilterRestoreStateKey      = u"Filters/RestoreState";
constexpr auto FilterRowHeightKey         = u"Filters/RowHeight";
constexpr auto FilterSendPlaybackKey      = u"Filters/StartPlaybackOnSend";
constexpr auto FilterSourceKey            = u"Filters/Source";
constexpr auto FilterKeepAliveKey         = u"Filters/KeepAlive";
constexpr auto FilterIconSizeKey          = u"Filters/IconSize";
constexpr auto FilterAutoFitArtworkKey    = u"Filters/AutoFitArtwork";
constexpr auto FilterIconHorizontalGapKey = u"Filters/IconHorizontalGap";
constexpr auto FilterIconVerticalGapKey   = u"Filters/IconVerticalGap";
constexpr auto FilterArtworkRadiusKey     = u"Filters/ArtworkCornerRadius";
constexpr auto FilterArtworkMarginsKey    = u"Filters/ArtworkMargins";

constexpr auto FilterExpandGroupsKey                = u"Filters/ExpandGroups";
constexpr auto FilterExpansionAnimatedKey           = u"Filters/ExpansionAnimated";
constexpr auto FilterExpansionAlternatingRowsKey    = u"Filters/ExpansionAlternatingRows";
constexpr auto FilterExpandedArtworkKey             = u"Filters/ExpandedArtwork";
constexpr auto FilterExpansionDoubleClickKey        = u"Filters/ExpansionDoubleClickBehaviour";
constexpr auto FilterExpansionMiddleClickKey        = u"Filters/ExpansionMiddleClickBehaviour";
constexpr auto FilterExpansionArtworkMarginsKey     = u"Filters/ExpansionArtworkMargins";
constexpr auto FilterExpansionArtworkAutoMarginsKey = u"Filters/ExpansionArtworkAutoMargins";
constexpr auto FilterExpansionReflectionKey         = u"Filters/ExpansionArtworkReflection";
constexpr auto FilterExpansionReflectionHeightKey   = u"Filters/ExpansionArtworkReflectionHeight";
constexpr auto FilterExpansionReflectionOpacityKey  = u"Filters/ExpansionArtworkReflectionOpacity";
constexpr auto FilterExpansionReflectionGapKey      = u"Filters/ExpansionArtworkReflectionGap";
constexpr auto FilterExpansionCloseButtonKey        = u"Filters/ExpansionShowCloseButton";
constexpr auto FilterExpansionArtworkPositionKey    = u"Filters/ExpansionArtworkOnLeft";
constexpr auto FilterExpansionArtworkSizeKey        = u"Filters/ExpansionArtworkSize";
constexpr auto FilterExpansionMaximumColumnsKey     = u"Filters/ExpansionMaximumColumns";
constexpr auto FilterExpansionMinimumRowsKey        = u"Filters/ExpansionMinimumRows";
constexpr auto FilterExpansionStretchColumnsKey     = u"Filters/ExpansionStretchColumns";
constexpr auto FilterExpansionMinimumColumnWidthKey = u"Filters/ExpansionMinimumColumnWidth";
constexpr auto FilterExpansionScriptsKey            = u"Filters/ExpansionScripts";
constexpr auto FilterExpansionPlayingRowFontKey     = u"Filters/ExpansionPlayingRowFont";
constexpr auto FilterExpansionPlayingRowColourKey   = u"Filters/ExpansionPlayingRowColour";

constexpr auto FilterBackgroundKey                = u"Filters/Background";
constexpr auto FilterExpansionBackgroundKey       = u"Filters/ExpansionBackground";
constexpr auto FilterExpansionBackgroundColourKey = u"Filters/ExpansionBackgroundColour";

constexpr auto FilterDisplayKey         = u"Filters/Display";
constexpr auto FilterCaptionsKey        = u"Filters/Captions";
constexpr auto FilterArtworkKey         = u"Filters/Artwork";
constexpr auto FilterShowSummaryKey     = u"Filters/ShowSummary";
constexpr auto FilterShowHeaderKey      = u"Filters/ShowHeader";
constexpr auto FilterShowScrollbarKey   = u"Filters/ShowScrollbar";
constexpr auto FilterAlternatingRowsKey = u"Filters/AlternatingRows";
constexpr auto FilterCoverSourceKey     = u"Filters/CoverSource";

constexpr int FilterModelPinUpdateDelay = 50;

namespace Fooyin::Filters {
namespace {
QString normaliseColour(const QString& value)
{
    const QColor colour{value};
    return colour.isValid() ? colour.name(QColor::HexArgb) : QString{};
}

QMargins artworkMarginsFromJson(const QJsonObject& object, const QMargins& fallback)
{
    return {std::clamp(object.value("Left"_L1).toInt(fallback.left()), 0, 999),
            std::clamp(object.value("Top"_L1).toInt(fallback.top()), 0, 999),
            std::clamp(object.value("Right"_L1).toInt(fallback.right()), 0, 999),
            std::clamp(object.value("Bottom"_L1).toInt(fallback.bottom()), 0, 999)};
}

QJsonObject artworkMarginsToJson(const QMargins& margins)
{
    return {{"Left"_L1, margins.left()},
            {"Top"_L1, margins.top()},
            {"Right"_L1, margins.right()},
            {"Bottom"_L1, margins.bottom()}};
}
} // namespace

FilterWidget::FilterWidget(ActionManager* actionManager, FilterColumnRegistry* columnRegistry,
                           const CorePluginContext& core, CoverRepository* coverRepository,
                           TrackSelectionController* selection, QWidget* parent)
    : FyWidget{parent}
    , m_actionManager{actionManager}
    , m_columnRegistry{columnRegistry}
    , m_libraryManager{core.libraryManager}
    , m_settings{core.settingsManager}
    , m_view{new FilterView(this)}
    , m_delegate{new FilterDelegate(this)}
    , m_header{new AutoHeaderView(Qt::Horizontal, this)}
    , m_model{new FilterModel(core.library, new CoverProvider(coverRepository, this), m_settings, this)}
    , m_sortProxy{new FilterSortModel(this)}
    , m_backgroundSource{new BackgroundImageSource(m_model->coverProvider(), core.playerController, selection, this)}
    , m_index{-1}
    , m_multipleColumns{false}
    , m_widgetContext{new WidgetContext(
          this, Context{IdList{Constants::Context::TrackSelection, Id{"Fooyin.Context.FilterWidget."}.append(id())}},
          this)}
    , m_pendingScrollPosition{-1}
    , m_applyingViewState{false}
    , m_selectionRestorePending{false}
    , m_expansionUpdateQueued{false}
    , m_expansion{nullptr}
    , m_trackSelection{selection}
    , m_player{core.playerController}
    , m_expansionRequest{0}
    , m_scriptContext{core.scriptContext}
    , m_expansionSubscription{nullptr}
    , m_displayedGeneration{0}
{
    setObjectName(FilterWidget::name());
    setFeature(Search);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    m_sortProxy->setSourceModel(m_model);
    m_view->setModel(m_sortProxy);
    m_view->setHeader(m_header);
    m_view->setItemDelegate(m_delegate);
    m_view->viewport()->installEventFilter(new ToolTipFilter(this));
    QObject::connect(m_view, &FilterView::displayChanged, this, &FilterWidget::filterUpdated);
    QObject::connect(m_view, &FilterView::displayChanged, this, [this]() { scheduleVisibleCoverPinUpdate(); });

    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setTextElideMode(Qt::ElideRight);
    m_view->setDragEnabled(true);
    m_view->setDragDropMode(QAbstractItemView::DragOnly);
    m_view->setDefaultDropAction(Qt::CopyAction);
    m_view->setDropIndicatorShown(true);
    m_view->setUniformRowHeights(false);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    m_view->setSelectBeforeDrag(m_settings->value<Settings::Gui::DragOnlyAfterSelect>());
    m_settings->subscribe<Settings::Gui::DragOnlyAfterSelect>(m_view, &FilterView::setSelectBeforeDrag);

    m_header->setStretchEnabled(true);
    m_header->setSortIndicatorShown(true);
    m_header->setSectionsMovable(true);
    m_header->setFirstSectionMovable(true);
    m_header->setSectionsClickable(true);
    m_header->setContextMenuPolicy(Qt::CustomContextMenu);

    if(const auto column = m_columnRegistry->itemByIndex(0)) {
        m_columns = {column.value()};
    }

    m_config = defaultConfig();
    applyConfig(m_config);

    setupConnections();

    m_expansionSubscription = m_scriptContext->subscribe(this);
    m_expansionSubscription->setDependencies({.unclassified = false});
    QObject::connect(m_expansionSubscription, &ScriptSubscription::changed, this,
                     &FilterWidget::refreshExpansionScripts);
}

FilterWidget::~FilterWidget()
{
    m_model->coverProvider()->clearVisibleThumbnailKeys(this);
    Q_EMIT filterDeleted();
}

Id FilterWidget::group() const
{
    return m_group;
}

int FilterWidget::index() const
{
    return m_index;
}

const FilterColumnList& FilterWidget::columns() const
{
    return m_columns;
}

bool FilterWidget::multipleColumns() const
{
    return m_multipleColumns;
}

bool FilterWidget::isActive() const
{
    return !selectedKeys().empty();
}

std::vector<RowKey> FilterWidget::selectedKeys() const
{
    const QModelIndexList selectedIndexes = m_view->selectionModel()->selectedIndexes();

    std::vector<RowKey> keys;
    keys.reserve(selectedIndexes.size());
    std::set<RowKey> seen;

    for(const QModelIndex& index : selectedIndexes) {
        if(index.isValid()) {
            RowKey key = index.data(FilterItem::Key).toByteArray();
            if(seen.emplace(key).second) {
                keys.emplace_back(std::move(key));
            }
        }
    }

    if(keys.empty() && m_selectionRestorePending) {
        return m_restoredSelectedKeys;
    }

    return keys;
}

bool FilterWidget::selectionRestorePending() const
{
    return m_selectionRestorePending;
}

QString FilterWidget::searchText() const
{
    return m_searchStr;
}

WidgetContext* FilterWidget::widgetContext() const
{
    return m_widgetContext;
}

TrackAction FilterWidget::doubleClickAction() const
{
    return static_cast<TrackAction>(m_config.doubleClickAction);
}

TrackAction FilterWidget::middleClickAction() const
{
    return static_cast<TrackAction>(m_config.middleClickAction);
}

bool FilterWidget::sendPlayback() const
{
    return m_config.sendPlayback;
}

FilterSource FilterWidget::source() const
{
    return m_config.source;
}

bool FilterWidget::playlistEnabled() const
{
    return m_config.playlistEnabled;
}

bool FilterWidget::autoSwitch() const
{
    return m_config.autoSwitch;
}

bool FilterWidget::preservePlaybackPlaylist() const
{
    return m_config.preservePlaybackPlaylist;
}

QString FilterWidget::playlistName() const
{
    return m_config.playlistName;
}

bool FilterWidget::hasSelection() const
{
    return m_view->selectionModel()->hasSelection();
}

void FilterWidget::setGroup(const Id& group)
{
    m_group = group;
}

void FilterWidget::setIndex(int index)
{
    m_index = index;
}

void FilterWidget::setViewState(const FilterViewState& state)
{
    traceSortState("Before applying rows");

    m_applyingViewState = true;
    ++m_expansionRequest;
    m_searchStr               = state.searchText;
    m_restoredSelectedKeys    = state.selectedKeys;
    m_selectionRestorePending = state.selectionRestorePending;

    const bool rowsChanged = m_model->setRows(m_columns, state.rows);
    traceSortState("Rows applied", true);

    if(state.scrollPolicy == FilterScrollPolicy::ResetIfChanged && rowsChanged && m_pendingScrollPosition < 0) {
        m_view->scrollToTop();
    }

    auto* selectionModel = m_view->selectionModel();
    const QSignalBlocker blocker{selectionModel};
    selectionModel->clearSelection();

    const QModelIndexList selectedIndexes = m_model->indexesForKeys(state.selectedKeys);
    if(!selectedIndexes.empty()) {
        QItemSelection indexesToSelect;
        indexesToSelect.reserve(selectedIndexes.size());

        const int columnCount = static_cast<int>(m_columns.size());

        for(const QModelIndex& index : selectedIndexes) {
            if(!index.isValid()) {
                continue;
            }

            const QModelIndex proxyIndex = m_sortProxy->mapFromSource(index);
            if(!proxyIndex.isValid()) {
                continue;
            }

            const QModelIndex last = proxyIndex.siblingAtColumn(std::max(columnCount - 1, 0));
            indexesToSelect.append({proxyIndex, last.isValid() ? last : proxyIndex});
        }

        if(!indexesToSelect.empty()) {
            selectionModel->select(indexesToSelect, QItemSelectionModel::ClearAndSelect);
        }
    }

    m_applyingViewState = false;
    if(!m_expandedKey.isEmpty()) {
        scheduleExpansionUpdate();
    }

    if(m_expandedKey.isEmpty() && !state.rows.empty()) {
        restoreScrollPosition();
    }

    // Restored layouts can publish rows before the widget has gone through a resize,
    // so nudge the view to recalculate delegate-driven row heights immediately.
    QMetaObject::invokeMethod(m_delegate, "sizeHintChanged", Q_ARG(QModelIndex, {}));
}

ExpansionRequest FilterWidget::expansionPreviewRequest(const ExpansionScripts& scripts,
                                                       std::shared_ptr<const TrackList> tracks) const
{
    auto request = expansionRequest(std::move(tracks), scripts);
    request.columnValues.clear();
    for(const int column : Utils::logicalIndexOrder(m_header)) {
        request.columnScripts.push_back(m_columns.at(column).field);
    }
    return request;
}

ScriptContextService* FilterWidget::expansionScriptContext() const
{
    return m_scriptContext;
}

QString FilterWidget::name() const
{
    return tr("Library Filter");
}

QString FilterWidget::layoutName() const
{
    return u"LibraryFilter"_s;
}

void FilterWidget::saveLayoutData(QJsonObject& layout)
{
    traceSortState("Saving layout");

    saveConfigToLayout(m_config, layout);

    QStringList columns;
    for(int i{0}; const auto& column : m_columns) {
        const auto alignment = m_model->columnAlignment(i++);
        QString colStr       = QString::number(column.id);

        if(alignment != Qt::AlignLeft) {
            colStr += u":"_s + QString::number(alignment.toInt());
        }

        columns.push_back(colStr);
    }
    layout["Columns"_L1] = columns.join(u"|"_s);

    QJsonArray selectedKeys;
    for(const RowKey& key : this->selectedKeys()) {
        selectedKeys.push_back(QString::fromLatin1(key.toBase64()));
    }
    if(!selectedKeys.empty()) {
        layout["SelectedKeys"_L1] = selectedKeys;
    }
    if(m_config.restoreState && !m_expandedKey.isEmpty()) {
        layout["ExpandedKey"_L1] = QString::fromLatin1(m_expandedKey.toBase64());
    }
    layout["ScrollPosition"_L1] = m_view->verticalScrollBar()->value();

    QByteArray state = m_header->saveHeaderState();
    state            = qCompress(state, 9);

    layout["Group"_L1] = m_group.name();
    layout["Index"_L1] = m_index;
    layout["State"_L1] = QString::fromUtf8(state.toBase64());

    qCDebug(FILTER_WIDGET_TRACE) << "Saved header state:" << "filter=" << id().name()
                                 << "columns=" << layout.value("Columns"_L1).toString()
                                 << "state=" << layout.value("State"_L1).toString();
}

void FilterWidget::saveCopyLayoutData(QJsonObject& layout, LayoutCopyContext& context, bool isRoot)
{
    FyWidget::saveCopyLayoutData(layout, context, isRoot);

    if(isRoot) {
        layout["Index"_L1] = -1;
        return;
    }

    const QString group = layout.value("Group"_L1).toString();
    if(!group.isEmpty()) {
        layout["Group"_L1] = context.mappedString(u"Fooyin.Filters.FilterGroup"_s, group);
    }
}

void FilterWidget::loadLayoutData(const QJsonObject& layout)
{
    applyConfig(configFromLayout(layout));

    m_restoredSelectedKeys.clear();

    std::set<RowKey> seenKeys;
    const QJsonArray selectedKeys = m_config.restoreState ? layout.value("SelectedKeys"_L1).toArray() : QJsonArray{};
    for(const auto& value : selectedKeys) {
        if(!value.isString()) {
            continue;
        }

        RowKey key = QByteArray::fromBase64(value.toString().toLatin1());
        if(seenKeys.emplace(key).second) {
            m_restoredSelectedKeys.push_back(std::move(key));
        }
    }
    m_selectionRestorePending = !m_restoredSelectedKeys.empty();
    m_pendingScrollPosition   = m_config.restoreState ? layout.value("ScrollPosition"_L1).toInt(-1) : -1;

    if(layout.contains("Columns"_L1)) {
        m_columns.clear();

        const QString columnData    = layout.value("Columns"_L1).toString();
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

    if(layout.contains("Group"_L1)) {
        m_group = Id{layout.value("Group"_L1).toString()};
    }

    m_index       = layout.value("Index"_L1).toInt(m_index);
    m_expandedKey = m_config.restoreState ? QByteArray::fromBase64(layout.value("ExpandedKey"_L1).toString().toLatin1())
                                          : RowKey{};
    m_expandedColumns = m_columns;

    Q_EMIT filterUpdated();

    if(layout.contains("State"_L1)) {
        const auto headerState = layout.value("State"_L1).toString().toUtf8();

        if(!headerState.isEmpty() && headerState.isValidUtf8()) {
            const auto state = QByteArray::fromBase64(headerState);
            m_headerState    = qUncompress(state);
        }
    }

    qCDebug(FILTER_WIDGET_TRACE) << "Loaded header state:" << "filter=" << id().name() << "group=" << m_group.name()
                                 << "index=" << m_index << "columns=" << layout.value("Columns"_L1).toString()
                                 << "state=" << layout.value("State"_L1).toString()
                                 << "pendingHeaderBytes=" << m_headerState.size();
}

void FilterWidget::finalise()
{
    m_multipleColumns = m_columns.size() > 1;

    if(!m_columns.empty()) {
        if(m_headerState.isEmpty()) {
            m_header->setSortIndicator(0, Qt::AscendingOrder);
        }
        else {
            traceSortState("Restoring header state");
            QObject::connect(
                m_header, &AutoHeaderView::stateRestored, this,
                [this]() {
                    m_sortProxy->sort(m_header->sortIndicatorSection(), m_header->sortIndicatorOrder());
                    traceSortState("Header state restored", true);
                },
                Qt::SingleShotConnection);

            m_header->restoreHeaderState(m_headerState);
        }
    }

    updateAppearance();
}

void FilterWidget::searchEvent(const SearchRequest& request)
{
    if(std::exchange(m_searchStr, request.text) == m_searchStr) {
        return;
    }

    Q_EMIT searchTextChanged(m_searchStr);
}

FilterWidget::ConfigData FilterWidget::factoryConfig() const
{
    return {};
}

FilterWidget::ConfigData FilterWidget::defaultConfig() const
{
    auto config{factoryConfig()};

    config.doubleClickAction = m_settings->fileValue(FilterDoubleClickKey, config.doubleClickAction).toInt();
    config.middleClickAction = m_settings->fileValue(FilterMiddleClickKey, config.middleClickAction).toInt();
    config.expansionDoubleClickAction
        = m_settings->fileValue(FilterExpansionDoubleClickKey, config.expansionDoubleClickAction).toInt();
    config.expansionMiddleClickAction
        = m_settings->fileValue(FilterExpansionMiddleClickKey, config.expansionMiddleClickAction).toInt();
    config.sendPlayback = m_settings->fileValue(FilterSendPlaybackKey, config.sendPlayback).toBool();
    config.source
        = static_cast<FilterSource>(m_settings->fileValue(FilterSourceKey, static_cast<int>(config.source)).toInt());
    config.playlistEnabled = m_settings->fileValue(FilterPlaylistEnabledKey, config.playlistEnabled).toBool();
    config.autoSwitch      = m_settings->fileValue(FilterAutoSwitchKey, config.autoSwitch).toBool();
    config.preservePlaybackPlaylist
        = m_settings->fileValue(FilterKeepAliveKey, config.preservePlaybackPlaylist).toBool();
    config.playlistName        = m_settings->fileValue(FilterAutoPlaylistKey, config.playlistName).toString();
    config.restoreState        = m_settings->fileValue(FilterRestoreStateKey, config.restoreState).toBool();
    config.rowHeight           = m_settings->fileValue(FilterRowHeightKey, config.rowHeight).toInt();
    config.iconSize            = m_settings->fileValue(FilterIconSizeKey, config.iconSize).toSize();
    config.autoFitArtwork      = m_settings->fileValue(FilterAutoFitArtworkKey, config.autoFitArtwork).toBool();
    config.iconHorizontalGap   = m_settings->fileValue(FilterIconHorizontalGapKey, config.iconHorizontalGap).toInt();
    config.iconVerticalGap     = m_settings->fileValue(FilterIconVerticalGapKey, config.iconVerticalGap).toInt();
    config.artworkCornerRadius = m_settings->fileValue(FilterArtworkRadiusKey, config.artworkCornerRadius).toInt();
    config.artworkMargins      = artworkMarginsFromJson(
        QJsonObject::fromVariantMap(m_settings->fileValue(FilterArtworkMarginsKey).toMap()), config.artworkMargins);
    config.expandGroups      = m_settings->fileValue(FilterExpandGroupsKey, config.expandGroups).toBool();
    config.expansionAnimated = m_settings->fileValue(FilterExpansionAnimatedKey, config.expansionAnimated).toBool();
    config.expansionAlternatingRows
        = m_settings->fileValue(FilterExpansionAlternatingRowsKey, config.expansionAlternatingRows).toBool();
    config.expandedArtwork         = m_settings->fileValue(FilterExpandedArtworkKey, config.expandedArtwork).toBool();
    config.expansionArtworkMargins = artworkMarginsFromJson(
        QJsonObject::fromVariantMap(m_settings->fileValue(FilterExpansionArtworkMarginsKey).toMap()), {10, 10, 10, 10});
    config.expansionArtworkAutoMargins
        = m_settings->fileValue(FilterExpansionArtworkAutoMarginsKey, config.expansionArtworkAutoMargins).toBool();
    config.expansionArtworkReflection
        = m_settings->fileValue(FilterExpansionReflectionKey, config.expansionArtworkReflection).toBool();
    config.expansionArtworkReflectionHeight = std::clamp(
        m_settings->fileValue(FilterExpansionReflectionHeightKey, config.expansionArtworkReflectionHeight).toInt(), 1,
        100);
    config.expansionArtworkReflectionOpacity = std::clamp(
        m_settings->fileValue(FilterExpansionReflectionOpacityKey, config.expansionArtworkReflectionOpacity).toInt(), 0,
        100);
    config.expansionArtworkReflectionGap = std::clamp(
        m_settings->fileValue(FilterExpansionReflectionGapKey, config.expansionArtworkReflectionGap).toInt(), 0, 100);
    config.expansionShowCloseButton
        = m_settings->fileValue(FilterExpansionCloseButtonKey, config.expansionShowCloseButton).toBool();
    config.expansionArtworkOnLeft
        = m_settings->fileValue(FilterExpansionArtworkPositionKey, config.expansionArtworkOnLeft).toBool();
    config.expansionArtworkSize = std::clamp(
        m_settings->fileValue(FilterExpansionArtworkSizeKey, config.expansionArtworkSize).toInt(), 50, 200);
    config.expansionMaximumColumns
        = std::max(0, m_settings->fileValue(FilterExpansionMaximumColumnsKey, config.expansionMaximumColumns).toInt());
    config.expansionMinimumRows
        = std::max(1, m_settings->fileValue(FilterExpansionMinimumRowsKey, config.expansionMinimumRows).toInt());
    config.expansionStretchColumns
        = m_settings->fileValue(FilterExpansionStretchColumnsKey, config.expansionStretchColumns).toBool();
    config.expansionMinimumColumnWidth = std::clamp(
        m_settings->fileValue(FilterExpansionMinimumColumnWidthKey, config.expansionMinimumColumnWidth).toInt(), 25,
        300);
    config.expansionScripts.load(QJsonObject::fromVariantMap(m_settings->fileValue(FilterExpansionScriptsKey).toMap()));
    config.expansionPlayingRowFont
        = m_settings->fileValue(FilterExpansionPlayingRowFontKey, config.expansionPlayingRowFont).toString();
    config.expansionPlayingRowColour
        = m_settings->fileValue(FilterExpansionPlayingRowColourKey, config.expansionPlayingRowColour).toString();

    config.background.loadLayout(QJsonObject::fromVariantMap(m_settings->fileValue(FilterBackgroundKey).toMap()));
    config.expansionBackground.loadLayout(
        QJsonObject::fromVariantMap(m_settings->fileValue(FilterExpansionBackgroundKey).toMap()));
    config.expansionBackgroundColour
        = m_settings->fileValue(FilterExpansionBackgroundColourKey, config.expansionBackgroundColour).toString();

    config.viewMode = static_cast<ExpandedTreeView::ViewMode>(
        m_settings->fileValue(FilterDisplayKey, static_cast<int>(config.viewMode)).toInt());
    config.captionDisplay = static_cast<ExpandedTreeView::CaptionDisplay>(
        m_settings->fileValue(FilterCaptionsKey, static_cast<int>(config.captionDisplay)).toInt());
    config.coverType = static_cast<Track::Cover>(
        m_settings->fileValue(FilterArtworkKey, static_cast<int>(config.coverType)).toInt());
    config.showSummary     = m_settings->fileValue(FilterShowSummaryKey, config.showSummary).toBool();
    config.showHeader      = m_settings->fileValue(FilterShowHeaderKey, config.showHeader).toBool();
    config.showScrollbar   = m_settings->fileValue(FilterShowScrollbarKey, config.showScrollbar).toBool();
    config.alternatingRows = m_settings->fileValue(FilterAlternatingRowsKey, config.alternatingRows).toBool();
    const int coverSource
        = m_settings->fileValue(FilterCoverSourceKey, config.coverSource ? static_cast<int>(*config.coverSource) : -1)
              .toInt();
    config.coverSource
        = coverSource < 0 ? std::nullopt : std::optional{static_cast<ArtworkSourcePreference>(coverSource)};

    return config;
}

const FilterWidget::ConfigData& FilterWidget::currentConfig() const
{
    return m_config;
}

void FilterWidget::saveDefaults(const ConfigData& config) const
{
    m_settings->fileSet(FilterDoubleClickKey, config.doubleClickAction);
    m_settings->fileSet(FilterMiddleClickKey, config.middleClickAction);
    m_settings->fileSet(FilterExpansionDoubleClickKey, config.expansionDoubleClickAction);
    m_settings->fileSet(FilterExpansionMiddleClickKey, config.expansionMiddleClickAction);
    m_settings->fileSet(FilterSendPlaybackKey, config.sendPlayback);
    m_settings->fileSet(FilterSourceKey, static_cast<int>(config.source));
    m_settings->fileSet(FilterPlaylistEnabledKey, config.playlistEnabled);
    m_settings->fileSet(FilterAutoSwitchKey, config.autoSwitch);
    m_settings->fileSet(FilterKeepAliveKey, config.preservePlaybackPlaylist);
    m_settings->fileSet(FilterAutoPlaylistKey, config.playlistName);
    m_settings->fileSet(FilterRestoreStateKey, config.restoreState);
    m_settings->fileSet(FilterRowHeightKey, config.rowHeight);
    m_settings->fileSet(FilterIconSizeKey, config.iconSize);
    m_settings->fileSet(FilterAutoFitArtworkKey, config.autoFitArtwork);
    m_settings->fileSet(FilterIconHorizontalGapKey, config.iconHorizontalGap);
    m_settings->fileSet(FilterIconVerticalGapKey, config.iconVerticalGap);
    m_settings->fileSet(FilterArtworkRadiusKey, config.artworkCornerRadius);
    m_settings->fileSet(FilterArtworkMarginsKey, artworkMarginsToJson(config.artworkMargins).toVariantMap());
    m_settings->fileSet(FilterExpandGroupsKey, config.expandGroups);
    m_settings->fileSet(FilterExpansionAnimatedKey, config.expansionAnimated);
    m_settings->fileSet(FilterExpansionAlternatingRowsKey, config.expansionAlternatingRows);
    m_settings->fileSet(FilterExpandedArtworkKey, config.expandedArtwork);
    m_settings->fileSet(FilterExpansionArtworkMarginsKey,
                        artworkMarginsToJson(config.expansionArtworkMargins).toVariantMap());
    m_settings->fileSet(FilterExpansionArtworkAutoMarginsKey, config.expansionArtworkAutoMargins);
    m_settings->fileSet(FilterExpansionReflectionKey, config.expansionArtworkReflection);
    m_settings->fileSet(FilterExpansionReflectionHeightKey, config.expansionArtworkReflectionHeight);
    m_settings->fileSet(FilterExpansionReflectionOpacityKey, config.expansionArtworkReflectionOpacity);
    m_settings->fileSet(FilterExpansionReflectionGapKey, config.expansionArtworkReflectionGap);
    m_settings->fileSet(FilterExpansionCloseButtonKey, config.expansionShowCloseButton);
    m_settings->fileSet(FilterExpansionArtworkPositionKey, config.expansionArtworkOnLeft);
    m_settings->fileSet(FilterExpansionArtworkSizeKey, config.expansionArtworkSize);
    m_settings->fileSet(FilterExpansionMaximumColumnsKey, config.expansionMaximumColumns);
    m_settings->fileSet(FilterExpansionMinimumRowsKey, config.expansionMinimumRows);
    m_settings->fileSet(FilterExpansionStretchColumnsKey, config.expansionStretchColumns);
    m_settings->fileSet(FilterExpansionMinimumColumnWidthKey, config.expansionMinimumColumnWidth);
    m_settings->fileSet(FilterExpansionScriptsKey, config.expansionScripts.toJson().toVariantMap());
    m_settings->fileSet(FilterExpansionPlayingRowFontKey, config.expansionPlayingRowFont);
    m_settings->fileSet(FilterExpansionPlayingRowColourKey, config.expansionPlayingRowColour);

    QJsonObject background;
    config.background.saveLayout(background);
    m_settings->fileSet(FilterBackgroundKey, background.toVariantMap());

    QJsonObject expansionBackground;
    config.expansionBackground.saveLayout(expansionBackground);

    m_settings->fileSet(FilterExpansionBackgroundKey, expansionBackground.toVariantMap());
    m_settings->fileSet(FilterExpansionBackgroundColourKey, config.expansionBackgroundColour);
    m_settings->fileSet(FilterDisplayKey, static_cast<int>(config.viewMode));
    m_settings->fileSet(FilterCaptionsKey, static_cast<int>(config.captionDisplay));
    m_settings->fileSet(FilterArtworkKey, static_cast<int>(config.coverType));
    m_settings->fileSet(FilterShowSummaryKey, config.showSummary);
    m_settings->fileSet(FilterShowHeaderKey, config.showHeader);
    m_settings->fileSet(FilterShowScrollbarKey, config.showScrollbar);
    m_settings->fileSet(FilterAlternatingRowsKey, config.alternatingRows);
    m_settings->fileSet(FilterCoverSourceKey, config.coverSource ? static_cast<int>(*config.coverSource) : -1);
}

void FilterWidget::clearSavedDefaults() const
{
    m_settings->fileRemove(FilterDoubleClickKey);
    m_settings->fileRemove(FilterMiddleClickKey);
    m_settings->fileRemove(FilterExpansionDoubleClickKey);
    m_settings->fileRemove(FilterExpansionMiddleClickKey);
    m_settings->fileRemove(FilterSendPlaybackKey);
    m_settings->fileRemove(FilterSourceKey);
    m_settings->fileRemove(FilterPlaylistEnabledKey);
    m_settings->fileRemove(FilterAutoSwitchKey);
    m_settings->fileRemove(FilterKeepAliveKey);
    m_settings->fileRemove(FilterAutoPlaylistKey);
    m_settings->fileRemove(FilterRestoreStateKey);
    m_settings->fileRemove(FilterRowHeightKey);
    m_settings->fileRemove(FilterIconSizeKey);
    m_settings->fileRemove(FilterAutoFitArtworkKey);
    m_settings->fileRemove(FilterIconHorizontalGapKey);
    m_settings->fileRemove(FilterIconVerticalGapKey);
    m_settings->fileRemove(FilterArtworkRadiusKey);
    m_settings->fileRemove(FilterArtworkMarginsKey);
    m_settings->fileRemove(FilterExpandGroupsKey);
    m_settings->fileRemove(FilterExpansionAnimatedKey);
    m_settings->fileRemove(FilterExpansionAlternatingRowsKey);
    m_settings->fileRemove(FilterExpandedArtworkKey);
    m_settings->fileRemove(FilterExpansionArtworkMarginsKey);
    m_settings->fileRemove(FilterExpansionArtworkAutoMarginsKey);
    m_settings->fileRemove(FilterExpansionReflectionKey);
    m_settings->fileRemove(FilterExpansionReflectionHeightKey);
    m_settings->fileRemove(FilterExpansionReflectionOpacityKey);
    m_settings->fileRemove(FilterExpansionReflectionGapKey);
    m_settings->fileRemove(FilterExpansionCloseButtonKey);
    m_settings->fileRemove(FilterExpansionArtworkPositionKey);
    m_settings->fileRemove(FilterExpansionArtworkSizeKey);
    m_settings->fileRemove(FilterExpansionMaximumColumnsKey);
    m_settings->fileRemove(FilterExpansionMinimumRowsKey);
    m_settings->fileRemove(FilterExpansionStretchColumnsKey);
    m_settings->fileRemove(FilterExpansionMinimumColumnWidthKey);
    m_settings->fileRemove(FilterExpansionScriptsKey);
    m_settings->fileRemove(FilterExpansionPlayingRowFontKey);
    m_settings->fileRemove(FilterExpansionPlayingRowColourKey);
    m_settings->fileRemove(FilterBackgroundKey);
    m_settings->fileRemove(FilterExpansionBackgroundKey);
    m_settings->fileRemove(FilterExpansionBackgroundColourKey);
    m_settings->fileRemove(FilterDisplayKey);
    m_settings->fileRemove(FilterCaptionsKey);
    m_settings->fileRemove(FilterArtworkKey);
    m_settings->fileRemove(FilterShowSummaryKey);
    m_settings->fileRemove(FilterShowHeaderKey);
    m_settings->fileRemove(FilterShowScrollbarKey);
    m_settings->fileRemove(FilterAlternatingRowsKey);
    m_settings->fileRemove(FilterCoverSourceKey);
}

void FilterWidget::applyConfig(const ConfigData& config)
{
    auto validated{config};

    validated.background.normalise();
    validated.expansionBackground.normalise();
    validated.expansionBackground.fadeDuration = 0;
    validated.expansionBackgroundColour        = normaliseColour(validated.expansionBackgroundColour);
    validated.expansionPlayingRowColour        = normaliseColour(validated.expansionPlayingRowColour);

    if(validated.source != FilterSource::Library && validated.source != FilterSource::CurrentPlaylist) {
        validated.source = FilterSource::Library;
    }
    if(validated.viewMode != ExpandedTreeView::ViewMode::Tree
       && validated.viewMode != ExpandedTreeView::ViewMode::Icon) {
        validated.viewMode = ExpandedTreeView::ViewMode::Tree;
    }
    if(validated.captionDisplay != ExpandedTreeView::CaptionDisplay::None
       && validated.captionDisplay != ExpandedTreeView::CaptionDisplay::Bottom
       && validated.captionDisplay != ExpandedTreeView::CaptionDisplay::Right) {
        validated.captionDisplay = ExpandedTreeView::CaptionDisplay::Bottom;
    }
    if(validated.coverType != Track::Cover::Front && validated.coverType != Track::Cover::Back
       && validated.coverType != Track::Cover::Artist) {
        validated.coverType = Track::Cover::Front;
    }
    if(validated.coverSource && *validated.coverSource != ArtworkSourcePreference::PreferDirectory
       && *validated.coverSource != ArtworkSourcePreference::PreferEmbedded) {
        validated.coverSource.reset();
    }

    validated.rowHeight           = std::max(validated.rowHeight, 0);
    validated.iconHorizontalGap   = std::max(validated.iconHorizontalGap, -1);
    validated.iconVerticalGap     = std::max(validated.iconVerticalGap, 0);
    validated.artworkCornerRadius = std::clamp(validated.artworkCornerRadius, 0, 100);
    validated.artworkMargins      = artworkMarginsFromJson({}, validated.artworkMargins);

    if(!validated.iconSize.isValid()) {
        validated.iconSize = factoryConfig().iconSize;
    }

    validated.expansionArtworkMargins           = artworkMarginsFromJson({}, validated.expansionArtworkMargins);
    validated.expansionArtworkSize              = std::clamp(validated.expansionArtworkSize, 50, 200);
    validated.expansionArtworkReflectionHeight  = std::clamp(validated.expansionArtworkReflectionHeight, 1, 100);
    validated.expansionArtworkReflectionOpacity = std::clamp(validated.expansionArtworkReflectionOpacity, 0, 100);
    validated.expansionArtworkReflectionGap     = std::clamp(validated.expansionArtworkReflectionGap, 0, 100);
    validated.expansionMinimumRows              = std::max(1, validated.expansionMinimumRows);
    validated.expansionMaximumColumns           = std::max(0, validated.expansionMaximumColumns);
    validated.expansionMinimumColumnWidth       = std::clamp(validated.expansionMinimumColumnWidth, 25, 300);

    const bool appearanceChanged
        = m_config.viewMode != validated.viewMode || m_config.captionDisplay != validated.captionDisplay
       || m_config.coverType != validated.coverType || m_config.coverSource != validated.coverSource
       || m_config.showSummary != validated.showSummary || m_config.showHeader != validated.showHeader
       || m_config.showScrollbar != validated.showScrollbar || m_config.alternatingRows != validated.alternatingRows;
    const bool backgroundChanged = m_config.background != validated.background
                                || m_config.expansionAlternatingRows != validated.expansionAlternatingRows
                                || m_config.expansionBackground != validated.expansionBackground
                                || m_config.expansionBackgroundColour != validated.expansionBackgroundColour;
    const bool layoutChanged     = m_config.expansionMinimumRows != validated.expansionMinimumRows
                                || m_config.expansionMaximumColumns != validated.expansionMaximumColumns
                                || m_config.expansionStretchColumns != validated.expansionStretchColumns
                                || m_config.expansionMinimumColumnWidth != validated.expansionMinimumColumnWidth
                                || m_config.expansionArtworkOnLeft != validated.expansionArtworkOnLeft
                                || m_config.expansionArtworkSize != validated.expansionArtworkSize
                                || m_config.expansionShowCloseButton != validated.expansionShowCloseButton
                                || m_config.expansionArtworkReflection != validated.expansionArtworkReflection
                                || m_config.expansionArtworkReflectionHeight != validated.expansionArtworkReflectionHeight
                                || m_config.expansionArtworkReflectionOpacity != validated.expansionArtworkReflectionOpacity
                                || m_config.expansionArtworkReflectionGap != validated.expansionArtworkReflectionGap
                                || m_config.expansionArtworkAutoMargins != validated.expansionArtworkAutoMargins
                                || m_config.expansionArtworkMargins != validated.expansionArtworkMargins;
    const bool hasConfigChanged
        = m_config.doubleClickAction != validated.doubleClickAction
       || m_config.expansionDoubleClickAction != validated.expansionDoubleClickAction
       || m_config.expansionMiddleClickAction != validated.expansionMiddleClickAction
       || m_config.middleClickAction != validated.middleClickAction || m_config.sendPlayback != validated.sendPlayback
       || m_config.source != validated.source || m_config.playlistEnabled != validated.playlistEnabled
       || m_config.autoSwitch != validated.autoSwitch
       || m_config.preservePlaybackPlaylist != validated.preservePlaybackPlaylist
       || m_config.playlistName != validated.playlistName || m_config.restoreState != validated.restoreState
       || m_config.rowHeight != validated.rowHeight || m_config.iconSize != validated.iconSize
       || m_config.autoFitArtwork != validated.autoFitArtwork
       || m_config.iconHorizontalGap != validated.iconHorizontalGap
       || m_config.iconVerticalGap != validated.iconVerticalGap
       || m_config.artworkCornerRadius != validated.artworkCornerRadius
       || m_config.artworkMargins != validated.artworkMargins || m_config.expandGroups != validated.expandGroups
       || m_config.expansionAnimated != validated.expansionAnimated
       || m_config.expandedArtwork != validated.expandedArtwork
       || m_config.expansionScripts != validated.expansionScripts
       || m_config.expansionPlayingRowFont != validated.expansionPlayingRowFont
       || m_config.expansionPlayingRowColour != validated.expansionPlayingRowColour;

    if(hasConfigChanged) {
        ++m_expansionRequest;
    }

    m_config = std::move(validated);

    if(m_view->viewMode() != m_config.viewMode) {
        updateViewMode(m_config.viewMode);
    }

    m_model->setShowDecoration(m_config.viewMode == ExpandedTreeView::ViewMode::Icon);
    updateCaptions(m_config.captionDisplay);
    m_model->setCoverType(m_config.coverType);
    m_model->setCoverSource(m_config.coverSource);
    m_model->setShowSummary(m_config.showSummary);
    m_header->setCollapsed(!m_config.showHeader);
    m_view->setCollapseDelayEnabled(doubleClickAction() != TrackAction::None);
    m_view->setExpansionAnimated(m_config.expansionAnimated);
    m_view->setBackgroundConfig(m_config.background);
    m_backgroundSource->setConfig(m_config.background);
    updateAppearance();

    if(m_expansion) {
        m_expansion->setBackground(m_config.expansionBackground, m_config.expansionBackgroundColour);
        m_expansion->setAlternatingRowColors(m_config.expansionAlternatingRows);
    }

    m_model->setRowHeight(m_config.rowHeight);
    m_view->setAutoFitArtwork(m_config.autoFitArtwork);
    m_view->setIconHorizontalGap(m_config.iconHorizontalGap);
    m_view->setIconVerticalGap(m_config.iconVerticalGap);
    m_delegate->setArtworkCornerRadius(m_config.artworkCornerRadius);
    m_delegate->setArtworkMargins(m_config.artworkMargins);
    m_view->changeIconSize(m_config.iconSize);
    m_model->setIconSize(m_view->effectiveIconSize());

    if(m_expansion && layoutChanged) {
        m_expansion->setArtworkOnLeft(m_config.expansionArtworkOnLeft);
        m_expansion->setArtworkSize(m_config.expansionArtworkSize);
        m_expansion->setCloseButtonVisible(m_config.expansionShowCloseButton);
        m_expansion->setArtworkReflection({.enabled = m_config.expansionArtworkReflection,
                                           .height  = m_config.expansionArtworkReflectionHeight,
                                           .opacity = m_config.expansionArtworkReflectionOpacity,
                                           .gap     = m_config.expansionArtworkReflectionGap});
        m_expansion->setArtworkAutoMargins(m_config.expansionArtworkAutoMargins);
        m_expansion->setArtworkMargins(m_config.expansionArtworkMargins);
        m_expansion->setMinimumColumnRows(m_config.expansionMinimumRows);
        m_expansion->setMaximumColumns(m_config.expansionMaximumColumns);
        m_expansion->setStretchColumns(m_config.expansionStretchColumns);
        m_expansion->setMinimumColumnWidth(m_config.expansionMinimumColumnWidth);
    }

    if(!m_config.expandGroups) {
        closeExpansion();
    }
    else if(!m_expandedKey.isEmpty() && (hasConfigChanged || appearanceChanged || !m_expansion)) {
        scheduleExpansionUpdate();
    }

    m_view->viewport()->update();
    QMetaObject::invokeMethod(m_delegate, "sizeHintChanged", Q_ARG(QModelIndex, {}));

    if(hasConfigChanged) {
        Q_EMIT configChanged();
    }
    else if(layoutChanged || backgroundChanged || appearanceChanged) {
        Q_EMIT expansionLayoutChanged();
    }
}

void FilterWidget::openConfigDialog()
{
    showConfigDialog(new FilterConfigDialog(this, m_columnRegistry, this), Qt::NonModal);
}

void FilterWidget::addFilterHeaderMenu(QMenu* menu, const QPoint& pos, bool includeWidgetActions)
{
    auto* columnsMenu = new QMenu(tr("Columns"), menu);
    auto* columnGroup = new QActionGroup{menu};
    columnGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::None);

    for(const auto& column : m_columnRegistry->items()) {
        const bool columnVisible
            = std::ranges::any_of(m_columns, [column](const FilterColumn& current) { return current.id == column.id; });
        if(!column.isEnabled && !columnVisible) {
            continue;
        }

        auto* columnAction = new QAction(column.name, menu);
        columnAction->setData(column.id);
        columnAction->setCheckable(true);
        columnAction->setChecked(columnVisible);
        columnAction->setEnabled(columnAction->isChecked() ? m_columns.size() > 1 : true);
        columnsMenu->addAction(columnAction);
        columnGroup->addAction(columnAction);
    }

    menu->setDefaultAction(columnGroup->checkedAction());
    QObject::connect(columnGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        const int columnId = action->data().toInt();
        if(action->isChecked()) {
            if(const auto column = m_columnRegistry->itemById(columnId)) {
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
                                              [columnId](const FilterColumn& column) { return column.id == columnId; });

            if(colIt != m_columns.end()) {
                const int removedIndex = static_cast<int>(std::distance(m_columns.begin(), colIt));
                if(m_model->removeColumn(removedIndex)) {
                    m_columns.erase(colIt);
                }
            }
        }

        traceSortState("Filter columns changed");
        Q_EMIT filterUpdated();
    });

    auto* multiColAction = new QAction(tr("Multiple columns"), menu);
    multiColAction->setCheckable(true);
    multiColAction->setChecked(m_multipleColumns);
    multiColAction->setEnabled(m_columns.size() <= 1);
    QObject::connect(multiColAction, &QAction::triggered, this, [this](bool checked) { m_multipleColumns = checked; });
    columnsMenu->addSeparator();
    columnsMenu->addAction(multiColAction);

    auto* moreSettings = new QAction(tr("More…"), columnsMenu);
    QObject::connect(moreSettings, &QAction::triggered, this, [this]() {
        auto* dialog = new FilterColumnEditorDialog(m_columnRegistry, this);
        dialog->open();
    });
    columnsMenu->addSeparator();
    columnsMenu->addAction(moreSettings);

    menu->addMenu(columnsMenu);
    menu->addSeparator();
    m_header->addHeaderContextMenu(menu, m_header->mapToGlobal(pos));
    menu->addSeparator();
    m_header->addHeaderAlignmentMenu(menu, m_header->mapToGlobal(pos));

    addDisplayMenu(menu);

    auto* sourceMenu  = new QMenu(tr("Source"), menu);
    auto* sourceGroup = new QActionGroup(sourceMenu);

    const auto addSourceAction = [this, sourceMenu, sourceGroup](const QString& text, FilterSource source) {
        auto* action = sourceMenu->addAction(text);
        action->setCheckable(true);
        action->setChecked(m_config.source == source);
        action->setData(static_cast<int>(source));
        sourceGroup->addAction(action);
    };

    addSourceAction(tr("Library"), FilterSource::Library);
    addSourceAction(tr("Current playlist"), FilterSource::CurrentPlaylist);

    QObject::connect(sourceGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        ConfigData config{m_config};
        config.source = static_cast<FilterSource>(action->data().toInt());
        applyConfig(config);
    });

    menu->addMenu(sourceMenu);
    menu->addSeparator();

    if(includeWidgetActions) {
        menu->addSeparator();
        auto* manageConnections = new QAction(tr("Manage filter groups…"), menu);
        QObject::connect(manageConnections, &QAction::triggered, this, &FilterWidget::requestEditConnections);
        menu->addAction(manageConnections);

        auto* configure = new QAction(tr("Configure…"), menu);
        QObject::connect(configure, &QAction::triggered, this, &FilterWidget::openConfigDialog);
        menu->addAction(configure);
    }
}

void FilterWidget::contextMenuEvent(QContextMenuEvent* event)
{
    Q_EMIT requestContextMenu(event->globalPos(), m_widgetContext, hasSelection());
}

void FilterWidget::keyPressEvent(QKeyEvent* event)
{
    const auto key = event->key();
    if(key == Qt::Key_Escape && !m_expandedKey.isEmpty()) {
        closeExpansion(true);
        event->accept();
        return;
    }
    if(key == Qt::Key_Enter || key == Qt::Key_Return) {
        Q_EMIT doubleClicked();
    }

    FyWidget::keyPressEvent(event);
}

void FilterWidget::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_visibleCoverPinUpdateTimer.timerId()) {
        m_visibleCoverPinUpdateTimer.stop();
        updateVisibleCoverPins();
        return;
    }

    FyWidget::timerEvent(event);
}

void FilterWidget::setupConnections()
{
    const auto syncIconColumnOrder = [this]() {
        if(m_view->viewMode() == ExpandedTreeView::ViewMode::Icon) {
            m_model->setColumnOrder(Utils::logicalIndexOrder(m_header));
        }
        if(!m_expandedKey.isEmpty()) {
            scheduleExpansionUpdate();
        }
        traceSortState("Header columns changed");
    };

    QObject::connect(m_columnRegistry, &FilterColumnRegistry::columnChanged, this, &FilterWidget::columnChanged);
    QObject::connect(m_columnRegistry, &FilterColumnRegistry::itemRemoved, this, &FilterWidget::columnRemoved);

    QObject::connect(m_header, &QHeaderView::sectionCountChanged, this, syncIconColumnOrder);
    QObject::connect(m_header, &AutoHeaderView::sectionVisiblityChanged, this, syncIconColumnOrder);
    QObject::connect(m_header, &QHeaderView::sectionMoved, this, syncIconColumnOrder);
    QObject::connect(m_header, &QHeaderView::sortIndicatorChanged, m_sortProxy, &QSortFilterProxyModel::sort);
    QObject::connect(m_header, &QHeaderView::sortIndicatorChanged, this,
                     [this]() { traceSortState("Sort indicator changed", true); });
    QObject::connect(m_header, &QHeaderView::sectionClicked, this, [this](int column) {
        qCDebug(FILTER_WIDGET_TRACE) << "Header clicked:" << "filter=" << id().name() << "logicalColumn=" << column
                                     << "visualColumn=" << m_header->visualIndex(column) << "label="
                                     << m_model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
    });
    QObject::connect(m_header, &QWidget::customContextMenuRequested, this, &FilterWidget::filterHeaderMenu);
    QObject::connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &FilterWidget::handleSelectionChanged);
    QObject::connect(m_view, &ExpandedTreeView::viewModeChanged, this, [this](ExpandedTreeView::ViewMode mode) {
        m_model->setShowDecoration(mode == ExpandedTreeView::ViewMode::Icon);
        if(mode != ExpandedTreeView::ViewMode::Icon) {
            closeExpansion();
        }
        scheduleVisibleCoverPinUpdate();
    });
    QObject::connect(m_view, &QAbstractItemView::iconSizeChanged, this, [this](const QSize& size) {
        if(size.isValid() && m_config.iconSize != size) {
            m_config.iconSize = size;
            m_model->setIconSize(m_view->effectiveIconSize());
            scheduleVisibleCoverPinUpdate();
            Q_EMIT configChanged();
        }
    });
    QObject::connect(m_view, &ExpandedTreeView::effectiveIconSizeChanged, this, [this](const QSize& size) {
        m_model->setIconSize(size);
        scheduleVisibleCoverPinUpdate();
    });
    QObject::connect(m_view->verticalScrollBar(), &QScrollBar::valueChanged, this,
                     [this]() { scheduleVisibleCoverPinUpdate(); });
    QObject::connect(m_sortProxy, &QAbstractItemModel::modelReset, this,
                     [this]() { scheduleVisibleCoverPinUpdate(FilterModelPinUpdateDelay); });
    QObject::connect(m_sortProxy, &QAbstractItemModel::rowsInserted, this,
                     [this]() { scheduleVisibleCoverPinUpdate(FilterModelPinUpdateDelay); });
    QObject::connect(m_sortProxy, &QAbstractItemModel::rowsRemoved, this,
                     [this]() { scheduleVisibleCoverPinUpdate(FilterModelPinUpdateDelay); });
    QObject::connect(m_view, &ExpandedTreeView::clicked, this, &FilterWidget::expandSelection);
    QObject::connect(m_view, &FilterView::collapseRequested, this, [this]() { closeExpansion(true); });
    QObject::connect(m_sortProxy, &QAbstractItemModel::layoutChanged, this, [this]() {
        if(!m_applyingViewState && !m_expandedKey.isEmpty()) {
            scheduleExpansionUpdate();
        }
    });
    QObject::connect(m_view, &ExpandedTreeView::doubleClicked, this, &FilterWidget::doubleClicked);
    QObject::connect(m_view, &ExpandedTreeView::middleClicked, this, &FilterWidget::middleClicked);

    QObject::connect(m_backgroundSource, &BackgroundImageSource::pixmapChanged, m_view,
                     &FilterView::setBackgroundPixmap);

    m_settings->subscribe<Settings::Gui::ResolvedAppStyle>(this, [this](const QVariant& var) {
        const auto resolvedStyle = var.value<ResolvedAppStyle>();
        Gui::updateItemViewStyle(m_view, resolvedStyle.palette);
        scheduleExpansionUpdate();
    });

    const auto refreshRatingStars = [this]() {
        scheduleExpansionUpdate();
    };
    m_settings->subscribe<Settings::Gui::RatingFullStarSymbol>(this, refreshRatingStars);
    m_settings->subscribe<Settings::Gui::RatingHalfStarSymbol>(this, refreshRatingStars);
    m_settings->subscribe<Settings::Gui::RatingEmptyStarSymbol>(this, refreshRatingStars);
}

void FilterWidget::traceSortState(const char* event, bool includeRows) const
{
    if(!FILTER_WIDGET_TRACE().isDebugEnabled()) {
        return;
    }

    const QString filterId = id().name();
    const int headerColumn = m_header->sortIndicatorSection();
    const int sortColumn   = m_sortProxy->sortColumn();

    qCDebug(FILTER_WIDGET_TRACE) << event << "filter=" << filterId << "group=" << m_group.name() << "index=" << m_index
                                 << "sourceRows=" << m_model->rowCount({}) << "proxyRows=" << m_sortProxy->rowCount()
                                 << "headerColumn=" << headerColumn
                                 << "headerVisualColumn=" << m_header->visualIndex(headerColumn) << "headerLabel="
                                 << m_model->headerData(headerColumn, Qt::Horizontal, Qt::DisplayRole).toString()
                                 << "headerOrder=" << m_header->sortIndicatorOrder() << "proxyColumn=" << sortColumn
                                 << "proxyLabel="
                                 << m_sortProxy->headerData(sortColumn, Qt::Horizontal, Qt::DisplayRole).toString()
                                 << "proxyOrder=" << m_sortProxy->sortOrder() << "viewMode=" << m_view->viewMode()
                                 << "visibleColumns=" << Utils::logicalIndexOrder(m_header);

    if(!includeRows) {
        return;
    }

    for(int logical{0}; const auto& column : m_columns) {
        qCDebug(FILTER_WIDGET_TRACE) << "Column:" << "filter=" << filterId << "logical=" << logical
                                     << "visual=" << m_header->visualIndex(logical)
                                     << "hidden=" << m_header->isSectionHidden(logical) << "id=" << column.id
                                     << "name=" << column.name << "displayScript=" << column.field
                                     << "sortScript=" << column.sortField;
        ++logical;
    }

    for(int row{0}; row < std::min(8, m_sortProxy->rowCount()); ++row) {
        const QModelIndex sourceIndex = m_sortProxy->mapToSource(m_sortProxy->index(row, 0));
        if(!sourceIndex.isValid()) {
            continue;
        }

        const auto* item = m_model->itemForIndex(sourceIndex);
        qCDebug(FILTER_WIDGET_TRACE) << "Sorted row:" << "filter=" << filterId << "row=" << row
                                     << "sourceRow=" << sourceIndex.row() << "key=" << item->key().toHex()
                                     << "columns=" << item->columns() << "sortValue=" << item->sortColumn(sortColumn);
    }
}

void FilterWidget::handleSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    if(m_applyingViewState) {
        return;
    }

    if(selected.indexes().empty() && deselected.indexes().empty()) {
        return;
    }

    m_view->cancelPendingCollapse();
    m_restoredSelectedKeys.clear();
    m_selectionRestorePending = false;

    expandSelection();
    Q_EMIT selectionKeysChanged(selectedKeys());
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

    scheduleVisibleCoverPinUpdate();
}

void FilterWidget::updateCaptions(ExpandedTreeView::CaptionDisplay captions)
{
    m_model->setShowLabels(captions != ExpandedTreeView::CaptionDisplay::None);
    m_view->setCaptionDisplay(captions);
}

void FilterWidget::updateAppearance()
{
    m_view->setVerticalScrollBarPolicy(m_config.showScrollbar ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    m_view->setAlternatingRowColors(m_config.alternatingRows);

    const QVariant resolvedStyleValue = m_settings->value<Settings::Gui::ResolvedAppStyle>();
    Gui::refreshItemViewPalette(m_view, resolvedStyleValue.value<ResolvedAppStyle>().palette);
}

void FilterWidget::scheduleVisibleCoverPinUpdate(int delay)
{
    if(delay <= 0 || !m_visibleCoverPinUpdateTimer.isActive()) {
        m_visibleCoverPinUpdateTimer.start(delay, this);
    }
}

void FilterWidget::updateVisibleCoverPins()
{
    auto* coverProvider = m_model->coverProvider();
    if(m_view->viewMode() != ExpandedTreeView::ViewMode::Icon) {
        coverProvider->clearVisibleThumbnailKeys(this);
        return;
    }

    std::set<QString> keys;

    const QSize artworkSize   = m_view->effectiveIconSize();
    const int pinMargin       = std::max(artworkSize.width(), artworkSize.height());
    const auto visibleIndexes = m_view->visibleIndexes(pinMargin);

    for(const QModelIndex& index : visibleIndexes) {
        if(index.isValid()) {
            const QString key = index.data(FilterItem::CoverKey).toString();
            if(!key.isEmpty()) {
                keys.emplace(key);
            }
        }
    }

    coverProvider->setVisibleThumbnailKeys(this, keys);
}

void FilterWidget::expandSelection()
{
    if(m_applyingViewState || !m_config.expandGroups || m_view->viewMode() != ExpandedTreeView::ViewMode::Icon) {
        return;
    }

    const auto rows = m_view->selectionModel()->selectedRows();
    ++m_expansionRequest;

    if(rows.size() != 1 || rows.front().data(FilterItem::IsSummary).toBool()) {
        scheduleExpansionUpdate();
        return;
    }

    m_expandedKey     = rows.front().data(FilterItem::Key).toByteArray();
    m_expandedColumns = m_columns;
    scheduleExpansionUpdate();
}

void FilterWidget::scheduleExpansionUpdate()
{
    if(std::exchange(m_expansionUpdateQueued, true)) {
        return;
    }
    QMetaObject::invokeMethod(
        this,
        [this]() {
            m_expansionUpdateQueued = false;
            updateExpansion();
        },
        Qt::QueuedConnection);
}

void FilterWidget::updateExpansion()
{
    if(m_expandedKey.isEmpty()) {
        return;
    }

    if(!m_config.expandGroups || m_view->viewMode() != ExpandedTreeView::ViewMode::Icon
       || m_expandedColumns != m_columns) {
        closeExpansion();
        return;
    }

    const auto selection = selectedKeys();
    if(!m_selectionRestorePending && (selection.size() != 1 || selection.front() != m_expandedKey)) {
        closeExpansion();
        return;
    }

    const auto indexes = m_model->indexesForKeys({m_expandedKey});
    if(indexes.empty()) {
        if(!m_selectionRestorePending) {
            closeExpansion();
        }
        return;
    }

    const QModelIndex index = m_sortProxy->mapFromSource(indexes.front());
    if(!index.isValid() || index.data(FilterItem::IsSummary).toBool()) {
        closeExpansion();
        return;
    }

    m_expansionColumnValues.clear();
    const auto logicalColumns = Utils::logicalIndexOrder(m_header);
    for(const int column : logicalColumns) {
        const auto columnIndex = index.siblingAtColumn(column);
        auto rich              = columnIndex.data(FilterItem::RichColumn).value<RichText>();
        if(rich.empty()) {
            rich.blocks.push_back({.text = m_model->itemForIndex(indexes.front())->column(column), .format = {}});
        }
        m_expansionColumnValues.push_back(rich.joinedText());
    }

    auto tracks = index.data(FilterItem::Tracks).value<TrackList>();
    if(tracks.empty()) {
        closeExpansion();
        return;
    }

    ++m_expansionRequest;
    auto request = expansionRequest(std::make_shared<const TrackList>(std::move(tracks)), m_config.expansionScripts);
    request.sortScript = m_settings->value<Settings::Core::LibraryViewPlaylistSortScript>();
    submitExpansion(std::move(request));
}

void FilterWidget::submitExpansion(ExpansionRequest request)
{
    if(!m_expansion) {
        m_expansion = new FilterExpansionWidget(m_trackSelection, m_model->coverProvider()->repository());
        m_view->setExpansionWidget(m_expansion);

        QObject::connect(m_expansion, &FilterExpansionWidget::populated, this, &FilterWidget::applyExpansionContent);
        QObject::connect(m_expansion, &FilterExpansionWidget::closeRequested, this, [this]() { closeExpansion(true); });
        QObject::connect(m_expansion, &FilterExpansionWidget::contentChanged, m_view,
                         &ExpandedTreeView::refreshExpansion);
        QObject::connect(m_expansion, &FilterExpansionWidget::requestContextMenu, this,
                         &FilterWidget::requestContextMenu);
        QObject::connect(m_expansion, &FilterExpansionWidget::doubleClicked, this, [this]() {
            const auto action = m_config.expansionDoubleClickAction < 0
                                  ? doubleClickAction()
                                  : static_cast<TrackAction>(m_config.expansionDoubleClickAction);
            Q_EMIT actionRequested(action);
        });
        QObject::connect(m_expansion, &FilterExpansionWidget::middleClicked, this, [this]() {
            const auto action = m_config.expansionMiddleClickAction < 0
                                  ? middleClickAction()
                                  : static_cast<TrackAction>(m_config.expansionMiddleClickAction);
            Q_EMIT actionRequested(action);
        });
        QObject::connect(m_expansion, &FilterExpansionWidget::scrollAdjustmentRequested, this, [this](int delta) {
            auto* bar = m_view->verticalScrollBar();
            bar->setValue(bar->value() + delta);
        });
        QObject::connect(m_expansion, &FilterExpansionWidget::trackFocused, this, [this](const QRect& rect) {
            const QRect visible = rect.translated(m_expansion->pos());
            auto* bar           = m_view->verticalScrollBar();
            if(visible.top() < 0) {
                bar->setValue(bar->value() + visible.top());
            }
            else if(visible.bottom() >= m_view->viewport()->height()) {
                bar->setValue(bar->value() + visible.bottom() - m_view->viewport()->height() + 1);
            }
        });
    }

    m_expansion->submit(std::move(request));
}

ExpansionRequest FilterWidget::expansionRequest(std::shared_ptr<const TrackList> tracks,
                                                const ExpansionScripts& scripts) const
{
    ExpansionRequest request;
    request.generation                      = m_expansionRequest;
    request.key                             = m_expandedKey;
    request.scripts                         = scripts;
    request.tracks                          = std::move(tracks);
    request.columnValues                    = m_expansionColumnValues;
    request.font                            = m_view->font();
    request.libraries                       = m_libraryManager ? m_libraryManager->allLibraries() : LibraryInfoMap{};
    request.context                         = m_scriptContext ? m_scriptContext->playbackContext()
                                            : m_player        ? makePlaybackScriptData(m_player)
                                                              : ScriptContextData{};
    request.context.ownedTracks             = request.tracks;
    request.context.playlist                = nullptr;
    request.context.playlistData            = {};
    request.context.playlistData.trackCount = static_cast<int>(request.tracks->size());
    request.context.freeze();

    if(!m_config.expansionPlayingRowFont.isEmpty()) {
        QFont font;
        if(font.fromString(m_config.expansionPlayingRowFont)) {
            request.playingRowFont = font;
        }
    }
    request.playingRowColour = QColor{m_config.expansionPlayingRowColour};
    if(!request.playingRowColour.isValid()) {
        request.playingRowColour = Gui::defaultPlayingRowColour();
    }

    if(m_player) {
        request.queue = m_player->playbackQueue().tracks();
    }

    request.options = {.trackListContextPolicy = TrackListContextPolicy::Fallback,
                       .escapeRichText         = true,
                       .useVariousArtists      = m_settings->value<Settings::Core::UseVariousForCompilations>(),
                       .ratingSymbols          = Gui::ratingStarSymbols(*m_settings)};
    return request;
}

void FilterWidget::applyExpansionContent(const ExpansionResult& result)
{
    if(result.generation != m_expansionRequest || result.key != m_expandedKey || m_expandedKey.isEmpty()) {
        return;
    }

    Q_EMIT expansionScriptErrorsChanged(result.errors);
    if(!result.errors.empty()) {
        return;
    }

    const auto indexes = m_model->indexesForKeys({m_expandedKey});
    if(indexes.empty()) {
        return;
    }

    const auto index = m_sortProxy->mapFromSource(indexes.front());
    if(!index.isValid()) {
        return;
    }

    m_expansion->setBackground(m_config.expansionBackground, m_config.expansionBackgroundColour);
    m_expansion->setAlternatingRowColors(m_config.expansionAlternatingRows);
    m_expansion->setMinimumColumnRows(m_config.expansionMinimumRows);
    m_expansion->setMaximumColumns(m_config.expansionMaximumColumns);
    m_expansion->setStretchColumns(m_config.expansionStretchColumns);
    m_expansion->setMinimumColumnWidth(m_config.expansionMinimumColumnWidth);
    m_expansion->setArtworkOnLeft(m_config.expansionArtworkOnLeft);
    m_expansion->setArtworkSize(m_config.expansionArtworkSize);
    m_expansion->setCloseButtonVisible(m_config.expansionShowCloseButton);
    m_expansion->setArtworkReflection({.enabled = m_config.expansionArtworkReflection,
                                       .height  = m_config.expansionArtworkReflectionHeight,
                                       .opacity = m_config.expansionArtworkReflectionOpacity,
                                       .gap     = m_config.expansionArtworkReflectionGap});
    m_expansion->setArtworkAutoMargins(m_config.expansionArtworkAutoMargins);
    m_expansion->setArtworkMargins(m_config.expansionArtworkMargins);

    const bool changed = m_displayedKey != result.key;
    if(result.rebuild) {
        if(changed) {
            if(m_pendingScrollPosition < 0) {
                // Reveal clipped icons before the expansion starts
                m_view->scrollTo(index, QAbstractItemView::EnsureVisible);
            }
            m_view->prepareExpansionChange();
        }
        {
            const QSignalBlocker blocker{m_expansion};
            m_expansion->setContent(result, !changed);
            m_expansion->setArtwork(result.coverTrack, m_model->coverType(), m_model->coverSource(),
                                    m_config.expandedArtwork, index.data(Qt::DecorationRole).value<QPixmap>());
        }

        m_expansionTracks     = result.tracks;
        m_displayedKey        = result.key;
        m_displayedGeneration = result.generation;

        if(m_view->expandedIndex() == index) {
            m_view->refreshExpansion();
        }
        else {
            m_view->setExpandedIndex(index);
        }

        if(!m_view->currentIndex().isValid()) {
            m_view->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        }

        if(m_pendingScrollPosition >= 0) {
            restoreScrollPosition();
        }
        else if(changed) {
            const QRect iconRect     = m_view->visualRect(index);
            const int viewportHeight = m_view->viewport()->height();
            const int bottom         = m_expansion->y() + m_expansion->height();
            const int spacing        = m_view->captionDisplay() != ExpandedTreeView::CaptionDisplay::Right
                                            || m_view->useIconGapsForSideCaptions()
                                         ? m_view->iconVerticalGap()
                                         : 0;
            // Keep the selected icon and its top spacing visible while showing the expansion
            const int scroll = std::clamp(bottom - viewportHeight, 0, std::max(0, iconRect.top() - spacing));
            if(scroll > 0) {
                auto* bar = m_view->verticalScrollBar();
                bar->setValue(bar->value() + scroll);
            }
        }
    }
    else {
        m_expansion->updateContent(result);
    }

    m_expansionDependencies = result.dependencies;
    m_expansionContext      = result.context;

    if(m_expansionSubscription) {
        m_expansionSubscription->setDependencies(result.dependencies.combined());
    }

    scheduleVisibleCoverPinUpdate();

    if(m_scriptContext && !m_scriptContext->isCurrent(result.context, result.dependencies.combined(), *result.tracks)) {
        refreshExpansionScripts({});
    }
}

void FilterWidget::refreshExpansionScripts(const ScriptChanges& changes)
{
    if(!m_expansionTracks || m_expandedKey.isEmpty() || m_expandedKey != m_displayedKey
       || m_displayedGeneration != m_expansionRequest) {
        return;
    }

    auto request     = expansionRequest(m_expansionTracks, m_config.expansionScripts);
    request.rebuild  = false;
    request.sections = changes.empty() ? ExpansionAll : 0;

    std::set<int> rows;
    bool allRows = changes.empty();

    const auto depends = [](const ScriptDependencies& dependencies, const ScriptChange& change) {
        return dependencies.unclassified || dependencies.contains(change.source);
    };

    for(const auto& change : changes) {
        const bool inGroup
            = !change.track.isValid() || std::ranges::any_of(*m_expansionTracks, [&change](const Track& track) {
                  return track.sameIdentityAs(change.track);
              });

        if(change.source == ScriptSources::TrackMetadata && !inGroup) {
            continue;
        }
        if(depends(m_expansionDependencies.header, change)) {
            request.sections |= ExpansionHeader;
        }
        if(depends(m_expansionDependencies.footer, change)) {
            request.sections |= ExpansionFooter;
        }
        if(depends(m_expansionDependencies.grouping, change)) {
            request.sections |= ExpansionGrouping | ExpansionSubheaders;
        }
        if(depends(m_expansionDependencies.subheader, change)) {
            request.sections |= ExpansionSubheaders;
        }
        if(!depends(m_expansionDependencies.track, change)) {
            continue;
        }

        request.sections |= ExpansionTracks;

        const bool playbackRows
            = change.source == ScriptSources::PlaybackTrack || change.source == ScriptSources::PlaybackState
           || change.source == ScriptSources::PlaybackTime || change.source == ScriptSources::PlaybackOutput;
        if(m_expansionDependencies.track.unclassified || (!playbackRows && !change.track.isValid())) {
            allRows = true;
            continue;
        }

        for(int row{0}; const Track& track : *m_expansionTracks) {
            if((playbackRows
                && (track.sameIdentityAs(request.context.playback.track)
                    || track.sameIdentityAs(m_expansionContext.playback.track)))
               || (!playbackRows && track.sameIdentityAs(change.track))) {
                rows.emplace(row);
            }
            ++row;
        }
    }

    if(!allRows && rows.empty()) {
        request.sections &= ~ExpansionTracks;
    }
    if(!request.sections) {
        return;
    }
    if(!allRows) {
        request.rows.assign(rows.cbegin(), rows.cend());
    }

    submitExpansion(std::move(request));
}

void FilterWidget::closeExpansion(bool restoreFocus)
{
    m_view->cancelPendingCollapse();
    ++m_expansionRequest;
    m_expandedKey.clear();
    m_displayedKey.clear();
    m_expansionTracks.reset();
    if(m_expansionSubscription) {
        m_expansionSubscription->setDependencies({.unclassified = false});
    }
    m_view->setExpandedIndex({});
    if(m_expansion) {
        m_expansion->clear();
    }
    if(restoreFocus) {
        m_view->setFocus(Qt::OtherFocusReason);
    }
    restoreScrollPosition();
    scheduleVisibleCoverPinUpdate();
}

void FilterWidget::restoreScrollPosition()
{
    if(m_pendingScrollPosition >= 0) {
        const int scrollPosition = std::exchange(m_pendingScrollPosition, -1);
        QMetaObject::invokeMethod(
            m_view, [this, scrollPosition]() { m_view->verticalScrollBar()->setValue(scrollPosition); },
            Qt::QueuedConnection);
    }
}

void FilterWidget::addDisplayMenu(QMenu* menu)
{
    auto* displayMenu  = new QMenu(FilterWidget::tr("Display"), menu);
    auto* displayGroup = new QActionGroup(displayMenu);
    auto* coverGroup   = new QActionGroup(displayMenu);
    auto* sourceGroup  = new QActionGroup(displayMenu);

    auto* displayList      = new QAction(FilterWidget::tr("Columns"), displayGroup);
    auto* displayArtBottom = new QAction(FilterWidget::tr("Artwork (bottom labels)"), displayGroup);
    auto* displayArtLeft   = new QAction(FilterWidget::tr("Artwork (right labels)"), displayGroup);
    auto* displayArtNone   = new QAction(FilterWidget::tr("Artwork (no labels)"), displayGroup);
    auto* coverFront       = new QAction(FilterWidget::tr("Front cover"), coverGroup);
    auto* coverBack        = new QAction(FilterWidget::tr("Back cover"), coverGroup);
    auto* coverArtist      = new QAction(FilterWidget::tr("Artist"), coverGroup);
    auto* sourceEmbedded   = new QAction(FilterWidget::tr("Use embedded covers"), sourceGroup);
    auto* sourceDirectory  = new QAction(FilterWidget::tr("Use directory covers"), sourceGroup);
    auto* sourceDefault    = new QAction(FilterWidget::tr("Use default source"), sourceGroup);

    displayList->setCheckable(true);
    displayArtBottom->setCheckable(true);
    displayArtLeft->setCheckable(true);
    displayArtNone->setCheckable(true);
    coverFront->setCheckable(true);
    coverBack->setCheckable(true);
    coverArtist->setCheckable(true);
    sourceEmbedded->setCheckable(true);
    sourceDirectory->setCheckable(true);
    sourceDefault->setCheckable(true);

    const auto currentMode     = m_view->viewMode();
    const auto currentCaptions = m_view->captionDisplay();
    const auto currentType     = m_model->coverType();
    const auto currentSource   = m_model->coverSource();

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

    if(currentType == Track::Cover::Front) {
        coverFront->setChecked(true);
    }
    else if(currentType == Track::Cover::Back) {
        coverBack->setChecked(true);
    }
    else {
        coverArtist->setChecked(true);
    }

    if(!currentSource) {
        sourceDefault->setChecked(true);
    }
    else if(*currentSource == ArtworkSourcePreference::PreferEmbedded) {
        sourceEmbedded->setChecked(true);
    }
    else if(*currentSource == ArtworkSourcePreference::PreferDirectory) {
        sourceDirectory->setChecked(true);
    }

    QObject::connect(displayList, &QAction::triggered, this, [this]() {
        auto config{m_config};
        config.viewMode       = ViewMode::Tree;
        config.captionDisplay = CaptionDisplay::Bottom;
        applyConfig(config);
    });
    QObject::connect(displayArtBottom, &QAction::triggered, this, [this]() {
        auto config{m_config};
        config.viewMode       = ViewMode::Icon;
        config.captionDisplay = CaptionDisplay::Bottom;
        applyConfig(config);
    });
    QObject::connect(displayArtLeft, &QAction::triggered, this, [this]() {
        auto config{m_config};
        config.viewMode       = ViewMode::Icon;
        config.captionDisplay = CaptionDisplay::Right;
        applyConfig(config);
    });
    QObject::connect(displayArtNone, &QAction::triggered, this, [this]() {
        auto config{m_config};
        config.viewMode       = ViewMode::Icon;
        config.captionDisplay = CaptionDisplay::None;
        applyConfig(config);
    });
    QObject::connect(coverFront, &QAction::triggered, this, [this]() {
        auto config{m_config};
        config.coverType = Track::Cover::Front;
        applyConfig(config);
    });
    QObject::connect(coverBack, &QAction::triggered, this, [this]() {
        auto config{m_config};
        config.coverType = Track::Cover::Back;
        applyConfig(config);
    });
    QObject::connect(coverArtist, &QAction::triggered, this, [this]() {
        auto config{m_config};
        config.coverType = Track::Cover::Artist;
        applyConfig(config);
    });
    QObject::connect(sourceDefault, &QAction::triggered, this, [this]() {
        auto config{m_config};
        config.coverSource.reset();
        applyConfig(config);
    });
    QObject::connect(sourceEmbedded, &QAction::triggered, this, [this]() {
        auto config{m_config};
        config.coverSource = ArtworkSourcePreference::PreferEmbedded;
        applyConfig(config);
    });
    QObject::connect(sourceDirectory, &QAction::triggered, this, [this]() {
        auto config{m_config};
        config.coverSource = ArtworkSourcePreference::PreferDirectory;
        applyConfig(config);
    });

    auto* expandGroups = new QAction(tr("Expand selected group"), displayMenu);
    expandGroups->setCheckable(true);
    expandGroups->setChecked(m_config.expandGroups);
    QObject::connect(expandGroups, &QAction::toggled, this, [this](bool checked) {
        auto config         = m_config;
        config.expandGroups = checked;
        applyConfig(config);
        if(checked) {
            expandSelection();
        }
    });

    auto* displaySummary = new QAction(FilterWidget::tr("Summary item"), displayMenu);
    displaySummary->setCheckable(true);
    displaySummary->setChecked(m_model->showSummary());
    QObject::connect(displaySummary, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.showSummary = checked;
        applyConfig(config);
    });

    auto* showHeader = new QAction(FilterWidget::tr("Show header"), displayMenu);
    showHeader->setCheckable(true);
    showHeader->setChecked(!m_header->isCollapsed());
    QObject::connect(showHeader, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.showHeader = checked;
        applyConfig(config);
    });

    auto* showScrollbar = new QAction(FilterWidget::tr("Show scrollbar"), displayMenu);
    showScrollbar->setCheckable(true);
    showScrollbar->setChecked(m_config.showScrollbar);
    QObject::connect(showScrollbar, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.showScrollbar = checked;
        applyConfig(config);
    });

    auto* alternatingRows = new QAction(FilterWidget::tr("Alternating row colours"), displayMenu);
    alternatingRows->setCheckable(true);
    alternatingRows->setChecked(m_config.alternatingRows);
    QObject::connect(alternatingRows, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.alternatingRows = checked;
        applyConfig(config);
    });

    displayMenu->addActions(displayGroup->actions());
    displayMenu->addSeparator();
    displayMenu->addAction(expandGroups);
    displayMenu->addAction(displaySummary);
    displayMenu->addAction(showHeader);
    displayMenu->addAction(showScrollbar);
    displayMenu->addAction(alternatingRows);
    displayMenu->addSeparator();
    displayMenu->addActions(coverGroup->actions());
    displayMenu->addSeparator();
    displayMenu->addActions(sourceGroup->actions());

    menu->addMenu(displayMenu);
}

void FilterWidget::filterHeaderMenu(const QPoint& pos)
{
    Q_EMIT requestHeaderMenu(m_header, pos);
}

bool FilterWidget::hasColumn(int id) const
{
    return std::ranges::any_of(m_columns, [id](const FilterColumn& column) { return column.id == id; });
}

void FilterWidget::columnChanged(const FilterColumn& changedColumn)
{
    bool changed{false};

    for(FilterColumn& column : m_columns) {
        if(column.id == changedColumn.id) {
            column  = changedColumn;
            changed = true;
        }
    }

    if(changed) {
        Q_EMIT filterUpdated();
    }
}

void FilterWidget::columnRemoved(int id)
{
    auto columnIt = std::ranges::find_if(m_columns, [id](const FilterColumn& column) { return column.id == id; });
    if(columnIt == m_columns.end()) {
        return;
    }

    if(m_columns.size() == 1) {
        if(const auto replacement = m_columnRegistry->itemByIndex(0)) {
            *columnIt = replacement.value();
        }
    }
    else {
        m_columns.erase(columnIt);
    }

    Q_EMIT filterUpdated();
}

FilterWidget::ConfigData FilterWidget::configFromLayout(const QJsonObject& layout) const
{
    ConfigData config = defaultConfig();

    config.doubleClickAction = layout.value("DoubleClickAction"_L1).toInt(config.doubleClickAction);
    config.middleClickAction = layout.value("MiddleClickAction"_L1).toInt(config.middleClickAction);
    config.expansionDoubleClickAction
        = layout.value("ExpansionDoubleClickAction"_L1).toInt(config.expansionDoubleClickAction);
    config.expansionMiddleClickAction
        = layout.value("ExpansionMiddleClickAction"_L1).toInt(config.expansionMiddleClickAction);
    config.sendPlayback = layout.value("SendPlayback"_L1).toBool(config.sendPlayback);
    config.source = static_cast<FilterSource>(layout.value("FilterSource"_L1).toInt(static_cast<int>(config.source)));
    config.playlistEnabled          = layout.value("PlaylistEnabled"_L1).toBool(config.playlistEnabled);
    config.autoSwitch               = layout.value("AutoSwitch"_L1).toBool(config.autoSwitch);
    config.preservePlaybackPlaylist = layout.value("KeepAlive"_L1).toBool(config.preservePlaybackPlaylist);
    config.playlistName             = layout.value("PlaylistName"_L1).toString(config.playlistName);
    config.restoreState             = layout.value("RestoreState"_L1).toBool(config.restoreState);
    config.rowHeight                = layout.value("RowHeight"_L1).toInt(config.rowHeight);
    if(layout.contains("IconWidth"_L1) && layout.contains("IconHeight"_L1)) {
        config.iconSize = {layout.value("IconWidth"_L1).toInt(), layout.value("IconHeight"_L1).toInt()};
    }
    config.iconHorizontalGap   = layout.value("IconHorizontalGap"_L1).toInt(config.iconHorizontalGap);
    config.autoFitArtwork      = layout.value("AutoFitArtwork"_L1).toBool(config.autoFitArtwork);
    config.iconVerticalGap     = layout.value("IconVerticalGap"_L1).toInt(config.iconVerticalGap);
    config.artworkCornerRadius = layout.value("ArtworkCornerRadius"_L1).toInt(config.artworkCornerRadius);
    config.artworkMargins = artworkMarginsFromJson(layout.value("ArtworkMargins"_L1).toObject(), config.artworkMargins);
    config.expansionArtworkMargins
        = artworkMarginsFromJson(layout.value("ExpansionArtworkMargins"_L1).toObject(), {10, 10, 10, 10});
    config.expansionArtworkAutoMargins
        = layout.value("ExpansionArtworkAutoMargins"_L1).toBool(config.expansionArtworkAutoMargins);
    config.expansionArtworkReflection
        = layout.value("ExpansionArtworkReflection"_L1).toBool(config.expansionArtworkReflection);
    config.expansionArtworkReflectionHeight = std::clamp(
        layout.value("ExpansionArtworkReflectionHeight"_L1).toInt(config.expansionArtworkReflectionHeight), 1, 100);
    config.expansionArtworkReflectionOpacity = std::clamp(
        layout.value("ExpansionArtworkReflectionOpacity"_L1).toInt(config.expansionArtworkReflectionOpacity), 0, 100);
    config.expansionArtworkReflectionGap = std::clamp(
        layout.value("ExpansionArtworkReflectionGap"_L1).toInt(config.expansionArtworkReflectionGap), 0, 100);
    config.expansionShowCloseButton
        = layout.value("ExpansionShowCloseButton"_L1).toBool(config.expansionShowCloseButton);
    config.expansionAlternatingRows
        = layout.value("ExpansionAlternatingRows"_L1).toBool(config.expansionAlternatingRows);
    config.expansionArtworkOnLeft = layout.value("ExpansionArtworkOnLeft"_L1).toBool(config.expansionArtworkOnLeft);
    config.expansionArtworkSize
        = std::clamp(layout.value("ExpansionArtworkSize"_L1).toInt(config.expansionArtworkSize), 50, 200);
    config.expansionMaximumColumns
        = std::max(0, layout.value("ExpansionMaximumColumns"_L1).toInt(config.expansionMaximumColumns));
    config.expansionMinimumRows
        = std::max(1, layout.value("ExpansionMinimumRows"_L1).toInt(config.expansionMinimumRows));
    config.expansionStretchColumns = layout.value("ExpansionStretchColumns"_L1).toBool(config.expansionStretchColumns);
    config.expansionMinimumColumnWidth
        = std::clamp(layout.value("ExpansionMinimumColumnWidth"_L1).toInt(config.expansionMinimumColumnWidth), 25, 300);
    config.expansionScripts.load(layout.value("ExpansionScripts"_L1).toObject());
    config.expansionPlayingRowFont
        = layout.value("ExpansionPlayingRowFont"_L1).toString(config.expansionPlayingRowFont);
    config.expansionPlayingRowColour
        = layout.value("ExpansionPlayingRowColour"_L1).toString(config.expansionPlayingRowColour);
    config.expandGroups        = layout.value("ExpandGroups"_L1).toBool(config.expandGroups);
    config.expansionAnimated   = layout.value("ExpansionAnimated"_L1).toBool(config.expansionAnimated);
    config.expandedArtwork     = layout.value("ExpandedArtwork"_L1).toBool(config.expandedArtwork);
    config.rowHeight           = std::max(config.rowHeight, 0);
    config.iconHorizontalGap   = std::max(config.iconHorizontalGap, -1);
    config.iconVerticalGap     = std::max(config.iconVerticalGap, 0);
    config.artworkCornerRadius = std::max(config.artworkCornerRadius, 0);

    if(!config.iconSize.isValid()) {
        config.iconSize = factoryConfig().iconSize;
    }

    config.background.loadLayout(layout.value("Background"_L1).toObject());
    config.expansionBackground.loadLayout(layout.value("ExpansionBackground"_L1).toObject());
    config.expansionBackgroundColour
        = layout.value("ExpansionBackgroundColour"_L1).toString(config.expansionBackgroundColour);

    config.viewMode
        = static_cast<ExpandedTreeView::ViewMode>(layout.value("Display"_L1).toInt(static_cast<int>(config.viewMode)));
    config.captionDisplay = static_cast<ExpandedTreeView::CaptionDisplay>(
        layout.value("Captions"_L1).toInt(static_cast<int>(config.captionDisplay)));
    config.coverType = static_cast<Track::Cover>(layout.value("Artwork"_L1).toInt(static_cast<int>(config.coverType)));
    config.showSummary     = layout.value("ShowSummary"_L1).toBool(config.showSummary);
    config.showHeader      = layout.value("ShowHeader"_L1).toBool(config.showHeader);
    config.showScrollbar   = layout.value("ShowScrollbar"_L1).toBool(config.showScrollbar);
    config.alternatingRows = layout.value("AlternatingRows"_L1).toBool(config.alternatingRows);
    const int coverSource
        = layout.value("Source"_L1).toInt(config.coverSource ? static_cast<int>(*config.coverSource) : -1);
    config.coverSource
        = coverSource < 0 ? std::nullopt : std::optional{static_cast<ArtworkSourcePreference>(coverSource)};

    return config;
}

void FilterWidget::saveConfigToLayout(const ConfigData& config, QJsonObject& layout)
{
    layout["DoubleClickAction"_L1]                 = config.doubleClickAction;
    layout["MiddleClickAction"_L1]                 = config.middleClickAction;
    layout["ExpansionDoubleClickAction"_L1]        = config.expansionDoubleClickAction;
    layout["ExpansionMiddleClickAction"_L1]        = config.expansionMiddleClickAction;
    layout["SendPlayback"_L1]                      = config.sendPlayback;
    layout["FilterSource"_L1]                      = static_cast<int>(config.source);
    layout["PlaylistEnabled"_L1]                   = config.playlistEnabled;
    layout["AutoSwitch"_L1]                        = config.autoSwitch;
    layout["KeepAlive"_L1]                         = config.preservePlaybackPlaylist;
    layout["PlaylistName"_L1]                      = config.playlistName;
    layout["RestoreState"_L1]                      = config.restoreState;
    layout["RowHeight"_L1]                         = config.rowHeight;
    layout["IconWidth"_L1]                         = config.iconSize.width();
    layout["IconHeight"_L1]                        = config.iconSize.height();
    layout["IconHorizontalGap"_L1]                 = config.iconHorizontalGap;
    layout["AutoFitArtwork"_L1]                    = config.autoFitArtwork;
    layout["IconVerticalGap"_L1]                   = config.iconVerticalGap;
    layout["ArtworkCornerRadius"_L1]               = config.artworkCornerRadius;
    layout["ArtworkMargins"_L1]                    = artworkMarginsToJson(config.artworkMargins);
    layout["ExpandGroups"_L1]                      = config.expandGroups;
    layout["ExpansionAnimated"_L1]                 = config.expansionAnimated;
    layout["ExpansionAlternatingRows"_L1]          = config.expansionAlternatingRows;
    layout["ExpandedArtwork"_L1]                   = config.expandedArtwork;
    layout["ExpansionArtworkMargins"_L1]           = artworkMarginsToJson(config.expansionArtworkMargins);
    layout["ExpansionArtworkAutoMargins"_L1]       = config.expansionArtworkAutoMargins;
    layout["ExpansionArtworkReflection"_L1]        = config.expansionArtworkReflection;
    layout["ExpansionArtworkReflectionHeight"_L1]  = config.expansionArtworkReflectionHeight;
    layout["ExpansionArtworkReflectionOpacity"_L1] = config.expansionArtworkReflectionOpacity;
    layout["ExpansionArtworkReflectionGap"_L1]     = config.expansionArtworkReflectionGap;
    layout["ExpansionShowCloseButton"_L1]          = config.expansionShowCloseButton;
    layout["ExpansionArtworkOnLeft"_L1]            = config.expansionArtworkOnLeft;
    layout["ExpansionArtworkSize"_L1]              = config.expansionArtworkSize;
    layout["ExpansionMaximumColumns"_L1]           = config.expansionMaximumColumns;
    layout["ExpansionMinimumRows"_L1]              = config.expansionMinimumRows;
    layout["ExpansionStretchColumns"_L1]           = config.expansionStretchColumns;
    layout["ExpansionMinimumColumnWidth"_L1]       = config.expansionMinimumColumnWidth;
    layout["ExpansionScripts"_L1]                  = config.expansionScripts.toJson();
    layout["ExpansionPlayingRowFont"_L1]           = config.expansionPlayingRowFont;
    layout["ExpansionPlayingRowColour"_L1]         = config.expansionPlayingRowColour;
    QJsonObject background;
    config.background.saveLayout(background);
    layout["Background"_L1] = background;
    QJsonObject expansionBackground;
    config.expansionBackground.saveLayout(expansionBackground);
    layout["ExpansionBackground"_L1]       = expansionBackground;
    layout["ExpansionBackgroundColour"_L1] = config.expansionBackgroundColour;
    layout["Display"_L1]                   = static_cast<int>(config.viewMode);
    layout["Captions"_L1]                  = static_cast<int>(config.captionDisplay);
    layout["Artwork"_L1]                   = static_cast<int>(config.coverType);
    layout["ShowSummary"_L1]               = config.showSummary;
    layout["ShowHeader"_L1]                = config.showHeader;
    layout["ShowScrollbar"_L1]             = config.showScrollbar;
    layout["AlternatingRows"_L1]           = config.alternatingRows;
    layout["Source"_L1]                    = config.coverSource ? static_cast<int>(*config.coverSource) : -1;
}
} // namespace Fooyin::Filters

#include "moc_filterwidget.cpp"
