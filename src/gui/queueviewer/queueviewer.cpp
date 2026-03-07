/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "guiutils.h"
#include "internalguisettings.h"
#include "playlist/playlistinteractor.h"
#include "queueviewerconfigwidget.h"
#include "queueviewerdelegate.h"
#include "queueviewermodel.h"
#include "queueviewerview.h"

#include <core/player/playercontroller.h>
#include <gui/configdialog.h>
#include <gui/guiconstants.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/crypto.h>
#include <utils/settings/settingsmanager.h>

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
#include <ranges>

using namespace Qt::StringLiterals;

// Settings
constexpr auto QueueViewerShowIconKey    = u"PlaybackQueue/ShowIcon";
constexpr auto QueueViewerIconSizeKey    = u"PlaybackQueue/IconSize";
constexpr auto QueueViewerHeaderKey      = u"PlaybackQueue/Header";
constexpr auto QueueViewerScrollBarKey   = u"PlaybackQueue/Scrollbar";
constexpr auto QueueViewerAltColoursKey  = u"PlaybackQueue/AlternatingColours";
constexpr auto QueueViewerLeftScriptKey  = u"PlaybackQueue/LeftScript";
constexpr auto QueueViewerRightScriptKey = u"PlaybackQueue/RightScript";
constexpr auto QueueViewerShowCurrentKey = u"PlaybackQueue/ShowCurrent";

namespace Fooyin {
QueueViewer::QueueViewer(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                         std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_actionManager{actionManager}
    , m_playlistInteractor{playlistInteractor}
    , m_playerController{m_playlistInteractor->playerController()}
    , m_settings{settings}
    , m_view{new QueueViewerView(this)}
    , m_model{new QueueViewerModel(std::move(audioLoader), m_playerController, settings, this)}
    , m_context{new WidgetContext(this, Context{Id{"Context.QueueViewer."}.append(Utils::generateUniqueHash())}, this)}
    , m_remove{new QAction(tr("Remove"), this)}
    , m_removeCmd{nullptr}
    , m_clear{new QAction(tr("&Clear"), this)}
    , m_clearCmd{nullptr}
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    m_view->setModel(m_model);
    m_view->setItemDelegate(new QueueViewerDelegate(this));

    m_config = defaultConfig();
    applyConfig(m_config);

    setupActions();
    setupConnections();
    resetModel();
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

void QueueViewer::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if(m_removeCmd && m_view->selectionModel()->hasSelection()) {
        m_remove->setEnabled(canRemoveSelected());
        menu->addAction(m_removeCmd->action());
    }
    if(m_clearCmd) {
        m_clear->setEnabled(m_playerController->queuedTracksCount() > 0);
        menu->addAction(m_clearCmd->action());
    }

    auto* showCurrent = new QAction(tr("Show playing queue track"), menu);
    showCurrent->setCheckable(true);
    showCurrent->setChecked(m_config.showCurrent);
    QObject::connect(showCurrent, &QAction::triggered, showCurrent, [this](bool enabled) {
        auto config        = m_config;
        config.showCurrent = enabled;
        applyConfig(config);
    });

    menu->addSeparator();
    menu->addAction(showCurrent);
    menu->addSeparator();

    addConfigureAction(menu, false);

    menu->popup(event->globalPos());
}

void QueueViewer::setupActions()
{
    m_actionManager->addContextObject(m_context);

    m_remove->setStatusTip(tr("Remove the selected tracks from the playback queue"));
    m_removeCmd = m_actionManager->registerAction(m_remove, Constants::Actions::Remove, m_context->context());
    m_removeCmd->setDefaultShortcut(QKeySequence::Delete);
    QObject::connect(m_remove, &QAction::triggered, this, &QueueViewer::removeSelectedTracks);
    QObject::connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this]() { m_remove->setEnabled(canRemoveSelected()); });
    m_remove->setEnabled(canRemoveSelected());

    auto* editMenu = m_actionManager->actionContainer(Constants::Menus::Edit);

    m_clear->setStatusTip(tr("Remove all tracks in the playback queue"));
    m_clearCmd = m_actionManager->registerAction(m_clear, Constants::Actions::Clear, m_context->context());
    editMenu->addAction(m_clearCmd);
    QObject::connect(m_clear, &QAction::triggered, m_playerController, &PlayerController::clearQueue);
    m_clear->setEnabled(m_playerController->queuedTracksCount() > 0);

    auto* selectAllAction = new QAction(tr("&Select all"), this);
    selectAllAction->setStatusTip(tr("Select all tracks in the playback queue"));
    auto* selectAllCmd
        = m_actionManager->registerAction(selectAllAction, Constants::Actions::SelectAll, m_context->context());
    selectAllCmd->setDefaultShortcut(QKeySequence::SelectAll);
    editMenu->addAction(selectAllCmd);
    QObject::connect(selectAllAction, &QAction::triggered, m_view, &QAbstractItemView::selectAll);
}

void QueueViewer::setupConnections()
{
    QObject::connect(m_model, &QueueViewerModel::queueTracksMoved, this, &QueueViewer::handleQueueTracksMoved);
    QObject::connect(m_model, &QueueViewerModel::tracksDropped, this, &QueueViewer::handleTracksDropped);
    QObject::connect(m_model, &QueueViewerModel::playlistTracksDropped, this,
                     &QueueViewer::handlePlaylistTracksDropped);
    QObject::connect(m_playerController, &PlayerController::trackQueueChanged, this, &QueueViewer::resetModel);
    QObject::connect(m_playerController, &PlayerController::trackIndexesDequeued, this, &QueueViewer::resetModel);
    QObject::connect(m_playerController, &PlayerController::tracksQueued, this, &QueueViewer::resetModel);
    QObject::connect(m_playerController, &PlayerController::tracksDequeued, this, &QueueViewer::resetModel);
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &QueueViewer::resetModel);
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
    });
    QObject::connect(m_view, &QAbstractItemView::doubleClicked, this, &QueueViewer::handleQueueDoubleClicked);
}

void QueueViewer::resetModel() const
{
    const auto viewState = captureViewState();

    m_model->reset(m_playerController->playbackQueue().tracks());

    if(m_view->selectionModel()) {
        restoreViewState(viewState);
    }
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
    m_clear->setEnabled(m_playerController->queuedTracksCount() > 0);
}

QueueViewer::ViewRowState QueueViewer::viewRowState(const QModelIndex& index) const
{
    ViewRowState rowState;
    if(!index.isValid()) {
        return rowState;
    }

    rowState.track = index.data(QueueViewerItem::Track).value<PlaylistTrack>();
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

    QModelIndex firstMatch;
    int occurrence{0};

    for(int row{0}; row < m_model->rowCount({}); ++row) {
        const QModelIndex candidate = m_model->index(row, 0, {});

        if(!candidate.isValid() || m_model->queueIndex(candidate) < 0) {
            continue;
        }

        if(candidate.data(QueueViewerItem::Track).value<PlaylistTrack>() != state.track) {
            continue;
        }

        if(!firstMatch.isValid()) {
            firstMatch = candidate;
        }

        ++occurrence;

        if(occurrence == std::max(state.occurrence, 1)) {
            return candidate;
        }
    }

    return state.currentRow ? firstMatch : QModelIndex{};
}

bool QueueViewer::canRemoveSelected() const
{
    const auto selected = m_view->selectionModel()->selectedRows();
    return std::ranges::any_of(selected, [this](const QModelIndex& index) {
        const auto track = index.data(QueueViewerItem::Track).value<PlaylistTrack>();
        return !m_playerController->currentIsQueueTrack() || track != m_playerController->currentPlaylistTrack();
    });
}

void QueueViewer::handleRowsChanged() const
{
    m_clear->setEnabled(m_playerController->queuedTracksCount() > 0);
}

void QueueViewer::removeSelectedTracks() const
{
    const auto selected = m_view->selectionModel()->selectedRows();
    if(selected.empty()) {
        return;
    }

    std::vector<int> indexes;
    indexes.reserve(selected.size());

    for(const QModelIndex& index : selected) {
        const auto track = index.data(QueueViewerItem::Track).value<PlaylistTrack>();
        if(!m_playerController->currentIsQueueTrack() || track != m_playerController->currentPlaylistTrack()) {
            indexes.emplace_back(m_model->queueIndex(index));
        }
    }

    m_playerController->dequeueTracks(indexes);
}

void QueueViewer::handleQueueTracksMoved(int row, const QList<int>& indexes) const
{
    QueueTracks tracks = m_playerController->playbackQueue().tracks();
    if(tracks.empty() || indexes.empty()) {
        return;
    }

    std::vector<int> sortedIndexes;
    sortedIndexes.reserve(indexes.size());

    for(const int index : indexes) {
        if(index >= 0 && std::cmp_less(index, tracks.size())) {
            sortedIndexes.emplace_back(index);
        }
    }

    if(sortedIndexes.empty()) {
        return;
    }

    std::ranges::sort(sortedIndexes);
    sortedIndexes.erase(std::ranges::unique(sortedIndexes).begin(), sortedIndexes.end());

    QueueTracks movedTracks;
    movedTracks.reserve(sortedIndexes.size());

    for(const int index : sortedIndexes) {
        movedTracks.emplace_back(tracks.at(static_cast<size_t>(index)));
    }

    int insertRow = std::clamp(row, 0, static_cast<int>(tracks.size()));
    insertRow -= static_cast<int>(std::ranges::count_if(sortedIndexes, [row](int index) { return index < row; }));

    for(int sortedIndex : std::ranges::reverse_view(sortedIndexes)) {
        tracks.erase(tracks.begin() + sortedIndex);
    }

    tracks.insert(tracks.begin() + std::clamp(insertRow, 0, static_cast<int>(tracks.size())), movedTracks.begin(),
                  movedTracks.end());
    replaceQueueTracks(std::move(tracks));
}

void QueueViewer::handleTracksDropped(int row, const QByteArray& mimeData) const
{
    const TrackList tracks = Gui::tracksFromMimeData(m_playlistInteractor->library(), mimeData);

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

    std::vector<int> indexes;
    indexes.reserve(queueIndex);

    std::ranges::copy(std::views::iota(0, queueIndex), std::back_inserter(indexes));

    m_playerController->dequeueTracks(indexes);

    m_playerController->next();
}

void QueueViewer::replaceQueueTracks(QueueTracks tracks) const
{
    m_playerController->replaceTracks(tracks);
}

void QueueViewer::insertQueueTracks(int row, const QueueTracks& tracksToInsert) const
{
    if(tracksToInsert.empty()) {
        return;
    }

    QueueTracks tracks  = m_playerController->playbackQueue().tracks();
    const int insertRow = std::clamp(row, 0, static_cast<int>(tracks.size()));

    tracks.insert(tracks.begin() + insertRow, tracksToInsert.begin(), tracksToInsert.end());
    replaceQueueTracks(std::move(tracks));
}

QueueViewer::ConfigData QueueViewer::defaultConfig() const
{
    auto config{factoryConfig()};

    config.leftScript      = m_settings->fileValue(QueueViewerLeftScriptKey, config.leftScript).toString();
    config.rightScript     = m_settings->fileValue(QueueViewerRightScriptKey, config.rightScript).toString();
    config.showCurrent     = m_settings->fileValue(QueueViewerShowCurrentKey, config.showCurrent).toBool();
    config.showIcon        = m_settings->fileValue(QueueViewerShowIconKey, config.showIcon).toBool();
    config.iconSize        = m_settings->fileValue(QueueViewerIconSizeKey, config.iconSize).toSize();
    config.showHeader      = m_settings->fileValue(QueueViewerHeaderKey, config.showHeader).toBool();
    config.showScrollBar   = m_settings->fileValue(QueueViewerScrollBarKey, config.showScrollBar).toBool();
    config.alternatingRows = m_settings->fileValue(QueueViewerAltColoursKey, config.alternatingRows).toBool();

    return config;
}

QueueViewer::ConfigData QueueViewer::factoryConfig() const
{
    return {
        .leftScript      = u"%title%$crlf()%album%"_s,
        .rightScript     = u"%duration%"_s,
        .showCurrent     = true,
        .showIcon        = true,
        .iconSize        = QSize{36, 36},
        .showHeader      = true,
        .showScrollBar   = true,
        .alternatingRows = false,
    };
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
    m_settings->fileSet(QueueViewerHeaderKey, config.showHeader);
    m_settings->fileSet(QueueViewerScrollBarKey, config.showScrollBar);
    m_settings->fileSet(QueueViewerAltColoursKey, config.alternatingRows);
}

void QueueViewer::clearSavedDefaults() const
{
    m_settings->fileRemove(QueueViewerLeftScriptKey);
    m_settings->fileRemove(QueueViewerRightScriptKey);
    m_settings->fileRemove(QueueViewerShowCurrentKey);
    m_settings->fileRemove(QueueViewerShowIconKey);
    m_settings->fileRemove(QueueViewerIconSizeKey);
    m_settings->fileRemove(QueueViewerHeaderKey);
    m_settings->fileRemove(QueueViewerScrollBarKey);
    m_settings->fileRemove(QueueViewerAltColoursKey);
}

void QueueViewer::applyConfig(const ConfigData& config)
{
    m_config = config;

    m_model->setScripts(m_config.leftScript, m_config.rightScript);
    m_model->setShowCurrent(m_config.showCurrent);
    m_model->setShowIcon(m_config.showIcon);
    m_model->setIconSize(m_config.iconSize);

    m_view->changeIconSize(m_config.iconSize);
    m_view->header()->setHidden(!m_config.showHeader);
    m_view->verticalScrollBar()->setVisible(m_config.showScrollBar);
    m_view->setAlternatingRowColors(m_config.alternatingRows);

    QMetaObject::invokeMethod(m_view->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));
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
    if(layout.contains("ShowHeader"_L1)) {
        config.showHeader = layout.value("ShowHeader"_L1).toBool();
    }
    if(layout.contains("ShowScrollbar"_L1)) {
        config.showScrollBar = layout.value("ShowScrollbar"_L1).toBool();
    }
    if(layout.contains("AlternatingRows"_L1)) {
        config.alternatingRows = layout.value("AlternatingRows"_L1).toBool();
    }

    if(!config.iconSize.isValid()) {
        config.iconSize = factoryConfig().iconSize;
    }

    return config;
}

void QueueViewer::saveConfigToLayout(const ConfigData& config, QJsonObject& layout) const
{
    layout["LeftScript"_L1]      = config.leftScript;
    layout["RightScript"_L1]     = config.rightScript;
    layout["ShowCurrent"_L1]     = config.showCurrent;
    layout["ShowIcon"_L1]        = config.showIcon;
    layout["IconWidth"_L1]       = config.iconSize.width();
    layout["IconHeight"_L1]      = config.iconSize.height();
    layout["ShowHeader"_L1]      = config.showHeader;
    layout["ShowScrollbar"_L1]   = config.showScrollBar;
    layout["AlternatingRows"_L1] = config.alternatingRows;
}

void QueueViewer::openConfigDialog()
{
    showConfigDialog(new QueueViewerConfigDialog(this, this));
}
} // namespace Fooyin
