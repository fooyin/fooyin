/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "radiobrowserwidget.h"

#include "radiobrowserconfigdialog.h"
#include "radiobrowsercontextmenu.h"
#include "radiobrowsercontroller.h"
#include "radiobrowsermodel.h"
#include "radiosearch.h"
#include "radiostationdelegate.h"
#include "radiostationdialog.h"
#include "radiostationimportexportdialog.h"
#include "radiostationview.h"

#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QDesktopServices>
#include <QDialog>
#include <QGridLayout>
#include <QGuiApplication>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <gui/configdialog.h>
#include <gui/contextmenuutils.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/autoheaderview.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/actions/widgetcontext.h>
#include <utils/settings/settingsmanager.h>
#include <utils/signalthrottler.h>
#include <utils/utils.h>

#include <algorithm>

using namespace Qt::StringLiterals;

constexpr auto ToggleFilterBarAction = "RadioBrowser.ToggleFilterBar";
constexpr auto SaveStationAction     = "RadioBrowser.SaveStation";

constexpr auto HideBrokenKey        = "RadioBrowser/HideBroken";
constexpr auto DoubleClickActionKey = "RadioBrowser/DoubleClickAction";
constexpr auto MiddleClickActionKey = "RadioBrowser/MiddleClickAction";
constexpr auto PlaybackOnSendKey    = "RadioBrowser/PlaybackOnSend";
constexpr auto SendClicksKey        = "RadioBrowser/SendClicks";
constexpr auto RowHeightKey         = "RadioBrowser/RowHeight";
constexpr auto IconSizeKey          = "RadioBrowser/IconSize";
constexpr auto IconHorizontalGapKey = "RadioBrowser/IconHorizontalGap";
constexpr auto IconVerticalGapKey   = "RadioBrowser/IconVerticalGap";
constexpr auto IconItemBorderKey    = "RadioBrowser/IconItemBorder";
constexpr auto UniformIconsKey      = "RadioBrowser/UniformIcons";
constexpr auto ShowIconsKey         = "RadioBrowser/ShowIcons";
constexpr auto ShowSavedIconsKey    = "RadioBrowser/ShowSavedIcons";
constexpr auto ShowToolTipsKey      = "RadioBrowser/ShowToolTips";

namespace Fooyin::RadioBrowser {
namespace {
bool hasStationFilters(const RadioSearchRequest& request)
{
    return !request.text.trimmed().isEmpty() || !request.countryCode.trimmed().isEmpty()
        || !request.language.trimmed().isEmpty() || !request.tag.trimmed().isEmpty()
        || !request.codec.trimmed().isEmpty() || request.bitrateMin > 0 || request.bitrateMax > 0;
}

QString apiOrderForColumn(int column)
{
    switch(column) {
        case Station:
            return u"name"_s;
        case Country:
            return u"country"_s;
        case Tags:
            return u"tags"_s;
        case Language:
            return u"language"_s;
        case Codec:
            return u"codec"_s;
        case Bitrate:
            return u"bitrate"_s;
        case Votes:
            return u"votes"_s;
        case Clicks:
            return u"clickcount"_s;
        default:
            break;
    }

    return {};
}

QString normalisedApiOrder(const QString& order)
{
    const QString normalisedOrder = order.trimmed();
    return normalisedOrder.isEmpty() ? u"clickcount"_s : normalisedOrder;
}

int columnForApiOrder(const QString& order)
{
    const QString normalisedOrder = normalisedApiOrder(order);
    if(normalisedOrder == "name"_L1) {
        return Station;
    }
    if(normalisedOrder == "country"_L1) {
        return Country;
    }
    if(normalisedOrder == "tags"_L1) {
        return Tags;
    }
    if(normalisedOrder == "language"_L1) {
        return Language;
    }
    if(normalisedOrder == "codec"_L1) {
        return Codec;
    }
    if(normalisedOrder == "bitrate"_L1) {
        return Bitrate;
    }
    if(normalisedOrder == "votes"_L1) {
        return Votes;
    }
    if(normalisedOrder == "clickcount"_L1) {
        return Clicks;
    }

    return -1;
}

QUrl radioBrowserStationUrl(const RadioStation& station)
{
    if(station.uuid.trimmed().isEmpty()) {
        return {};
    }
    return QUrl{u"https://www.radio-browser.info/history/%1"_s.arg(station.uuid.trimmed())};
}
} // namespace

RadioBrowserWidget::RadioBrowserWidget(RadioBrowserController* controller, ActionManager* actionManager,
                                       TrackSelectionController* trackSelection, SettingsManager* settings,
                                       bool applyInitialSearch, QWidget* parent)
    : FyWidget{parent}
    , m_controller{controller}
    , m_actionManager{actionManager}
    , m_trackSelection{trackSelection}
    , m_settings{settings}
    , m_context{new WidgetContext(
          this, Context{IdList{Constants::Context::TrackSelection, Id{"Fooyin.Context.RadioBrowser."}.append(id())}},
          this)}
    , m_toggleFilterBarAction{new QAction(tr("Show search bar"), this)}
    , m_saveStationsAction{new QAction(tr("Add to My Stations"), this)}
    , m_removeStationsAction{new QAction(tr("Remove from My Stations"), this)}
    , m_saveStationsCmd{nullptr}
    , m_removeStationsCmd{nullptr}
    , m_filterThrottler{new SignalThrottler(this)}
    , m_resultsView{new RadioStationView(this)}
    , m_delegate{new RadioStationDelegate(this)}
    , m_model{new RadioBrowserModel(this)}
    , m_doubleClickAction{5}
    , m_middleClickAction{0}
    , m_playbackOnSend{true}
    , m_hideBroken{true}
    , m_sendClicks{true}
    , m_loadingLayout{false}
    , m_initialSearchState{applyInitialSearch ? InitialSearchState::Pending : InitialSearchState::Disabled}
    , m_filterBarMode{FilterBarMode::Toggleable}
    , m_visibleIconRequestPending{false}
{
    setObjectName(RadioBrowserWidget::name());
    setFeature(Search);
    m_actionManager->addContextObject(m_context);

    m_sendClicks = m_settings->fileValue(SendClicksKey, m_sendClicks).toBool();
    m_controller->setSendClicks(m_sendClicks);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins({});
    mainLayout->setSpacing(6);

    m_resultsView->setModel(m_model);
    m_resultsView->setItemDelegate(m_delegate);
    m_model->setIconProvider(m_controller->iconProvider());
    m_model->setSavedStations(m_controller->savedStations());
    m_resultsView->setDragDropMode(QAbstractItemView::DragOnly);
    m_resultsView->setDefaultDropAction(Qt::CopyAction);
    m_resultsView->setDragDropOverwriteMode(false);
    m_resultsView->setDropIndicatorShown(true);
    m_resultsView->setEmptyText(tr("No stations found"));
    m_resultsView->setLoadingText(tr("Loading stations…"));

    Context actionContext{m_context->context()};
    actionContext.erase(Constants::Context::TrackSelection);
    const QStringList radioBrowserCategory{tr("Radio Browser")};

    m_saveStationsAction->setEnabled(false);
    m_saveStationsAction->setStatusTip(tr("Add the selected stations to My Stations"));
    m_saveStationsCmd = m_actionManager->registerAction(m_saveStationsAction, SaveStationAction, actionContext);
    m_saveStationsCmd->setCategories(radioBrowserCategory);
    QObject::connect(m_saveStationsAction, &QAction::triggered, this, &RadioBrowserWidget::saveSelectedStation);

    m_removeStationsAction->setEnabled(false);
    m_removeStationsAction->setStatusTip(tr("Remove the selected stations from My Stations"));
    m_removeStationsCmd
        = m_actionManager->registerAction(m_removeStationsAction, Constants::Actions::Remove, actionContext);
    m_removeStationsCmd->setDescription(tr("Remove"));
    m_removeStationsCmd->setAttribute(ProxyAction::UpdateText);
    m_removeStationsCmd->setDefaultShortcut(QKeySequence::Delete);
    QObject::connect(m_removeStationsAction, &QAction::triggered, this, &RadioBrowserWidget::removeSelectedStations);

    m_toggleFilterBarAction->setCheckable(true);
    m_toggleFilterBarAction->setChecked(true);
    m_toggleFilterBarAction->setStatusTip(tr("Show or hide the Radio Browser filter bar"));
    auto* toggleFilterBarCmd
        = m_actionManager->registerAction(m_toggleFilterBarAction, ToggleFilterBarAction, actionContext);
    toggleFilterBarCmd->setCategories(radioBrowserCategory);
    QObject::connect(m_toggleFilterBarAction, &QAction::triggered, this, &RadioBrowserWidget::setFilterBarVisible);

    mainLayout->addWidget(m_resultsView, 1);

    m_filterThrottler->setTimeout(350);

    QObject::connect(m_resultsView, &RadioStationView::doubleClicked, this, &RadioBrowserWidget::handleDoubleClick);
    QObject::connect(m_resultsView, &RadioStationView::middleClicked, this, &RadioBrowserWidget::handleMiddleClick);
    QObject::connect(m_resultsView, &QWidget::customContextMenuRequested, this, &RadioBrowserWidget::showContextMenu);
    QObject::connect(m_resultsView->stationHeader(), &QWidget::customContextMenuRequested, this,
                     &RadioBrowserWidget::showHeaderContextMenu);
    QObject::connect(m_resultsView->stationHeader(), &QHeaderView::sectionCountChanged, this,
                     &RadioBrowserWidget::updateIconColumnOrder);
    QObject::connect(m_resultsView->stationHeader(), &AutoHeaderView::sectionVisiblityChanged, this,
                     &RadioBrowserWidget::updateIconColumnOrder);
    QObject::connect(m_resultsView->stationHeader(), &QHeaderView::sectionMoved, this,
                     &RadioBrowserWidget::updateIconColumnOrder);
    QObject::connect(m_resultsView, &ExpandedTreeView::viewModeChanged, this,
                     &RadioBrowserWidget::updateIconColumnOrder);
    QObject::connect(m_resultsView->verticalScrollBar(), &QScrollBar::valueChanged, this,
                     &RadioBrowserWidget::scheduleVisibleIconRequest);
    QObject::connect(m_resultsView->verticalScrollBar(), &QScrollBar::valueChanged, this,
                     &RadioBrowserWidget::maybeLoadMoreStations);
    QObject::connect(m_resultsView, &QAbstractItemView::iconSizeChanged, this,
                     &RadioBrowserWidget::handleIconSizeChanged);
    QObject::connect(m_resultsView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        updateSelectedTracks();
        updateActionState();
    });
    QObject::connect(m_model, &QAbstractItemModel::modelReset, this, &RadioBrowserWidget::scheduleVisibleIconRequest);
    QObject::connect(m_model, &RadioBrowserModel::sortRequested, this, &RadioBrowserWidget::handleSortRequested);
    QObject::connect(m_model, &RadioBrowserModel::stationsReordered, this,
                     &RadioBrowserWidget::handleStationsReordered);

    QObject::connect(m_filterThrottler, &SignalThrottler::triggered, this,
                     qOverload<>(&RadioBrowserWidget::applyFilterSearch));

    QObject::connect(m_controller, &RadioBrowserController::searchStarted, this,
                     [this]() { m_resultsView->setLoading(true); });
    QObject::connect(m_controller, &RadioBrowserController::stationsChanged, this,
                     &RadioBrowserWidget::handleStationsChanged);
    QObject::connect(m_controller, &RadioBrowserController::savedStationsBrowsingChanged, this,
                     &RadioBrowserWidget::handleSavedStationsBrowsingChanged);
    QObject::connect(m_controller, &RadioBrowserController::categoriesChanged, this,
                     &RadioBrowserWidget::handleCategoriesChanged);
    QObject::connect(m_controller, &RadioBrowserController::savedSearchesChanged, this,
                     &RadioBrowserWidget::updateSavedSearchState);
    QObject::connect(m_controller, &RadioBrowserController::savedStationsChanged, m_model,
                     &RadioBrowserModel::setSavedStations);
    QObject::connect(m_controller, &RadioBrowserController::searchRequestActivated, this,
                     &RadioBrowserWidget::handleSearchRequestActivated);
    QObject::connect(m_controller, &RadioBrowserController::searchFailed, this,
                     &RadioBrowserWidget::handleSearchFailed);
    QObject::connect(m_controller, &RadioBrowserController::stationActionFailed, this,
                     &RadioBrowserWidget::handleStationActionFailed);
    QObject::connect(m_controller, &RadioBrowserController::stationsResolved, this,
                     &RadioBrowserWidget::handleResolvedTracks);
    QObject::connect(m_controller, &RadioBrowserController::currentStationChanged, m_model,
                     &RadioBrowserModel::setCurrentStation);

    const auto handleThemeChange = [this]() {
        refreshThemeIcons();
        m_model->updateColours();
    };
    m_settings->subscribe<Settings::Gui::IconTheme>(this, handleThemeChange);
    m_settings->subscribe<Settings::Gui::ResolvedAppStyle>(this, handleThemeChange);

    const QScopedValueRollback loadingLayout{m_loadingLayout, true};
    applyConfig(defaultConfig());

    refreshThemeIcons();
    m_controller->fetchCategories(RadioCategoryType::Country);
    m_controller->fetchCategories(RadioCategoryType::Tag);
    m_controller->fetchCategories(RadioCategoryType::Codec);

    m_model->setCurrentStation(m_controller->currentStation());
}

RadioBrowserWidget::~RadioBrowserWidget()
{
    const auto menus = findChildren<QMenu*>();
    for(QMenu* menu : menus) {
        menu->close();
    }
}

QString RadioBrowserWidget::name() const
{
    return tr("Radio Browser");
}

QString RadioBrowserWidget::layoutName() const
{
    return u"RadioBrowser"_s;
}

void RadioBrowserWidget::saveLayoutData(QJsonObject& layout)
{
    layout["Display"_L1]           = static_cast<int>(m_viewConfig.viewMode);
    layout["Captions"_L1]          = static_cast<int>(m_viewConfig.captions);
    layout["ShowHeader"_L1]        = m_viewConfig.showHeader;
    layout["ShowScrollbar"_L1]     = m_viewConfig.showScrollbar;
    layout["AlternatingRows"_L1]   = m_viewConfig.alternatingRows;
    layout["ShowIcons"_L1]         = m_viewConfig.showIcons;
    layout["ShowSavedIcons"_L1]    = m_viewConfig.showSavedStationIcons;
    layout["ShowToolTips"_L1]      = m_viewConfig.showToolTips;
    layout["RowHeight"_L1]         = m_viewConfig.rowHeight;
    layout["IconSize"_L1]          = m_viewConfig.iconSize.width();
    layout["IconHorizontalGap"_L1] = m_viewConfig.iconHorizontalGap;
    layout["IconVerticalGap"_L1]   = m_viewConfig.iconVerticalGap;
    layout["IconItemBorder"_L1]    = m_viewConfig.iconItemBorderWidth;
    layout["UniformIcons"_L1]      = m_viewConfig.uniformStationIcons;
    layout["DoubleClickAction"_L1] = m_doubleClickAction;
    layout["MiddleClickAction"_L1] = m_middleClickAction;
    layout["PlaybackOnSend"_L1]    = m_playbackOnSend;
    layout["HideBroken"_L1]        = m_hideBroken;

    QByteArray state   = m_resultsView->stationHeader()->saveHeaderState();
    state              = qCompress(state, 9);
    layout["State"_L1] = QString::fromUtf8(state.toBase64());

    layout.insert(u"ShowFilterBar"_s, m_toggleFilterBarAction->isChecked());
}

void RadioBrowserWidget::loadLayoutData(const QJsonObject& layout)
{
    const QScopedValueRollback loadingLayout{m_loadingLayout, true};

    if(layout.contains("Display"_L1)) {
        m_viewConfig.viewMode = static_cast<ExpandedTreeView::ViewMode>(layout.value("Display"_L1).toInt());
    }
    if(layout.contains("Captions"_L1)) {
        m_viewConfig.captions = static_cast<ExpandedTreeView::CaptionDisplay>(layout.value("Captions"_L1).toInt());
    }
    if(layout.contains("ShowHeader"_L1)) {
        m_viewConfig.showHeader = layout.value("ShowHeader"_L1).toBool();
    }
    if(layout.contains("ShowScrollbar"_L1)) {
        m_viewConfig.showScrollbar = layout.value("ShowScrollbar"_L1).toBool();
    }
    if(layout.contains("AlternatingRows"_L1)) {
        m_viewConfig.alternatingRows = layout.value("AlternatingRows"_L1).toBool();
    }
    if(layout.contains("ShowIcons"_L1)) {
        m_viewConfig.showIcons = layout.value("ShowIcons"_L1).toBool();
    }
    if(layout.contains("ShowSavedIcons"_L1)) {
        m_viewConfig.showSavedStationIcons = layout.value("ShowSavedIcons"_L1).toBool();
    }
    if(layout.contains("ShowToolTips"_L1)) {
        m_viewConfig.showToolTips = layout.value("ShowToolTips"_L1).toBool();
    }
    if(layout.contains("RowHeight"_L1)) {
        m_viewConfig.rowHeight = layout.value("RowHeight"_L1).toInt();
    }
    if(layout.contains("IconSize"_L1)) {
        const int iconSize    = layout.value("IconSize"_L1).toInt();
        m_viewConfig.iconSize = {iconSize, iconSize};
    }
    if(layout.contains("IconHorizontalGap"_L1)) {
        m_viewConfig.iconHorizontalGap = layout.value("IconHorizontalGap"_L1).toInt();
    }
    if(layout.contains("IconVerticalGap"_L1)) {
        m_viewConfig.iconVerticalGap = layout.value("IconVerticalGap"_L1).toInt();
    }
    if(layout.contains("IconItemBorder"_L1)) {
        m_viewConfig.iconItemBorderWidth = layout.value("IconItemBorder"_L1).toInt();
    }
    if(layout.contains("UniformIcons"_L1)) {
        m_viewConfig.uniformStationIcons = layout.value("UniformIcons"_L1).toBool();
    }

    setViewConfig(m_viewConfig);

    m_doubleClickAction = layout.value("DoubleClickAction"_L1).toInt(m_doubleClickAction);
    m_middleClickAction = layout.value("MiddleClickAction"_L1).toInt(m_middleClickAction);
    m_playbackOnSend    = layout.value("PlaybackOnSend"_L1).toBool(m_playbackOnSend);
    m_hideBroken        = layout.value("HideBroken"_L1).toBool(m_hideBroken);
    m_controller->setHideBroken(m_hideBroken);

    m_config = {
        .view              = m_viewConfig,
        .doubleClickAction = m_doubleClickAction,
        .middleClickAction = m_middleClickAction,
        .playbackOnSend    = m_playbackOnSend,
        .hideBroken        = m_hideBroken,
    };

    if(layout.contains("State"_L1)) {
        const auto headerState = layout.value("State"_L1).toString().toUtf8();
        if(!headerState.isEmpty() && headerState.isValidUtf8()) {
            m_headerState = qUncompress(QByteArray::fromBase64(headerState));
        }
    }

    setFilterBarVisible(layout.value("ShowFilterBar"_L1).toBool(true));
    updateSavedSearchState();
}

void RadioBrowserWidget::finalise()
{
    m_resultsView->finaliseView(m_headerState);
    updateIconColumnOrder();

    if(m_initialSearchState == InitialSearchState::Disabled && syncControllerBrowseState()) {
        return;
    }

    if(m_initialSearchState == InitialSearchState::Pending) {
        m_resultsView->setLoading(true);
        QTimer::singleShot(0, this, &RadioBrowserWidget::startInitialSearch);
    }
}

void RadioBrowserWidget::searchEvent(const SearchRequest& request)
{
    if(currentFilterRequest().text == request.text) {
        applyFilterSearch();
        return;
    }

    if(m_filterBar) {
        m_filterBar->setSearchText(request.text);
        m_filterRequest = currentFilterRequest();
    }
    else {
        m_filterRequest.text = request.text;
    }

    applyFilterSearch();
}

void RadioBrowserWidget::browseSavedStations()
{
    m_controller->browseSavedStations();
}

void RadioBrowserWidget::showFilterBar()
{
    setFilterBarVisible(true);
}

void RadioBrowserWidget::hideFilterBar()
{
    setFilterBarVisible(false);
}

void RadioBrowserWidget::setFilterBarAllowed(bool allowed)
{
    if(allowed) {
        if(m_filterBarMode == FilterBarMode::Hidden) {
            m_filterBarMode = FilterBarMode::Toggleable;
        }
    }
    else {
        m_filterBarMode = FilterBarMode::Hidden;
    }

    if(!allowed) {
        disconnectFilterBar();
    }

    updateFilterBarActionState();

    if(!allowed) {
        setFilterBarVisible(false);
    }
}

void RadioBrowserWidget::setFilterBarToggleAllowed(bool allowed)
{
    if(m_filterBarMode != FilterBarMode::Hidden) {
        m_filterBarMode = allowed ? FilterBarMode::Toggleable : FilterBarMode::Fixed;
    }

    updateFilterBarActionState();
    if(!allowed && m_filterBarMode != FilterBarMode::Hidden) {
        setFilterBarVisible(true);
    }
}

void RadioBrowserWidget::setApplySearchOnLoad(bool enabled)
{
    if(enabled) {
        if(m_initialSearchState == InitialSearchState::Disabled) {
            m_initialSearchState = InitialSearchState::Pending;
        }
        return;
    }

    m_initialSearchState = InitialSearchState::Disabled;
    if(!syncControllerBrowseState()) {
        m_resultsView->setLoading(false);
    }
}

void RadioBrowserWidget::connectFilterBar(RadioSearch* filterBar)
{
    if(m_filterBarMode == FilterBarMode::Hidden) {
        disconnectFilterBar();
        updateFilterBarActionState();
        return;
    }

    if(m_filterBar && m_filterBar == filterBar) {
        updateFilterBarCategories();
        return;
    }

    disconnectFilterBar();
    m_filterBar = filterBar;

    if(!m_filterBar) {
        updateFilterBarActionState();
        updateSavedSearchState();
        return;
    }

    QObject::connect(m_filterBar, &RadioSearch::filterChanged, this, &RadioBrowserWidget::handleFilterChanged);
    QObject::connect(m_filterBar, &RadioSearch::saveSearchTriggered, this,
                     &RadioBrowserWidget::handleSaveSearchTriggered);
    QObject::connect(m_filterBar, &RadioSearch::resetTriggered, this, &RadioBrowserWidget::resetFilters);

    updateFilterBarCategories();
    m_filterRequest = currentFilterRequest();
    updateFilterBarActionState();
    setFilterBarVisible(m_filterBarMode == FilterBarMode::Toggleable ? m_toggleFilterBarAction->isChecked()
                                                                     : m_filterBarMode == FilterBarMode::Fixed);
    refreshThemeIcons();
}

RadioBrowserWidget::ConfigData RadioBrowserWidget::factoryConfig() const
{
    return {};
}

RadioBrowserWidget::ConfigData RadioBrowserWidget::defaultConfig() const
{
    auto config{factoryConfig()};

    config.doubleClickAction      = m_settings->fileValue(DoubleClickActionKey, config.doubleClickAction).toInt();
    config.middleClickAction      = m_settings->fileValue(MiddleClickActionKey, config.middleClickAction).toInt();
    config.playbackOnSend         = m_settings->fileValue(PlaybackOnSendKey, config.playbackOnSend).toBool();
    config.hideBroken             = m_settings->fileValue(HideBrokenKey, config.hideBroken).toBool();
    config.view.rowHeight         = m_settings->fileValue(RowHeightKey, config.view.rowHeight).toInt();
    config.view.iconSize          = m_settings->fileValue(IconSizeKey, config.view.iconSize).toSize();
    config.view.iconHorizontalGap = m_settings->fileValue(IconHorizontalGapKey, config.view.iconHorizontalGap).toInt();
    config.view.iconVerticalGap   = m_settings->fileValue(IconVerticalGapKey, config.view.iconVerticalGap).toInt();
    config.view.iconItemBorderWidth = m_settings->fileValue(IconItemBorderKey, config.view.iconItemBorderWidth).toInt();
    config.view.uniformStationIcons = m_settings->fileValue(UniformIconsKey, config.view.uniformStationIcons).toBool();
    config.view.showIcons           = m_settings->fileValue(ShowIconsKey, config.view.showIcons).toBool();
    config.view.showSavedStationIcons
        = m_settings->fileValue(ShowSavedIconsKey, config.view.showSavedStationIcons).toBool();
    config.view.showToolTips = m_settings->fileValue(ShowToolTipsKey, config.view.showToolTips).toBool();

    if(!config.view.iconSize.isValid()) {
        config.view.iconSize = factoryConfig().view.iconSize;
    }

    return config;
}

const RadioBrowserWidget::ConfigData& RadioBrowserWidget::currentConfig() const
{
    return m_config;
}

void RadioBrowserWidget::saveDefaults(const ConfigData& config) const
{
    m_settings->fileSet(DoubleClickActionKey, config.doubleClickAction);
    m_settings->fileSet(MiddleClickActionKey, config.middleClickAction);
    m_settings->fileSet(PlaybackOnSendKey, config.playbackOnSend);
    m_settings->fileSet(HideBrokenKey, config.hideBroken);
    m_settings->fileSet(RowHeightKey, config.view.rowHeight);
    m_settings->fileSet(IconSizeKey, config.view.iconSize);
    m_settings->fileSet(IconHorizontalGapKey, config.view.iconHorizontalGap);
    m_settings->fileSet(IconVerticalGapKey, config.view.iconVerticalGap);
    m_settings->fileSet(IconItemBorderKey, config.view.iconItemBorderWidth);
    m_settings->fileSet(UniformIconsKey, config.view.uniformStationIcons);
    m_settings->fileSet(ShowIconsKey, config.view.showIcons);
    m_settings->fileSet(ShowSavedIconsKey, config.view.showSavedStationIcons);
    m_settings->fileSet(ShowToolTipsKey, config.view.showToolTips);
}

void RadioBrowserWidget::clearSavedDefaults() const
{
    m_settings->fileRemove(DoubleClickActionKey);
    m_settings->fileRemove(MiddleClickActionKey);
    m_settings->fileRemove(PlaybackOnSendKey);
    m_settings->fileRemove(HideBrokenKey);
    m_settings->fileRemove(RowHeightKey);
    m_settings->fileRemove(IconSizeKey);
    m_settings->fileRemove(IconHorizontalGapKey);
    m_settings->fileRemove(IconVerticalGapKey);
    m_settings->fileRemove(IconItemBorderKey);
    m_settings->fileRemove(UniformIconsKey);
    m_settings->fileRemove(ShowIconsKey);
    m_settings->fileRemove(ShowSavedIconsKey);
    m_settings->fileRemove(ShowToolTipsKey);
}

void RadioBrowserWidget::applyConfig(const ConfigData& config)
{
    const bool searchConfigChanged = m_hideBroken != config.hideBroken;

    m_config            = config;
    m_doubleClickAction = config.doubleClickAction;
    m_middleClickAction = config.middleClickAction;
    m_playbackOnSend    = config.playbackOnSend;
    m_hideBroken        = config.hideBroken;
    m_controller->setHideBroken(m_hideBroken);

    setViewConfig(config.view);

    if(searchConfigChanged && !m_loadingLayout) {
        applyFilterSearch();
    }
}

bool RadioBrowserWidget::sendClicks() const
{
    return m_sendClicks;
}

void RadioBrowserWidget::setSendClicks(const bool enabled)
{
    m_sendClicks = enabled;
    m_settings->fileSet(SendClicksKey, enabled);
    m_controller->setSendClicks(enabled);
}

void RadioBrowserWidget::showEvent(QShowEvent* event)
{
    FyWidget::showEvent(event);
    scheduleVisibleIconRequest();
}

void RadioBrowserWidget::openConfigDialog()
{
    showConfigDialog(new RadioBrowserConfigDialog(this, this));
}

void RadioBrowserWidget::handleIconSizeChanged(const QSize& size)
{
    if(size.isValid() && m_viewConfig.iconSize != size) {
        auto config     = m_viewConfig;
        config.iconSize = size;
        setViewConfig(config);
    }
}

void RadioBrowserWidget::handleStationsReordered(const RadioStationList& stations)
{
    if(m_model->reorderEnabled()) {
        m_controller->reorderSavedStations(stations);
    }
}

void RadioBrowserWidget::handleStationsChanged(const RadioStationList& stations, bool reset)
{
    m_resultsView->setLoading(false);

    if(reset) {
        m_model->setStations(stations);
        m_resultsView->scrollToTop();
    }
    else {
        m_model->appendStations(stations);
    }

    updateSelectedTracks();
    updateActionState();
    scheduleVisibleIconRequest();
}

void RadioBrowserWidget::handleSavedStationsBrowsingChanged(bool browsing)
{
    m_initialSearchState = InitialSearchState::Complete;
    m_resultsView->setLoading(false);
    m_resultsView->setEmptyText(browsing ? tr("No saved stations") : tr("No stations found"));

    if(browsing) {
        RadioSearchRequest request;
        request.order      = u"clickcount"_s;
        request.hideBroken = m_hideBroken;
        setFilterRequest(request);
    }

    m_model->setReorderEnabled(browsing);
    m_model->setShowSavedIndicators(m_viewConfig.showSavedStationIcons && !browsing);
    if(browsing) {
        m_model->setApiSortingEnabled(false);
        m_resultsView->setSortingEnabled(false);
    }
    m_resultsView->setDragDropMode(browsing ? QAbstractItemView::DragDrop : QAbstractItemView::DragOnly);
    m_resultsView->setDefaultDropAction(browsing ? Qt::MoveAction : Qt::CopyAction);
    updateActionState();
}

void RadioBrowserWidget::handleCategoriesChanged(RadioCategoryType type, const RadioCategoryList& categories)
{
    if(type == RadioCategoryType::Country) {
        m_countryCategories = categories;
    }
    else if(type == RadioCategoryType::Tag) {
        m_tagCategories = categories;
    }
    else if(type == RadioCategoryType::Codec) {
        m_codecCategories = categories;
    }
    else {
        return;
    }

    updateFilterBarCategories();
    updateSavedSearchState();
}

void RadioBrowserWidget::handleSearchRequestActivated(const RadioSearchRequest& request)
{
    m_initialSearchState = InitialSearchState::Complete;
    setFilterRequest(request);
    updateApiSortingState(request);
}

void RadioBrowserWidget::handleSortRequested(const int column, const Qt::SortOrder order)
{
    const QString apiOrder = apiOrderForColumn(column);
    if(apiOrder.isEmpty()) {
        return;
    }

    const bool reverse = order == Qt::DescendingOrder;
    if(normalisedApiOrder(m_filterRequest.order) == apiOrder && m_filterRequest.reverse == reverse) {
        return;
    }

    RadioSearchRequest request{m_filterRequest};
    request.order   = apiOrder;
    request.reverse = reverse;
    request.offset  = 0;

    setFilterRequest(request);
    m_controller->sortCurrentStations(apiOrder, reverse);
}

void RadioBrowserWidget::handleSearchFailed()
{
    m_resultsView->setLoading(false);
    m_resultsView->setEmptyText(tr("No stations found"));
}

void RadioBrowserWidget::handleStationActionFailed(QObject* context, const RadioStation& station, const QString& error)
{
    if(context != this) {
        return;
    }

    QMessageBox::warning(this, tr("Radio Browser"), u"%1\n\n%2"_s.arg(station.name, error));
}

void RadioBrowserWidget::handleFilterChanged()
{
    if(!m_filterBar) {
        return;
    }

    m_filterRequest = currentFilterRequest();
    scheduleFilterSearch();
}

void RadioBrowserWidget::handleSaveSearchTriggered()
{
    if(m_currentSavedSearch) {
        showSavedSearchMenu();
    }
    else {
        saveCurrentSearch();
    }
}

void RadioBrowserWidget::updateActionState()
{
    const RadioStationList stations = selectedStations();
    const bool canSave              = std::ranges::any_of(
        stations, [this](const RadioStation& station) { return !m_controller->isSaved(station); });
    const bool canRemove
        = std::ranges::any_of(stations, [this](const RadioStation& station) { return m_controller->isSaved(station); });

    m_saveStationsAction->setEnabled(canSave);
    m_removeStationsAction->setEnabled(canRemove);
}

void RadioBrowserWidget::scheduleFilterSearch()
{
    if(m_loadingLayout) {
        return;
    }

    updateSavedSearchState();
    m_filterThrottler->throttle();
}

void RadioBrowserWidget::startInitialSearch()
{
    if(m_initialSearchState != InitialSearchState::Pending) {
        return;
    }

    m_initialSearchState = InitialSearchState::Complete;

    if(hasStationFilters(currentFilterRequest())) {
        applyFilterSearch(false);
    }
    else {
        browseInitialSelection();
    }
}

bool RadioBrowserWidget::syncControllerBrowseState()
{
    if(m_controller->stationRequestActive()) {
        if(!m_controller->browsingSavedStations()) {
            const RadioSearchRequest request = m_controller->currentRequest();
            setFilterRequest(request);
            updateApiSortingState(request);
        }
        m_resultsView->setLoading(true);
        m_initialSearchState = InitialSearchState::Complete;
        return true;
    }

    if(!m_controller->hasActivatedBrowse()) {
        return false;
    }

    if(m_controller->browsingSavedStations()) {
        handleSavedStationsBrowsingChanged(true);
    }
    else {
        const RadioSearchRequest request = m_controller->currentRequest();
        setFilterRequest(request);
        updateApiSortingState(request);
    }

    handleStationsChanged(m_controller->stations(), true);
    m_initialSearchState = InitialSearchState::Complete;
    return true;
}

void RadioBrowserWidget::browseInitialSelection()
{
    m_controller->fetchTopVoted();
}

void RadioBrowserWidget::applyFilterSearch()
{
    applyFilterSearch(true);
}

void RadioBrowserWidget::applyFilterSearch(bool updateLatestSearch)
{
    const RadioSearchRequest request = currentFilterRequest();

    if(updateLatestSearch) {
        m_controller->manualSearchStations(request);
    }
    else {
        m_controller->searchStations(request);
    }

    updateSavedSearchState();
}

RadioSearchRequest RadioBrowserWidget::currentFilterRequest() const
{
    if(m_filterBar) {
        RadioSearchRequest request   = m_filterBar->request(m_hideBroken);
        const QString preservedOrder = m_filterRequest.order.trimmed();
        if(!preservedOrder.isEmpty() && preservedOrder != "random"_L1) {
            request.order   = m_filterRequest.order;
            request.reverse = m_filterRequest.reverse;
        }
        return request;
    }

    RadioSearchRequest request{m_filterRequest};
    request.hideBroken = m_hideBroken;
    return request;
}

void RadioBrowserWidget::setFilterRequest(const RadioSearchRequest& request)
{
    m_filterRequest = request;

    if(m_filterBar) {
        m_filterBar->setRequest(request);
    }

    m_hideBroken        = request.hideBroken;
    m_config.hideBroken = m_hideBroken;
    m_controller->setHideBroken(m_hideBroken);
    updateSavedSearchState();
}

void RadioBrowserWidget::updateSavedSearchState()
{
    const RadioSearchRequest request = currentFilterRequest();
    m_currentSavedSearch             = m_controller->savedSearchForRequest(request);

    if(m_filterBar) {
        m_filterBar->setSaveSearchEnabled(!isDefaultSearchRequest(request) || m_currentSavedSearch.has_value());
        m_filterBar->setSaveSearchToolTip(m_currentSavedSearch ? tr("Saved search") : tr("Save search"));
        m_filterBar->setSaveSearchIcon(m_currentSavedSearch.has_value());
    }
}

void RadioBrowserWidget::saveCurrentSearch()
{
    const RadioSearchRequest request = currentFilterRequest();
    if(isDefaultSearchRequest(request)) {
        return;
    }

    m_controller->saveSearch(defaultSearchName(request), request);
}

void RadioBrowserWidget::showSavedSearchMenu()
{
    if(!m_currentSavedSearch) {
        return;
    }

    const RadioSavedSearch search{*m_currentSavedSearch};

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* titleAction = menu->addAction(tr("Saved as \"%1\"").arg(search.name));
    titleAction->setEnabled(false);
    menu->addSeparator();

    auto* renameAction = menu->addAction(tr("Rename saved search…"));
    renameAction->setStatusTip(tr("Rename the saved search"));
    QObject::connect(renameAction, &QAction::triggered, this, [this, search]() { renameSavedSearch(search); });

    auto* removeAction = menu->addAction(tr("Remove saved search"));
    removeAction->setStatusTip(tr("Remove the saved search"));
    QObject::connect(removeAction, &QAction::triggered, this, [this, search]() { removeSavedSearch(search); });

    if(m_filterBar) {
        menu->popup(m_filterBar->saveSearchMenuPosition());
    }
}

void RadioBrowserWidget::renameSavedSearch(const RadioSavedSearch& search)
{
    bool accepted{false};
    const QString name = QInputDialog::getText(this, tr("Rename Saved Search"), tr("Name") + u":"_s, QLineEdit::Normal,
                                               search.name, &accepted)
                             .trimmed();
    if(accepted && !name.isEmpty()) {
        m_controller->renameSearch(search.id, name);
    }
}

void RadioBrowserWidget::removeSavedSearch(const RadioSavedSearch& search)
{
    m_controller->removeSearch(search.id);
}

QString RadioBrowserWidget::defaultSearchName(const RadioSearchRequest& request) const
{
    QStringList parts;

    if(!request.text.trimmed().isEmpty()) {
        parts.push_back(request.text.trimmed());
    }
    if(!request.countryCode.trimmed().isEmpty()) {
        parts.push_back(m_filterBar ? m_filterBar->countryName(request.countryCode) : request.countryCode.trimmed());
    }
    if(!request.tag.trimmed().isEmpty()) {
        parts.push_back(m_filterBar ? m_filterBar->tagName(request.tag) : request.tag.trimmed());
    }
    if(!request.codec.trimmed().isEmpty()) {
        parts.push_back(request.codec.trimmed());
    }

    return parts.isEmpty() ? tr("Radio Search") : parts.join(u" "_s);
}

void RadioBrowserWidget::resetFilters()
{
    if(m_filterBar) {
        m_filterBar->reset();
        m_filterRequest = m_filterBar->request(m_hideBroken);
    }
    else {
        m_filterRequest = {};
    }

    applyFilterSearch();
    updateSavedSearchState();
}

void RadioBrowserWidget::setFilterBarVisible(bool visible)
{
    visible = visible && m_filterBarMode != FilterBarMode::Hidden;

    if(m_filterBar) {
        m_filterBar->setVisible(visible);
    }
    if(m_toggleFilterBarAction->isChecked() != visible) {
        m_toggleFilterBarAction->setChecked(visible);
    }
}

void RadioBrowserWidget::refreshThemeIcons()
{
    updateSavedSearchState();

    if(m_filterBar) {
        m_filterBar->refreshThemeIcons();
    }

    m_model->refreshIcons();
    m_resultsView->viewport()->update();
}

void RadioBrowserWidget::scheduleVisibleIconRequest()
{
    if(m_visibleIconRequestPending) {
        return;
    }

    m_visibleIconRequestPending = true;

    QMetaObject::invokeMethod(m_resultsView, [this]() {
        m_visibleIconRequestPending = false;
        requestVisibleIcons();
    });
}

void RadioBrowserWidget::requestVisibleIcons()
{
    if(!m_viewConfig.showIcons) {
        return;
    }

    m_model->requestIcons(m_resultsView->visibleIndexes(128));
}

void RadioBrowserWidget::maybeLoadMoreStations()
{
    if(!m_controller->canLoadMoreStations()) {
        return;
    }

    const QScrollBar* scrollBar = m_resultsView->verticalScrollBar();
    if(!scrollBar || scrollBar->maximum() <= 0) {
        return;
    }

    const int threshold = std::max(scrollBar->pageStep(), 1);
    if(scrollBar->maximum() - scrollBar->value() <= threshold) {
        m_controller->loadMoreStations();
    }
}

RadioStationList RadioBrowserWidget::selectedStations() const
{
    RadioStationList stations;

    const QModelIndexList indexes = m_resultsView->selectionModel()->selectedRows();
    stations.reserve(indexes.size());

    for(const QModelIndex& index : indexes) {
        if(index.isValid()) {
            stations.push_back(m_model->stationAt(index.row()));
        }
    }

    return stations;
}

void RadioBrowserWidget::updateSelectedTracks()
{
    if(!m_trackSelection) {
        return;
    }

    TrackSelection selection;
    for(const RadioStation& station : selectedStations()) {
        selection.tracks.push_back(RadioBrowserController::trackForStation(station));
    }

    m_trackSelection->changeSelectedTracks(m_context, selection);
}

void RadioBrowserWidget::addSelectedStations(bool play)
{
    executeResolvedAction(play ? TrackAction::Play : TrackAction::AddActivePlaylist);
}

void RadioBrowserWidget::queueSelectedStations()
{
    executeResolvedAction(TrackAction::AddToQueue);
}

void RadioBrowserWidget::executeResolvedAction(TrackAction action)
{
    if(!m_trackSelection || action == TrackAction::None) {
        return;
    }

    const RadioStationList stations = selectedStations();
    if(stations.empty()) {
        return;
    }

    PlaylistAction::ActionOptions options;
    if(action == TrackAction::Play) {
        options |= PlaylistAction::TempPlaylist;
    }
    if(m_playbackOnSend) {
        options |= PlaylistAction::StartPlayback;
    }

    const int resolveId = m_controller->resolveStations(stations, this);
    if(resolveId <= 0) {
        return;
    }

    m_pendingTrackActions.emplace(resolveId, PendingTrackAction{.action = action, .options = options});
}

void RadioBrowserWidget::handleResolvedTracks(int resolveId, QObject* context, const TrackList& tracks)
{
    if(context != this || !m_trackSelection) {
        return;
    }

    const auto it = m_pendingTrackActions.find(resolveId);
    if(it == m_pendingTrackActions.end()) {
        return;
    }

    const PendingTrackAction pending = it->second;
    m_pendingTrackActions.erase(it);
    if(tracks.empty() || pending.action == TrackAction::None) {
        return;
    }

    TrackSelection selection;
    selection.tracks = tracks;
    m_trackSelection->changeSelectedTracks(m_context, selection);
    m_trackSelection->executeAction(pending.action, pending.options);
}

void RadioBrowserWidget::saveSelectedStation()
{
    const RadioStationList stations = selectedStations();
    for(const RadioStation& station : stations) {
        if(!m_controller->isSaved(station)) {
            m_controller->saveStation(station);
        }
    }
    updateActionState();
}

void RadioBrowserWidget::removeSelectedStations()
{
    for(const RadioStation& station : selectedStations()) {
        m_controller->removeSavedStation(station);
    }
    updateActionState();
}

void RadioBrowserWidget::addCustomStation()
{
    RadioStationDialog dialog{m_controller, this};
    if(dialog.exec() == QDialog::Accepted) {
        m_controller->addLocalStation(dialog.station());
        m_controller->browseSavedStations();
    }
}

void RadioBrowserWidget::importSavedStations()
{
    RadioStationImportExportDialog::importStations(m_controller, this);
}

void RadioBrowserWidget::exportSavedStations()
{
    RadioStationImportExportDialog::exportStations(m_controller, this);
}

void RadioBrowserWidget::editSelectedStation()
{
    const RadioStationList stations = selectedStations();
    if(stations.size() != 1) {
        return;
    }

    const RadioStation& station = stations.front();
    if(!station.local || !m_controller->isSaved(station)) {
        auto* dialog = RadioStationDialog::createDetailsDialog(m_controller, station, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->open();
        return;
    }

    RadioStationDialog dialog{m_controller, station, this};
    if(dialog.exec() == QDialog::Accepted) {
        m_controller->updateSavedStation(station, dialog.station());
    }
}

void RadioBrowserWidget::handleDoubleClick(const QModelIndex& index)
{
    executeClickAction(index, m_doubleClickAction);
}

void RadioBrowserWidget::handleMiddleClick(const QModelIndex& index)
{
    executeClickAction(index, m_middleClickAction);
}

void RadioBrowserWidget::executeClickAction(const QModelIndex& index, const int actionValue)
{
    if(!index.isValid()) {
        return;
    }

    const auto action = static_cast<Fooyin::TrackAction>(actionValue);
    if(action == Fooyin::TrackAction::None) {
        return;
    }

    executeResolvedAction(action);
}

void RadioBrowserWidget::addTrackAction(QMenu* menu, const char* actionId)
{
    if(!m_actionManager || !m_trackSelection) {
        return;
    }

    if(auto* command = m_actionManager->command(actionId)) {
        const auto id = QLatin1StringView{actionId};
        std::optional<TrackAction> action;
        if(id == QLatin1StringView{Constants::Actions::AddToCurrent}) {
            action = TrackAction::AddCurrentPlaylist;
        }
        else if(id == QLatin1StringView{Constants::Actions::AddToActive}) {
            action = TrackAction::AddActivePlaylist;
        }
        else if(id == QLatin1StringView{Constants::Actions::SendToCurrent}) {
            action = TrackAction::SendCurrentPlaylist;
        }
        else if(id == QLatin1StringView{Constants::Actions::SendToNew}) {
            action = TrackAction::SendNewPlaylist;
        }
        else if(id == QLatin1StringView{Constants::Actions::AddToQueue}) {
            action = TrackAction::AddToQueue;
        }
        else if(id == QLatin1StringView{Constants::Actions::QueueNext}) {
            action = TrackAction::QueueNext;
        }

        if(!action) {
            menu->addAction(command->action());
            return;
        }

        QAction* sourceAction = command->action();
        auto* menuAction      = menu->addAction(sourceAction->icon(), sourceAction->text());
        menuAction->setEnabled(sourceAction->isEnabled());
        menuAction->setStatusTip(sourceAction->statusTip());
        QObject::connect(menuAction, &QAction::triggered, this, [this, action]() { executeResolvedAction(*action); });
    }
}

void RadioBrowserWidget::showContextMenu(const QPoint& pos)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    updateSelectedTracks();

    const RadioStationList stations = selectedStations();
    const bool hasSelection         = !stations.empty();

    ContextMenuUtils::renderStaticContextMenu(
        menu, RadioBrowserContextMenu::DefaultItems,
        m_settings->fileValue(RadioBrowserContextMenu::LayoutKey, QStringList{}).toStringList(),
        m_settings
            ->fileValue(RadioBrowserContextMenu::DisabledSectionsKey,
                        RadioBrowserContextMenu::defaultDisabledSections())
            .toStringList(),
        [&](const auto& id, QMenu* targetMenu, const auto& sectionEnabled) {
            if(id == QLatin1StringView{RadioBrowserContextMenu::Play}) {
                if(hasSelection && sectionEnabled(RadioBrowserContextMenu::Play)) {
                    auto* playAction = targetMenu->addAction(tr("Play"));
                    playAction->setStatusTip(tr("Play the selected station"));
                    QObject::connect(playAction, &QAction::triggered, this, [this]() { addSelectedStations(true); });
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::AddToCurrent}) {
                if(hasSelection && sectionEnabled(Constants::Actions::AddToCurrent)) {
                    addTrackAction(targetMenu, Constants::Actions::AddToCurrent);
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::AddToActive}) {
                if(hasSelection && sectionEnabled(Constants::Actions::AddToActive)) {
                    addTrackAction(targetMenu, Constants::Actions::AddToActive);
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::SendToCurrent}) {
                if(hasSelection && sectionEnabled(Constants::Actions::SendToCurrent)) {
                    addTrackAction(targetMenu, Constants::Actions::SendToCurrent);
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::SendToNew}) {
                if(hasSelection && sectionEnabled(Constants::Actions::SendToNew)) {
                    addTrackAction(targetMenu, Constants::Actions::SendToNew);
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::AddToPlaylist}) {
                if(hasSelection && m_trackSelection && sectionEnabled(Constants::Actions::AddToPlaylist)) {
                    auto* playlistMenu = new QMenu(tr("Add to playlist"), targetMenu);
                    m_trackSelection->addTrackAddToPlaylistContextMenu(playlistMenu);
                    if(!playlistMenu->actions().empty()) {
                        auto* playlistAction = targetMenu->addMenu(playlistMenu);
                        playlistAction->setStatusTip(tr("Add the selected stations to another playlist"));
                    }
                    else {
                        playlistMenu->deleteLater();
                    }
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::AddToQueue}) {
                if(hasSelection && sectionEnabled(Constants::Actions::AddToQueue)) {
                    addTrackAction(targetMenu, Constants::Actions::AddToQueue);
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::QueueNext}) {
                if(hasSelection && sectionEnabled(Constants::Actions::QueueNext)) {
                    addTrackAction(targetMenu, Constants::Actions::QueueNext);
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::RemoveFromQueue}) {
                if(hasSelection && sectionEnabled(Constants::Actions::RemoveFromQueue)) {
                    addTrackAction(targetMenu, Constants::Actions::RemoveFromQueue);
                }
                return;
            }
            if(id == QLatin1StringView{RadioBrowserContextMenu::Save}) {
                if(m_saveStationsAction->isEnabled() && sectionEnabled(RadioBrowserContextMenu::Save)) {
                    targetMenu->addAction(m_saveStationsCmd->action());
                }
                return;
            }
            if(id == QLatin1StringView{RadioBrowserContextMenu::Remove}) {
                if(m_removeStationsAction->isEnabled() && sectionEnabled(RadioBrowserContextMenu::Remove)) {
                    targetMenu->addAction(m_removeStationsCmd->action());
                }
                return;
            }
            if(id == QLatin1StringView{RadioBrowserContextMenu::Edit}) {
                if(stations.size() == 1 && sectionEnabled(RadioBrowserContextMenu::Edit)) {
                    const bool canEdit = stations.front().local && m_controller->isSaved(stations.front());
                    auto* editAction
                        = targetMenu->addAction(canEdit ? tr("Edit station…") : tr("View station details…"));
                    editAction->setStatusTip(canEdit ? tr("Edit the selected custom station")
                                                     : tr("View details for the selected station"));
                    QObject::connect(editAction, &QAction::triggered, this, &RadioBrowserWidget::editSelectedStation);
                }
                return;
            }
            if(id == QLatin1StringView{RadioBrowserContextMenu::AddCustom}) {
                if(m_model->reorderEnabled() && sectionEnabled(RadioBrowserContextMenu::AddCustom)) {
                    auto* addCustomAction = targetMenu->addAction(tr("Add custom station…"));
                    addCustomAction->setStatusTip(tr("Add a custom station to My Stations"));
                    QObject::connect(addCustomAction, &QAction::triggered, this, &RadioBrowserWidget::addCustomStation);
                }
                return;
            }
            if(id == QLatin1StringView{RadioBrowserContextMenu::CopyStreamUrl}) {
                const bool hasUrl = std::ranges::any_of(
                    stations, [](const RadioStation& station) { return !station.streamUrl.isEmpty(); });
                if(hasUrl && sectionEnabled(RadioBrowserContextMenu::CopyStreamUrl)) {
                    auto* copyUrlAction = targetMenu->addAction(tr("Copy stream URL"));
                    copyUrlAction->setStatusTip(tr("Copy the selected station stream URLs to the clipboard"));
                    QObject::connect(copyUrlAction, &QAction::triggered, this, [stations]() {
                        QStringList urls;
                        for(const RadioStation& station : stations) {
                            const QString url = station.streamUrl;
                            if(!url.isEmpty()) {
                                urls.push_back(url);
                            }
                        }
                        if(!urls.empty()) {
                            QGuiApplication::clipboard()->setText(urls.join(u'\n'));
                        }
                    });
                }
                return;
            }
            if(id == QLatin1StringView{RadioBrowserContextMenu::OpenHomepage}) {
                const bool hasHomepage = hasSelection && !stations.front().homepage.isEmpty();
                if(hasHomepage && sectionEnabled(RadioBrowserContextMenu::OpenHomepage)) {
                    auto* openAction = targetMenu->addAction(tr("Open homepage"));
                    openAction->setStatusTip(tr("Open the selected station homepage in a browser"));
                    QObject::connect(openAction, &QAction::triggered, this, [stations]() {
                        if(!stations.empty() && !stations.front().homepage.isEmpty()) {
                            QDesktopServices::openUrl(QUrl{stations.front().homepage});
                        }
                    });
                }
                return;
            }
            if(id == QLatin1StringView{RadioBrowserContextMenu::OpenRadioBrowser}) {
                const QUrl radioBrowserUrl     = hasSelection ? radioBrowserStationUrl(stations.front()) : QUrl{};
                const bool hasRadioBrowserPage = !radioBrowserUrl.isEmpty() && radioBrowserUrl.isValid();
                if(hasRadioBrowserPage && sectionEnabled(RadioBrowserContextMenu::OpenRadioBrowser)) {
                    auto* openAction = targetMenu->addAction(tr("Open radio-browser.info page"));
                    openAction->setStatusTip(tr("Open the selected station on radio-browser.info"));
                    QObject::connect(openAction, &QAction::triggered, this, [stations]() {
                        if(!stations.empty()) {
                            const QUrl url = radioBrowserStationUrl(stations.front());
                            if(!url.isEmpty() && url.isValid()) {
                                QDesktopServices::openUrl(url);
                            }
                        }
                    });
                }
                return;
            }
            if(id == QLatin1StringView{RadioBrowserContextMenu::Display}) {
                if(sectionEnabled(RadioBrowserContextMenu::Display)) {
                    addDisplayMenu(targetMenu);
                }
                return;
            }
            if(id == QLatin1StringView{RadioBrowserContextMenu::Configure}) {
                if(sectionEnabled(RadioBrowserContextMenu::Configure)) {
                    addConfigureAction(targetMenu, false);
                }
                return;
            }
        });

    if(m_model->reorderEnabled()) {
        if(!menu->actions().empty()) {
            menu->addSeparator();
        }

        auto* importAction = menu->addAction(tr("Import stations…"));
        importAction->setStatusTip(tr("Import saved stations from a playlist file"));
        QObject::connect(importAction, &QAction::triggered, this, &RadioBrowserWidget::importSavedStations);

        auto* exportAction = menu->addAction(tr("Export stations…"));
        exportAction->setStatusTip(tr("Export My Stations to a playlist file"));
        exportAction->setEnabled(!m_controller->savedStations().empty());
        QObject::connect(exportAction, &QAction::triggered, this, &RadioBrowserWidget::exportSavedStations);
    }

    menu->popup(m_resultsView->viewport()->mapToGlobal(pos));
}

void RadioBrowserWidget::showHeaderContextMenu(const QPoint& pos)
{
    auto* header = m_resultsView->stationHeader();

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    addColumnsMenu(menu);
    menu->addSeparator();
    auto* autoSizeSections = new QAction(tr("Auto-size sections"), menu);
    autoSizeSections->setStatusTip(tr("Automatically size columns to fill the available width"));
    autoSizeSections->setCheckable(true);
    autoSizeSections->setChecked(header->isStretchEnabled());
    QObject::connect(autoSizeSections, &QAction::triggered, header,
                     [header]() { header->setStretchEnabled(!header->isStretchEnabled()); });
    menu->addAction(autoSizeSections);

    auto* resetAction = new QAction(tr("Reset columns to default"), menu);
    QObject::connect(resetAction, &QAction::triggered, m_resultsView, &RadioStationView::resetColumnsToDefault);
    menu->addAction(resetAction);

    menu->addSeparator();
    addDisplayMenu(menu);
    addConfigureAction(menu, false);

    menu->popup(header->mapToGlobal(pos));
}

void RadioBrowserWidget::addColumnsMenu(QMenu* menu)
{
    if(!menu) {
        return;
    }

    auto* header      = m_resultsView->stationHeader();
    auto* columnsMenu = new QMenu(tr("Columns"), menu);
    auto* columnGroup = new QActionGroup(columnsMenu);
    columnGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::None);

    int visibleColumns{0};
    for(int section{0}; section < header->count(); ++section) {
        const int logical = header->logicalIndex(section);
        if(!header->isSectionHidden(logical)) {
            ++visibleColumns;
        }
    }

    for(int section{0}; section < header->count(); ++section) {
        const int logical   = header->logicalIndex(section);
        const QString title = m_model->headerData(logical, Qt::Horizontal, Qt::DisplayRole).toString().trimmed();
        if(title.isEmpty()) {
            continue;
        }

        auto* columnAction = new QAction(title, columnsMenu);
        columnAction->setCheckable(true);
        columnAction->setChecked(!header->isSectionHidden(logical));
        columnAction->setEnabled(logical == Station ? false : (columnAction->isChecked() ? visibleColumns > 1 : true));
        columnAction->setData(logical);
        columnGroup->addAction(columnAction);
        columnsMenu->addAction(columnAction);
    }

    QObject::connect(columnGroup, &QActionGroup::triggered, this, [header](QAction* action) {
        const int logical = action->data().toInt();
        if(action->isChecked()) {
            header->showHeaderSection(logical);
        }
        else {
            header->hideHeaderSection(logical);
        }
    });

    menu->addMenu(columnsMenu);
}

void RadioBrowserWidget::addDisplayMenu(QMenu* menu)
{
    auto* displayMenu  = new QMenu(tr("Display"), menu);
    auto* displayGroup = new QActionGroup(displayMenu);

    auto* displayColumns    = new QAction(tr("Columns"), displayGroup);
    auto* displayIconRight  = new QAction(tr("Icons (right captions)"), displayGroup);
    auto* displayIconBottom = new QAction(tr("Icons (bottom captions)"), displayGroup);

    displayColumns->setCheckable(true);
    displayIconRight->setCheckable(true);
    displayIconBottom->setCheckable(true);

    if(m_viewConfig.viewMode == ExpandedTreeView::ViewMode::Tree) {
        displayColumns->setChecked(true);
    }
    else if(m_viewConfig.captions == ExpandedTreeView::CaptionDisplay::Right) {
        displayIconRight->setChecked(true);
    }
    else if(m_viewConfig.captions == ExpandedTreeView::CaptionDisplay::Bottom) {
        displayIconBottom->setChecked(true);
    }

    QObject::connect(displayColumns, &QAction::triggered, this, [this]() {
        auto config{m_viewConfig};
        config.viewMode = ExpandedTreeView::ViewMode::Tree;
        setViewConfig(config);
    });
    QObject::connect(displayIconRight, &QAction::triggered, this, [this]() {
        auto config{m_viewConfig};
        config.viewMode = ExpandedTreeView::ViewMode::Icon;
        config.captions = ExpandedTreeView::CaptionDisplay::Right;
        setViewConfig(config);
    });
    QObject::connect(displayIconBottom, &QAction::triggered, this, [this]() {
        auto config{m_viewConfig};
        config.viewMode = ExpandedTreeView::ViewMode::Icon;
        config.captions = ExpandedTreeView::CaptionDisplay::Bottom;
        setViewConfig(config);
    });

    displayMenu->addAction(displayColumns);
    displayMenu->addAction(displayIconRight);
    displayMenu->addAction(displayIconBottom);
    displayMenu->addSeparator();

    displayMenu->addAction(m_toggleFilterBarAction);

    auto* showIcons = new QAction(tr("Show station icons"), displayMenu);
    showIcons->setCheckable(true);
    showIcons->setChecked(m_viewConfig.showIcons);
    QObject::connect(showIcons, &QAction::triggered, this, [this](bool checked) {
        auto config{m_viewConfig};
        config.showIcons = checked;
        setViewConfig(config);
    });

    auto* showToolTips = new QAction(tr("Show station tooltips"), displayMenu);
    showToolTips->setCheckable(true);
    showToolTips->setChecked(m_viewConfig.showToolTips);
    QObject::connect(showToolTips, &QAction::triggered, this, [this](bool checked) {
        auto config{m_viewConfig};
        config.showToolTips = checked;
        setViewConfig(config);
    });

    auto* showSavedIcons = new QAction(tr("Show saved station icons"), displayMenu);
    showSavedIcons->setCheckable(true);
    showSavedIcons->setChecked(m_viewConfig.showSavedStationIcons);
    QObject::connect(showSavedIcons, &QAction::triggered, this, [this](bool checked) {
        auto config{m_viewConfig};
        config.showSavedStationIcons = checked;
        setViewConfig(config);
    });

    auto* showHeader = new QAction(tr("Show header"), displayMenu);
    showHeader->setCheckable(true);
    showHeader->setChecked(m_viewConfig.showHeader);
    QObject::connect(showHeader, &QAction::triggered, this, [this](bool checked) {
        auto config{m_viewConfig};
        config.showHeader = checked;
        setViewConfig(config);
    });

    auto* showScrollbar = new QAction(tr("Show scrollbar"), displayMenu);
    showScrollbar->setCheckable(true);
    showScrollbar->setChecked(m_viewConfig.showScrollbar);
    QObject::connect(showScrollbar, &QAction::triggered, this, [this](bool checked) {
        auto config{m_viewConfig};
        config.showScrollbar = checked;
        setViewConfig(config);
    });

    auto* alternatingRows = new QAction(tr("Alternating row colours"), displayMenu);
    alternatingRows->setCheckable(true);
    alternatingRows->setChecked(m_viewConfig.alternatingRows);
    QObject::connect(alternatingRows, &QAction::triggered, this, [this](bool checked) {
        auto config{m_viewConfig};
        config.alternatingRows = checked;
        setViewConfig(config);
    });

    displayMenu->addAction(showIcons);
    displayMenu->addAction(showSavedIcons);
    displayMenu->addAction(showToolTips);
    displayMenu->addAction(showHeader);
    displayMenu->addAction(showScrollbar);
    displayMenu->addAction(alternatingRows);

    menu->addMenu(displayMenu);
}

void RadioBrowserWidget::setViewConfig(const ConfigData::ViewConfig& config)
{
    m_viewConfig = config;

    m_viewConfig.rowHeight           = std::max(config.rowHeight, 0);
    m_viewConfig.iconHorizontalGap   = std::max(config.iconHorizontalGap, -1);
    m_viewConfig.iconVerticalGap     = std::max(config.iconVerticalGap, 0);
    m_viewConfig.iconItemBorderWidth = std::clamp(config.iconItemBorderWidth, 0, 16);

    if(!config.iconSize.isValid()) {
        m_viewConfig.iconSize = ConfigData::ViewConfig{}.iconSize;
    }
    else {
        const int iconSize    = std::max(config.iconSize.width(), config.iconSize.height());
        m_viewConfig.iconSize = {iconSize, iconSize};
    }

    const QSize previousIconSize{m_config.view.iconSize};
    m_config.view = m_viewConfig;

    m_resultsView->setViewMode(m_viewConfig.viewMode);
    m_resultsView->setCaptionDisplay(m_viewConfig.captions);
    m_resultsView->setIconHorizontalGap(m_viewConfig.iconHorizontalGap);
    m_resultsView->setIconVerticalGap(m_viewConfig.iconVerticalGap);
    m_resultsView->setUseIconGapsForSideCaptions(true);
    m_resultsView->changeIconSize(m_viewConfig.iconSize);
    m_delegate->setUniformStationIcons(m_viewConfig.uniformStationIcons);
    m_delegate->setIconItemBorderWidth(m_viewConfig.iconItemBorderWidth);
    m_model->setIconSize(m_viewConfig.iconSize);
    m_model->setRowHeight(m_viewConfig.rowHeight);
    m_model->setShowToolTips(m_viewConfig.showToolTips);
    m_model->setShowSavedIndicators(m_viewConfig.showSavedStationIcons && !m_controller->browsingSavedStations());
    m_resultsView->setVerticalScrollBarPolicy(m_viewConfig.showScrollbar ? Qt::ScrollBarAsNeeded
                                                                         : Qt::ScrollBarAlwaysOff);
    m_resultsView->setAlternatingRowColors(m_viewConfig.viewMode != ExpandedTreeView::ViewMode::Icon
                                           && m_viewConfig.alternatingRows);

    auto* header = m_resultsView->stationHeader();
    header->setFixedHeight(!m_viewConfig.showHeader ? 0 : QWIDGETSIZE_MAX);
    header->adjustSize();

    m_model->setShowIcons(m_viewConfig.showIcons);
    updateIconColumnOrder();

    if(previousIconSize != m_viewConfig.iconSize) {
        scheduleVisibleIconRequest();
    }

    QMetaObject::invokeMethod(m_resultsView->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));
}

void RadioBrowserWidget::updateIconColumnOrder()
{
    m_model->setIconColumnOrder(m_resultsView->viewMode() == ExpandedTreeView::ViewMode::Icon
                                    ? Utils::logicalIndexOrder(m_resultsView->stationHeader())
                                    : std::vector<int>{});
}

void RadioBrowserWidget::disconnectFilterBar()
{
    if(!m_filterBar) {
        return;
    }

    m_filterRequest = m_filterBar->request(m_hideBroken);
    QObject::disconnect(m_filterBar, nullptr, this, nullptr);
    m_filterBar.clear();
}

void RadioBrowserWidget::updateFilterBarCategories()
{
    if(!m_filterBar) {
        return;
    }

    if(!m_countryCategories.empty()) {
        m_filterBar->setCountryCategories(m_countryCategories);
    }
    if(!m_tagCategories.empty()) {
        m_filterBar->setTagCategories(m_tagCategories);
    }
    if(!m_codecCategories.empty()) {
        m_filterBar->setCodecCategories(m_codecCategories);
    }
}

void RadioBrowserWidget::updateFilterBarActionState()
{
    const bool available = m_filterBarMode != FilterBarMode::Hidden && m_filterBar;
    m_toggleFilterBarAction->setEnabled(available && m_filterBarMode == FilterBarMode::Toggleable);
    m_toggleFilterBarAction->setVisible(available && m_filterBarMode == FilterBarMode::Toggleable);
}

void RadioBrowserWidget::updateApiSortingState(const RadioSearchRequest& request)
{
    const int sortColumn = columnForApiOrder(request.order);
    if(sortColumn < 0 || normalisedApiOrder(request.order) == "random"_L1) {
        m_model->setApiSortingEnabled(false);
        m_model->clearSort();
        m_resultsView->setSortingEnabled(false);
        return;
    }

    m_resultsView->setSortingEnabled(false);
    m_resultsView->sortByColumn(sortColumn, request.reverse ? Qt::DescendingOrder : Qt::AscendingOrder);
    m_model->setApiSortingEnabled(true);
    m_resultsView->setSortingEnabled(true);
}
} // namespace Fooyin::RadioBrowser

#include "moc_radiobrowserwidget.cpp"
