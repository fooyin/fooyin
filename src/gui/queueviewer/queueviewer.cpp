/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "queueviewer.h"

#include "internalguisettings.h"
#include "playlist/playlistcontroller.h"
#include "queueviewerconfigwidget.h"
#include "queueviewerdelegate.h"
#include "queueviewermodel.h"
#include "queueviewerview.h"
#include "sortactionhandler.h"

#include <core/coresettings.h>
#include <core/library/sortingregistry.h>
#include <core/library/tracksort.h>
#include <core/player/playercontroller.h>
#include <gui/configdialog.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/guiutils.h>
#include <gui/playlist/playlistinteractor.h>
#include <gui/trackmimedata.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/async.h>
#include <utils/crypto.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QActionGroup>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QShowEvent>
#include <QTimer>

#include <random>
#include <ranges>

using namespace Qt::StringLiterals;

// Settings
constexpr auto QueueViewerShowIconKey      = u"PlaybackQueue/ShowIcon";
constexpr auto QueueViewerIconSizeKey      = u"PlaybackQueue/IconSize";
constexpr auto QueueViewerArtworkRadiusKey = u"PlaybackQueue/ArtworkCornerRadius";
constexpr auto QueueViewerHeaderKey        = u"PlaybackQueue/Header";
constexpr auto QueueViewerScrollBarKey     = u"PlaybackQueue/Scrollbar";
constexpr auto QueueViewerAltColoursKey    = u"PlaybackQueue/AlternatingColours";
constexpr auto QueueViewerLeftScriptKey    = u"PlaybackQueue/LeftScript";
constexpr auto QueueViewerRightScriptKey   = u"PlaybackQueue/RightScript";
constexpr auto QueueViewerShowCurrentKey   = u"PlaybackQueue/ShowCurrent";
constexpr auto QueueViewerDisplayModeKey   = u"PlaybackQueue/DisplayMode";
constexpr auto QueueViewerStateKey         = "PlaybackQueue/State"_L1;

namespace Fooyin {
namespace {
std::vector<PlaybackQueueItemId> queueItemIds(const PlaybackQueueItems& items)
{
    std::vector<PlaybackQueueItemId> ids;
    ids.reserve(items.size());
    std::ranges::transform(items, std::back_inserter(ids), &PlaybackQueueItem::id);
    return ids;
}
} // namespace

QueueViewer::QueueViewer(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                         TrackSelectionController* selectionController, CoverRepository* coverRepository,
                         SortingRegistry* sortingRegistry, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_actionManager{actionManager}
    , m_playlistInteractor{playlistInteractor}
    , m_playerController{m_playlistInteractor->playerController()}
    , m_selectionController{selectionController}
    , m_sortingRegistry{sortingRegistry}
    , m_settings{settings}
    , m_view{new QueueViewerView(this)}
    , m_delegate{new QueueViewerDelegate(this)}
    , m_model{new QueueViewerModel(coverRepository, m_playerController, settings, this)}
    , m_context{new WidgetContext(
          this, Context{IdList{Constants::Context::TrackSelection, Id{"Context.QueueViewer."}.append(id())}}, this)}
    , m_remove{new QAction(tr("&Remove"), this)}
    , m_removeCmd{nullptr}
    , m_clear{new QAction(tr("&Clear"), this)}
    , m_clearCmd{nullptr}
    , m_randomise{new QAction(tr("Randomise"), this)}
    , m_reverse{new QAction(tr("Reverse"), this)}
    , m_sortActions{std::make_unique<SortActionHandler>(m_actionManager, m_sortingRegistry, m_context->context(), this)}
    , m_updatesPausedForTrackChange{false}
    , m_updatePauseToken{0}
    , m_sortRequestToken{0}
    , m_topLevelStateLoaded{false}
{
    setObjectName(QueueViewer::name());
    setWindowTitle(QueueViewer::name());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    m_view->setModel(m_model);
    m_view->setItemDelegate(m_delegate);

    m_config = defaultConfig();
    applyConfig(m_config);

    setupActions();
    setupConnections();
    resetModelAndFollowCurrent();
}

QString QueueViewer::name() const
{
    return tr("Playback Queue");
}

QString QueueViewer::layoutName() const
{
    return u"PlaybackQueue"_s;
}

void QueueViewer::saveLayoutData(QJsonObject& layout)
{
    saveConfigToLayout(m_config, layout);
}

void QueueViewer::loadLayoutData(const QJsonObject& layout)
{
    applyConfig(configFromLayout(layout));
}

QSize QueueViewer::sizeHint() const
{
    return {400, 520};
}

bool QueueViewer::isWindowWidget() const
{
    return parentWidget() == nullptr;
}

QueueViewer::ConfigData QueueViewer::factoryConfig() const
{
    return {
        .leftScript          = u"%title%$crlf()%album%"_s,
        .rightScript         = u"%duration%"_s,
        .showCurrent         = true,
        .showIcon            = true,
        .iconSize            = QSize{36, 36},
        .artworkCornerRadius = 0,
        .showHeader          = true,
        .showScrollBar       = true,
        .alternatingRows     = false,
        .displayMode         = DisplayMode::PlayingTracks,
    };
}

QueueViewer::ConfigData QueueViewer::defaultConfig() const
{
    auto config{factoryConfig()};

    config.leftScript          = m_settings->fileValue(QueueViewerLeftScriptKey, config.leftScript).toString();
    config.rightScript         = m_settings->fileValue(QueueViewerRightScriptKey, config.rightScript).toString();
    config.showCurrent         = m_settings->fileValue(QueueViewerShowCurrentKey, config.showCurrent).toBool();
    config.showIcon            = m_settings->fileValue(QueueViewerShowIconKey, config.showIcon).toBool();
    config.iconSize            = m_settings->fileValue(QueueViewerIconSizeKey, config.iconSize).toSize();
    config.artworkCornerRadius = m_settings->fileValue(QueueViewerArtworkRadiusKey, config.artworkCornerRadius).toInt();
    config.showHeader          = m_settings->fileValue(QueueViewerHeaderKey, config.showHeader).toBool();
    config.showScrollBar       = m_settings->fileValue(QueueViewerScrollBarKey, config.showScrollBar).toBool();
    config.alternatingRows     = m_settings->fileValue(QueueViewerAltColoursKey, config.alternatingRows).toBool();
    const int displayMode
        = m_settings->fileValue(QueueViewerDisplayModeKey, static_cast<int>(config.displayMode)).toInt();
    config.displayMode = displayMode == static_cast<int>(DisplayMode::UpcomingTracks) ? DisplayMode::UpcomingTracks
                                                                                      : DisplayMode::PlayingTracks;

    return config;
}

const QueueViewer::ConfigData& QueueViewer::currentConfig() const
{
    return m_config;
}

void QueueViewer::saveDefaults(const ConfigData& config) const
{
    m_settings->fileSet(QueueViewerLeftScriptKey, config.leftScript);
    m_settings->fileSet(QueueViewerRightScriptKey, config.rightScript);
    m_settings->fileSet(QueueViewerShowCurrentKey, config.showCurrent);
    m_settings->fileSet(QueueViewerShowIconKey, config.showIcon);
    m_settings->fileSet(QueueViewerIconSizeKey, config.iconSize);
    m_settings->fileSet(QueueViewerArtworkRadiusKey, config.artworkCornerRadius);
    m_settings->fileSet(QueueViewerHeaderKey, config.showHeader);
    m_settings->fileSet(QueueViewerScrollBarKey, config.showScrollBar);
    m_settings->fileSet(QueueViewerAltColoursKey, config.alternatingRows);
    m_settings->fileSet(QueueViewerDisplayModeKey, static_cast<int>(config.displayMode));
}

void QueueViewer::clearSavedDefaults() const
{
    m_settings->fileRemove(QueueViewerLeftScriptKey);
    m_settings->fileRemove(QueueViewerRightScriptKey);
    m_settings->fileRemove(QueueViewerShowCurrentKey);
    m_settings->fileRemove(QueueViewerShowIconKey);
    m_settings->fileRemove(QueueViewerIconSizeKey);
    m_settings->fileRemove(QueueViewerArtworkRadiusKey);
    m_settings->fileRemove(QueueViewerHeaderKey);
    m_settings->fileRemove(QueueViewerScrollBarKey);
    m_settings->fileRemove(QueueViewerAltColoursKey);
    m_settings->fileRemove(QueueViewerDisplayModeKey);
}

void QueueViewer::applyConfig(const ConfigData& config)
{
    const bool displayModeChanged = m_config.displayMode != config.displayMode;
    m_config                      = config;
    m_config.artworkCornerRadius  = std::clamp(m_config.artworkCornerRadius, 0, 100);

    if(isWindowWidget()) {
        m_config.showHeader = false;
    }

    m_model->setScripts(m_config.leftScript, m_config.rightScript);
    m_model->setShowCurrent(m_config.showCurrent);
    m_model->setShowUpcomingTracks(m_config.displayMode == DisplayMode::UpcomingTracks);
    m_model->setShowIcon(m_config.showIcon);
    m_model->setIconSize(m_config.iconSize);

    m_view->changeIconSize(m_config.iconSize);
    m_delegate->setArtworkCornerRadius(m_config.artworkCornerRadius);
    m_view->header()->setHidden(!m_config.showHeader);
    m_view->setVerticalScrollBarPolicy(m_config.showScrollBar ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    m_view->setAlternatingRowColors(m_config.alternatingRows);

    const QVariant resolvedStyleValue = m_settings->value<Settings::Gui::ResolvedAppStyle>();
    Gui::refreshItemViewPalette(m_view, resolvedStyleValue.value<ResolvedAppStyle>().palette);

    m_view->viewport()->update();
    QMetaObject::invokeMethod(m_view->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));

    if(displayModeChanged) {
        resetModelAndFollowCurrent();
    }

    Q_EMIT configChanged();
}

void QueueViewer::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const bool headerMenu = m_view->header()->rect().contains(m_view->header()->mapFromGlobal(event->globalPos()));

    if(m_removeCmd && m_view->selectionModel()->hasSelection()) {
        m_remove->setEnabled(canRemoveSelected());
        menu->addAction(m_removeCmd->action());
    }
    if(m_clearCmd) {
        m_clear->setEnabled(canClearQueue());
        menu->addAction(m_clearCmd->action());
    }
    if(m_playerController->queuedTracksCount() > 1) {
        addSortMenu(menu);
    }

    if(m_playerController->playbackQueueMode() == PlaybackQueueMode::QueueAsPlaybackSource) {
        menu->addSeparator();

        auto* viewActionGroup = new QActionGroup(menu);
        viewActionGroup->setExclusive(true);

        auto* playingTracks = menu->addAction(tr("Playing Tracks"));
        playingTracks->setCheckable(true);
        playingTracks->setChecked(m_config.displayMode == DisplayMode::PlayingTracks);
        playingTracks->setStatusTip(tr("Show played, playing and upcoming tracks"));
        viewActionGroup->addAction(playingTracks);

        auto* upcomingTracks = menu->addAction(tr("Upcoming Tracks"));
        upcomingTracks->setCheckable(true);
        upcomingTracks->setChecked(m_config.displayMode == DisplayMode::UpcomingTracks);
        upcomingTracks->setStatusTip(tr("Show the playing track and tracks that will play next"));
        viewActionGroup->addAction(upcomingTracks);

        QObject::connect(playingTracks, &QAction::triggered, this, [this]() {
            auto config{m_config};
            config.displayMode = DisplayMode::PlayingTracks;
            applyConfig(config);
        });
        QObject::connect(upcomingTracks, &QAction::triggered, this, [this]() {
            auto config{m_config};
            config.displayMode = DisplayMode::UpcomingTracks;
            applyConfig(config);
        });
    }
    else {
        auto* showCurrent = new QAction(tr("Show playing queue track"), menu);
        showCurrent->setCheckable(true);
        showCurrent->setChecked(m_config.showCurrent);
        QObject::connect(showCurrent, &QAction::triggered, showCurrent, [this](bool enabled) {
            auto config{m_config};
            config.showCurrent = enabled;
            applyConfig(config);
        });

        menu->addSeparator();
        menu->addAction(showCurrent);
    }

    addConfigureAction(menu, false);

    if(!headerMenu) {
        menu->addSeparator();
        m_selectionController->addTrackContextMenu(menu, m_context);
    }

    menu->popup(event->globalPos());
}

void QueueViewer::showEvent(QShowEvent* event)
{
    if(isWindowWidget() && !m_topLevelStateLoaded) {
        loadTopLevelState();
    }

    FyWidget::showEvent(event);
}

void QueueViewer::closeEvent(QCloseEvent* event)
{
    if(isWindowWidget()) {
        saveTopLevelState();
    }

    FyWidget::closeEvent(event);
}

void QueueViewer::openConfigDialog()
{
    const bool showDisplayMode = m_playerController->playbackQueueMode() == PlaybackQueueMode::QueueAsPlaybackSource;
    showConfigDialog(new QueueViewerConfigDialog(this, showDisplayMode, this), Qt::NonModal);
}

void QueueViewer::setupActions()
{
    m_actionManager->addContextObject(m_context);

    Context actionContext{m_context->context()};
    actionContext.erase(Constants::Context::TrackSelection);

    m_remove->setStatusTip(tr("Remove the selected tracks from the playback queue"));
    m_removeCmd = m_actionManager->registerAction(m_remove, Constants::Actions::Remove, actionContext);
    m_removeCmd->setDefaultShortcut(QKeySequence::Delete);
    QObject::connect(m_remove, &QAction::triggered, this, &QueueViewer::removeSelectedTracks);
    QObject::connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        m_remove->setEnabled(canRemoveSelected());
        updateSortActionState();
        updateSelectedTracks();
    });
    m_remove->setEnabled(canRemoveSelected());

    auto* editMenu = m_actionManager->actionContainer(Constants::Menus::Edit);

    m_clear->setStatusTip(tr("Remove all tracks in the playback queue"));
    m_clearCmd = m_actionManager->registerAction(m_clear, Constants::Actions::Clear, actionContext);
    editMenu->addAction(m_clearCmd);
    QObject::connect(m_clear, &QAction::triggered, m_playerController, &PlayerController::clearQueue);
    m_clear->setEnabled(canClearQueue());

    auto* selectAllAction = new QAction(tr("&Select all"), this);
    selectAllAction->setStatusTip(tr("Select all tracks in the playback queue"));
    auto* selectAllCmd = m_actionManager->registerAction(selectAllAction, Constants::Actions::SelectAll, actionContext);
    selectAllCmd->setDefaultShortcut(QKeySequence::SelectAll);
    editMenu->addAction(selectAllCmd);
    QObject::connect(selectAllAction, &QAction::triggered, m_view, &QAbstractItemView::selectAll);

    m_sortActions->registerRandomiseAction(m_randomise, tr("Randomise the playback queue"));
    m_sortActions->registerReverseAction(m_reverse, tr("Reverse the playback queue"));
    QObject::connect(m_sortActions.get(), &SortActionHandler::randomiseRequested, this, &QueueViewer::randomiseTracks);
    QObject::connect(m_sortActions.get(), &SortActionHandler::reverseRequested, this, &QueueViewer::reverseTracks);
    QObject::connect(m_sortActions.get(), &SortActionHandler::sortPresetRequested, this, &QueueViewer::sortTracks);
    QObject::connect(m_sortActions.get(), &SortActionHandler::settingsRequested, this,
                     [this]() { m_settings->settingsDialog()->openAtPage(Constants::Page::LibrarySorting); });

    QObject::connect(m_sortingRegistry, &RegistryBase::itemAdded, this, &QueueViewer::refreshSortActions);
    QObject::connect(m_sortingRegistry, &RegistryBase::itemChanged, this, &QueueViewer::refreshSortActions);
    QObject::connect(m_sortingRegistry, &RegistryBase::itemRemoved, this, &QueueViewer::refreshSortActions);
    refreshSortActions();
}

void QueueViewer::setupConnections()
{
    QObject::connect(m_model, &QueueViewerModel::queueTracksMoved, this, &QueueViewer::handleQueueTracksMoved);
    QObject::connect(m_model, &QueueViewerModel::tracksDropped, this, &QueueViewer::handleTracksDropped);
    QObject::connect(m_model, &QueueViewerModel::playlistTracksDropped, this,
                     &QueueViewer::handlePlaylistTracksDropped);

    const auto resetQueueModel = [this](bool followCurrent) {
        if(!m_updatesPausedForTrackChange) {
            followCurrent ? resetModelAndFollowCurrent() : resetModel();
        }
    };

    QObject::connect(m_playerController, &PlayerController::trackQueueChanged, this,
                     [resetQueueModel]() { resetQueueModel(true); });
    QObject::connect(m_playerController, &PlayerController::trackIndexesDequeued, this,
                     [resetQueueModel]() { resetQueueModel(false); });
    QObject::connect(m_playerController, &PlayerController::tracksQueued, this,
                     [resetQueueModel]() { resetQueueModel(false); });
    QObject::connect(m_playerController, &PlayerController::tracksDequeued, this,
                     [resetQueueModel]() { resetQueueModel(false); });
    QObject::connect(m_playerController, &PlayerController::trackChangeRequested, this,
                     [this](const Player::TrackChangeRequest& request) {
                         const bool pauseUpdates
                             = request.isQueueTrack
                            && m_playerController->playbackQueueMode() == PlaybackQueueMode::QueueAsPlaybackSource;
                         if(pauseUpdates) {
                             m_updatesPausedForTrackChange = true;
                             m_view->setUpdatesEnabled(false);

                             // Just in case the track isn't committed
                             const int pauseToken = ++m_updatePauseToken;
                             QTimer::singleShot(1000, this, [this, pauseToken]() {
                                 if(m_updatesPausedForTrackChange && pauseToken == m_updatePauseToken) {
                                     resumeUpdatesAfterTrackChange();
                                 }
                             });
                         }
                     });
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, [this]() {
        if(m_updatesPausedForTrackChange) {
            resumeUpdatesAfterTrackChange();
        }
        else {
            if(!m_model->updatePlaybackPosition(m_playerController->playbackQueue())) {
                resetModel();
            }
            scrollToCurrentTrack();
        }
    });
    QObject::connect(m_playerController, &PlayerController::currentTrackUpdated, this, &QueueViewer::resetModel);
    QObject::connect(m_playerController, &PlayerController::playStateChanged, m_model,
                     &QueueViewerModel::playbackStateChanged);
    QObject::connect(m_model, &QAbstractItemModel::rowsInserted, this, &QueueViewer::handleRowsChanged);
    QObject::connect(m_model, &QAbstractItemModel::rowsRemoved, this, &QueueViewer::handleRowsChanged);
    QObject::connect(m_view, &QAbstractItemView::iconSizeChanged, this, [this](const QSize& size) {
        if(m_config.iconSize == size) {
            return;
        }

        m_config.iconSize = size;
        m_model->setIconSize(size);
        Q_EMIT configChanged();
    });
    QObject::connect(m_view, &QAbstractItemView::doubleClicked, this, &QueueViewer::handleQueueDoubleClicked);

    m_settings->subscribe<Settings::Gui::ResolvedAppStyle>(this, [this](const QVariant& var) {
        const auto resolvedStyle = var.value<ResolvedAppStyle>();
        Gui::updateItemViewStyle(m_view, resolvedStyle.palette);
    });
    m_settings->subscribe<Settings::Gui::PlaybackQueueFollowCurrent>(this, [this](bool enabled) {
        if(enabled) {
            scrollToCurrentTrack();
        }
    });
    m_settings->subscribe<Settings::Core::PlaybackQueueMode>(this, [this](int mode) {
        const bool available = static_cast<PlaybackQueueMode>(mode) == PlaybackQueueMode::QueueAsPlaybackSource;
        Q_EMIT displayModeAvailabilityChanged(available);
    });
}

void QueueViewer::resetModel() const
{
    const auto viewState = captureViewState();

    m_model->reset(m_playerController->playbackQueue());

    if(m_view->selectionModel()) {
        restoreViewState(viewState);
    }

    updateSelectedTracks();
    updateSortActionState();
}

void QueueViewer::resetModelAndFollowCurrent() const
{
    resetModel();
    scrollToCurrentTrack();
}

void QueueViewer::scrollToCurrentTrack() const
{
    if((!m_settings->value<Settings::Gui::PlaybackQueueFollowCurrent>() && !showsUpcomingTracks())
       || m_playerController->playbackQueueMode() != PlaybackQueueMode::QueueAsPlaybackSource) {
        return;
    }

    const QModelIndex index = m_model->indexForQueueItem(m_playerController->currentQueueItemId());
    if(index.isValid()) {
        m_view->scrollTo(index, QAbstractItemView::PositionAtTop);
    }
}

void QueueViewer::resumeUpdatesAfterTrackChange()
{
    if(!m_updatesPausedForTrackChange) {
        return;
    }

    m_updatesPausedForTrackChange = false;
    ++m_updatePauseToken;
    if(!m_model->updatePlaybackPosition(m_playerController->playbackQueue())) {
        resetModel();
    }
    scrollToCurrentTrack();
    m_view->setUpdatesEnabled(true);
}

void QueueViewer::addSortMenu(QMenu* menu) const
{
    if(!menu->actions().empty()) {
        menu->addSeparator();
    }

    m_sortActions->addSortMenu(menu, false, SortScope::SelectedOrAll);
}

void QueueViewer::refreshSortActions()
{
    m_sortActions->refreshPresetActions(tr("Sort the playback queue using this preset"));
    updateSortActionState();
}

void QueueViewer::updateSortActionState() const
{
    const bool canSortTracks = queueIndexesToSort(SortScope::All).size() > 1;

    m_randomise->setEnabled(canSortTracks);
    m_reverse->setEnabled(canSortTracks);

    if(m_sortActions) {
        m_sortActions->setEnabled(canSortTracks);
    }
}

bool QueueViewer::showsUpcomingTracks() const
{
    return m_config.displayMode == DisplayMode::UpcomingTracks
        && m_playerController->playbackQueueMode() == PlaybackQueueMode::QueueAsPlaybackSource;
}

QueueViewer::ViewState QueueViewer::captureViewState() const
{
    ViewState state;
    state.scrollValue = m_view->verticalScrollBar()->value();
    state.current     = viewRowState(m_view->currentIndex());
    state.top         = viewRowState(m_view->indexAt({1, 1}));

    if(auto* selectionModel = m_view->selectionModel()) {
        const auto selected = selectionModel->selectedRows();
        state.selection.reserve(selected.size());

        for(const QModelIndex& index : selected) {
            if(const auto rowState = viewRowState(index); rowState.isValid()) {
                state.selection.emplace_back(rowState);
            }
        }
    }

    return state;
}

void QueueViewer::restoreViewState(const ViewState& state) const
{
    auto* selectionModel = m_view->selectionModel();
    if(!selectionModel) {
        return;
    }

    selectionModel->clearSelection();
    m_view->setCurrentIndex({});

    for(const auto& rowState : state.selection) {
        if(const QModelIndex index = indexForViewRowState(rowState); index.isValid()) {
            selectionModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    }

    const QModelIndex currentIndex = indexForViewRowState(state.current);
    if(currentIndex.isValid()) {
        selectionModel->setCurrentIndex(currentIndex, QItemSelectionModel::NoUpdate);
    }

    const QModelIndex topIndex = indexForViewRowState(state.top);
    if(topIndex.isValid()) {
        m_view->scrollTo(topIndex, QAbstractItemView::PositionAtTop);
    }
    else if(currentIndex.isValid()) {
        m_view->scrollTo(currentIndex, QAbstractItemView::EnsureVisible);
    }
    else {
        m_view->verticalScrollBar()->setValue(state.scrollValue);
    }

    m_remove->setEnabled(canRemoveSelected());
    m_clear->setEnabled(canClearQueue());
}

QueueViewer::ViewRowState QueueViewer::viewRowState(const QModelIndex& index) const
{
    ViewRowState rowState;
    if(!index.isValid()) {
        return rowState;
    }

    rowState.track       = index.data(QueueViewerItem::Track).value<PlaylistTrack>();
    rowState.queueItemId = index.data(QueueViewerItem::QueueItemId).toULongLong();
    if(!rowState.track.isValid()) {
        return {};
    }

    rowState.currentRow = m_model->queueIndex(index) < 0;
    if(rowState.currentRow) {
        rowState.occurrence = 1;
        return rowState;
    }

    for(int row{0}; row <= index.row(); ++row) {
        const QModelIndex candidate = m_model->index(row, 0, {});

        if(!candidate.isValid() || m_model->queueIndex(candidate) < 0) {
            continue;
        }

        if(candidate.data(QueueViewerItem::Track).value<PlaylistTrack>() == rowState.track) {
            ++rowState.occurrence;
        }
    }

    return rowState;
}

QModelIndex QueueViewer::indexForViewRowState(const ViewRowState& state) const
{
    if(!state.isValid()) {
        return {};
    }

    if(state.currentRow) {
        const QModelIndex currentIndex = m_model->index(0, 0, {});

        if(currentIndex.isValid() && m_model->queueIndex(currentIndex) < 0
           && currentIndex.data(QueueViewerItem::Track).value<PlaylistTrack>() == state.track) {
            return currentIndex;
        }
    }

    QModelIndex fallback;
    int occurrence{0};
    const int targetOccurrence = std::max(state.occurrence, 1);
    const int rowCount         = m_model->rowCount({});

    for(int row{0}; row < rowCount; ++row) {
        const QModelIndex candidate = m_model->index(row, 0, {});

        if(state.queueItemId != 0 && candidate.data(QueueViewerItem::QueueItemId).toULongLong() == state.queueItemId) {
            return candidate;
        }

        if(!candidate.isValid() || m_model->queueIndex(candidate) < 0) {
            continue;
        }

        if(candidate.data(QueueViewerItem::Track).value<PlaylistTrack>() != state.track) {
            continue;
        }

        if(++occurrence == targetOccurrence) {
            if(state.queueItemId == 0) {
                return candidate;
            }
            fallback = candidate;
        }
    }

    if(state.queueItemId != 0 && m_playerController->playbackQueue().item(state.queueItemId)) {
        return {};
    }

    return fallback;
}

bool QueueViewer::canRemoveSelected() const
{
    const auto selected = m_view->selectionModel()->selectedRows();
    return std::ranges::any_of(selected, [this](const QModelIndex& index) {
        if(m_playerController->playbackQueueMode() == PlaybackQueueMode::QueueAsPlaybackSource) {
            return index.data(QueueViewerItem::QueueItemId).toULongLong() != 0;
        }
        const auto track = index.data(QueueViewerItem::Track).value<PlaylistTrack>();
        return !m_playerController->currentIsQueueTrack() || track != m_playerController->currentPlaylistTrack();
    });
}

bool QueueViewer::canClearQueue() const
{
    return !m_playerController->playbackQueue().empty();
}

void QueueViewer::updateSelectedTracks() const
{
    TrackSelection selection;

    const auto selected = m_view->selectionModel()->selectedRows();
    selection.tracks.reserve(selected.size());
    selection.playlistIndexes.reserve(selected.size());
    selection.playlistEntryIds.reserve(selected.size());

    std::optional<UId> playlistId;
    bool playlistBacked{!selected.empty()};

    for(const QModelIndex& index : selected) {
        const auto playlistTrack = index.data(QueueViewerItem::Track).value<PlaylistTrack>();
        if(!playlistTrack.isValid()) {
            playlistBacked = false;
            continue;
        }

        selection.tracks.emplace_back(playlistTrack.track);

        if(playlistTrack.playlistId.isValid() && playlistTrack.indexInPlaylist >= 0
           && (!playlistId || *playlistId == playlistTrack.playlistId)) {
            playlistId = playlistTrack.playlistId;
            selection.playlistIndexes.emplace_back(playlistTrack.indexInPlaylist);
            selection.playlistEntryIds.emplace_back(playlistTrack.entryId);
        }
        else {
            playlistBacked = false;
        }
    }

    if(playlistBacked && playlistId && selection.playlistIndexes.size() == selection.tracks.size()) {
        selection.playlistId     = *playlistId;
        selection.playlistBacked = true;
    }
    else {
        selection.playlistIndexes.clear();
        selection.playlistEntryIds.clear();
    }

    m_selectionController->changeSelectedTracks(m_context, selection);
}

void QueueViewer::handleRowsChanged() const
{
    m_clear->setEnabled(canClearQueue());
    updateSortActionState();
    updateSelectedTracks();
}

void QueueViewer::removeSelectedTracks() const
{
    const auto selected = m_view->selectionModel()->selectedRows();
    if(selected.empty()) {
        return;
    }

    std::vector<PlaybackQueueItemId> ids;
    ids.reserve(selected.size());

    for(const QModelIndex& index : selected) {
        const auto track      = index.data(QueueViewerItem::Track).value<PlaylistTrack>();
        const auto id         = index.data(QueueViewerItem::QueueItemId).toULongLong();
        const bool isSequence = m_playerController->playbackQueueMode() == PlaybackQueueMode::QueueAsPlaybackSource;
        const bool isCurrent  = !isSequence && m_playerController->currentIsQueueTrack()
                             && track == m_playerController->currentPlaylistTrack();
        if(!isCurrent) {
            if(id != 0) {
                ids.emplace_back(id);
            }
        }
    }

    m_playerController->dequeueQueueItems(ids);
}

void QueueViewer::handleQueueTracksMoved(int row, const QList<int>& indexes) const
{
    const auto& queue = m_playerController->playbackQueue();
    if(queue.empty() || indexes.empty()) {
        return;
    }

    std::vector<PlaybackQueueItemId> ids;
    ids.reserve(indexes.size());
    for(const int index : indexes) {
        if(const auto* item = queue.item(index)) {
            ids.push_back(item->id);
        }
    }
    m_playerController->moveQueueItems(row, ids);
}

void QueueViewer::handleTracksDropped(int row, const QMimeData* mimeData) const
{
    TrackList tracks;

    if(const auto mimeTracks = TrackMimeData::tracksFrom(mimeData); mimeTracks && !mimeTracks->empty()) {
        tracks = *mimeTracks;
    }
    else if(mimeData) {
        tracks = Gui::tracksFromMimeData(m_playlistInteractor->library(),
                                         mimeData->data(QString::fromLatin1(Constants::Mime::TrackIds)));
    }

    QueueTracks queueTracks;
    for(const Track& track : tracks) {
        queueTracks.emplace_back(track);
    }
    insertQueueTracks(row, queueTracks);
}

void QueueViewer::handlePlaylistTracksDropped(int row, const QByteArray& mimeData) const
{
    const QueueTracks tracks = Gui::queueTracksFromMimeData(m_playlistInteractor->library(), mimeData);
    insertQueueTracks(row, tracks);
}

void QueueViewer::handleQueueDoubleClicked(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return;
    }

    const int queueIndex = m_model->queueIndex(index);
    if(queueIndex < 0) {
        return;
    }

    const auto queueItemId = index.data(QueueViewerItem::QueueItemId).toULongLong();
    if(m_playerController->playbackQueueMode() == PlaybackQueueMode::QueueAsPlaybackSource) {
        m_playerController->playQueueItem(queueItemId);
        return;
    }

    std::vector<int> indexes;
    indexes.reserve(queueIndex);
    std::ranges::copy(std::views::iota(0, queueIndex), std::back_inserter(indexes));
    m_playerController->dequeueTracks(indexes);
    m_playerController->next();
}

void QueueViewer::randomiseTracks(SortScope scope)
{
    reorderTracks(QueueReorder::Randomise, scope);
}

void QueueViewer::reverseTracks(SortScope scope)
{
    reorderTracks(QueueReorder::Reverse, scope);
}

void QueueViewer::sortTracks(const QString& script, SortScope scope)
{
    auto indexes = queueIndexesToSort(scope);
    if(indexes.size() < 2) {
        return;
    }

    const auto& queue        = m_playerController->playbackQueue();
    const auto items         = queue.items();
    const auto originalIds   = queueItemIds(items);
    const auto currentItemId = queue.currentItemId();
    const auto sortToken     = ++m_sortRequestToken;

    const auto queueItemTrack = [](const PlaybackQueueItem& item) -> const Track& {
        return item.track.track;
    };

    Utils::asyncExec([items, script, indexes, queueItemTrack]() {
        TrackSorter sorter;
        return queueItemIds(sorter.calcSortTracks(script, items, indexes, queueItemTrack));
    }).then(this, [this, originalIds, currentItemId, sortToken](std::vector<PlaybackQueueItemId> sortedIds) {
        const auto& currentQueue = m_playerController->playbackQueue();
        if(sortToken != m_sortRequestToken || currentQueue.currentItemId() != currentItemId
           || queueItemIds(currentQueue.items()) != originalIds) {
            return;
        }

        reorderTracks(std::move(sortedIds));
    });
}

void QueueViewer::reorderTracks(QueueReorder reorder, SortScope scope)
{
    ++m_sortRequestToken;

    auto indexes = queueIndexesToSort(scope);
    if(indexes.size() < 2) {
        return;
    }

    auto ids = queueItemIds(m_playerController->playbackQueue().items());

    std::vector<PlaybackQueueItemId> selectedIds;
    selectedIds.reserve(indexes.size());
    for(const int index : indexes) {
        if(index >= 0 && std::cmp_less(index, ids.size())) {
            selectedIds.push_back(ids.at(index));
        }
    }

    if(selectedIds.size() < 2) {
        return;
    }

    if(reorder == QueueReorder::Randomise) {
        const auto originalIds{selectedIds};
        std::ranges::shuffle(selectedIds, std::mt19937{std::random_device{}()});
        if(selectedIds == originalIds) {
            std::ranges::rotate(selectedIds, std::next(selectedIds.begin()));
        }
    }
    else {
        std::ranges::reverse(selectedIds);
    }

    for(size_t i{0}; i < selectedIds.size(); ++i) {
        ids.at(indexes.at(i)) = selectedIds.at(i);
    }

    reorderTracks(std::move(ids));
}

void QueueViewer::reorderTracks(std::vector<PlaybackQueueItemId> reorderedIds)
{
    if(reorderedIds.size() < 2 || reorderedIds == queueItemIds(m_playerController->playbackQueue().items())) {
        return;
    }

    m_playerController->reorderQueueItems(reorderedIds);
}

std::vector<int> QueueViewer::queueIndexesToSort(SortScope scope) const
{
    auto indexes         = selectedQueueIndexes();
    const int firstIndex = showsUpcomingTracks() ? m_playerController->playbackQueue().currentIndex() + 1 : 0;
    std::erase_if(indexes, [firstIndex](int index) { return index < firstIndex; });

    if(scope == SortScope::SelectedOrAll
       && (!indexes.empty() || (m_view->selectionModel() && m_view->selectionModel()->hasSelection()))) {
        return indexes;
    }

    indexes.clear();

    const auto itemCount = m_playerController->playbackQueue().items().size();
    indexes.reserve(itemCount);
    for(size_t i{static_cast<size_t>(std::max(firstIndex, 0))}; i < itemCount; ++i) {
        indexes.push_back(static_cast<int>(i));
    }

    return indexes;
}

std::vector<int> QueueViewer::selectedQueueIndexes() const
{
    std::vector<int> indexes;
    if(!m_view->selectionModel()) {
        return indexes;
    }

    const auto selected = m_view->selectionModel()->selectedRows();
    indexes.reserve(selected.size());

    for(const QModelIndex& index : selected) {
        const int queueIndex = m_model->queueIndex(index);
        if(queueIndex >= 0) {
            indexes.push_back(queueIndex);
        }
    }

    std::ranges::sort(indexes);
    indexes.erase(std::ranges::unique(indexes).begin(), indexes.end());

    return indexes;
}

void QueueViewer::insertQueueTracks(int row, const QueueTracks& tracksToInsert) const
{
    if(tracksToInsert.empty()) {
        return;
    }

    m_playerController->insertQueueTracks(row, tracksToInsert);
}

QueueViewer::ConfigData QueueViewer::configFromLayout(const QJsonObject& layout) const
{
    ConfigData config{defaultConfig()};

    if(layout.contains("LeftScript"_L1)) {
        config.leftScript = layout.value("LeftScript"_L1).toString();
    }
    if(layout.contains("RightScript"_L1)) {
        config.rightScript = layout.value("RightScript"_L1).toString();
    }
    if(layout.contains("ShowCurrent"_L1)) {
        config.showCurrent = layout.value("ShowCurrent"_L1).toBool();
    }
    if(layout.contains("ShowIcon"_L1)) {
        config.showIcon = layout.value("ShowIcon"_L1).toBool();
    }
    if(layout.contains("IconWidth"_L1) && layout.contains("IconHeight"_L1)) {
        config.iconSize = {layout.value("IconWidth"_L1).toInt(), layout.value("IconHeight"_L1).toInt()};
    }
    if(layout.contains("ArtworkCornerRadius"_L1)) {
        config.artworkCornerRadius = layout.value("ArtworkCornerRadius"_L1).toInt();
    }
    if(layout.contains("ShowHeader"_L1)) {
        config.showHeader = layout.value("ShowHeader"_L1).toBool();
    }
    if(layout.contains("ShowScrollbar"_L1)) {
        config.showScrollBar = layout.value("ShowScrollbar"_L1).toBool();
    }
    if(layout.contains("AlternatingRows"_L1)) {
        config.alternatingRows = layout.value("AlternatingRows"_L1).toBool();
    }
    if(layout.contains("DisplayMode"_L1)
       && layout.value("DisplayMode"_L1).toInt() == static_cast<int>(DisplayMode::UpcomingTracks)) {
        config.displayMode = DisplayMode::UpcomingTracks;
    }

    if(!config.iconSize.isValid()) {
        config.iconSize = factoryConfig().iconSize;
    }
    config.artworkCornerRadius = std::clamp(config.artworkCornerRadius, 0, 100);

    return config;
}

void QueueViewer::saveConfigToLayout(const ConfigData& config, QJsonObject& layout) const
{
    layout["LeftScript"_L1]          = config.leftScript;
    layout["RightScript"_L1]         = config.rightScript;
    layout["ShowCurrent"_L1]         = config.showCurrent;
    layout["ShowIcon"_L1]            = config.showIcon;
    layout["IconWidth"_L1]           = config.iconSize.width();
    layout["IconHeight"_L1]          = config.iconSize.height();
    layout["ArtworkCornerRadius"_L1] = config.artworkCornerRadius;
    layout["ShowHeader"_L1]          = config.showHeader;
    layout["ShowScrollbar"_L1]       = config.showScrollBar;
    layout["AlternatingRows"_L1]     = config.alternatingRows;
    layout["DisplayMode"_L1]         = static_cast<int>(config.displayMode);
}

void QueueViewer::saveTopLevelState()
{
    QJsonObject layoutData;
    saveLayoutData(layoutData);
    layoutData["Geometry"_L1] = QString::fromUtf8(saveGeometry().toBase64());

    FyStateSettings stateSettings;
    stateSettings.setValue(QueueViewerStateKey, layoutData);
}

void QueueViewer::loadTopLevelState()
{
    const FyStateSettings stateSettings;

    const QJsonObject layoutData = stateSettings.value(QueueViewerStateKey).toJsonObject();
    if(layoutData.isEmpty()) {
        m_topLevelStateLoaded = true;
        return;
    }

    loadLayoutData(layoutData);

    if(layoutData.contains("Geometry"_L1)) {
        restoreGeometry(QByteArray::fromBase64(layoutData.value("Geometry"_L1).toString().toUtf8()));
    }

    m_topLevelStateLoaded = true;
}
} // namespace Fooyin
