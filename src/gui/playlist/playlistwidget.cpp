/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <luket@pm.me>
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

#include "playlistwidget.h"

#include "contextmenuids.h"
#include "internalguisettings.h"
#include "playlist/playlistinteractor.h"
#include "playlist/presetregistry.h"
#include "playlistcolumnregistry.h"
#include "playlistcontroller.h"
#include "playlistdelegate.h"
#include "playlistsearchcontroller.h"
#include "playlistview.h"
#include "playlistwidgetsession.h"
#include "sortactionhandler.h"

#include <core/application.h>
#include <core/constants.h>
#include <core/coresettings.h>
#include <core/engine/enginehelpers.h>
#include <core/library/musiclibrary.h>
#include <core/library/tracksort.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <gui/contextmenuutils.h>
#include <gui/coverprovider.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/guistyleprovider.h>
#include <gui/guiutils.h>
#include <gui/iconloader.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/autoheaderview.h>
#include <gui/widgets/elapsedprogressdialog.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/async.h>
#include <utils/modelutils.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/signalthrottler.h>
#include <utils/stardelegate.h>
#include <utils/tooltipfilter.h>
#include <utils/utils.h>

#include <QAbstractItemModel>
#include <QActionGroup>
#include <QByteArray>
#include <QHeaderView>
#include <QItemSelection>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QPixmap>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <QVBoxLayout>

#include <array>
#include <map>
#include <set>
#include <utility>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

constexpr auto PlaylistLayoutProperty = "gui/playlist-layout"_L1;
constexpr auto PlaylistLayoutVersion  = 1;

namespace Fooyin {
using namespace Settings::Gui::Internal;

namespace {
struct DefaultPlaylistColumn
{
    int id;
    Qt::Alignment alignment;
    double width;
};

constexpr std::array DefaultPlaylistColumns{
    DefaultPlaylistColumn{.id = 8, .alignment = Qt::AlignCenter, .width = 0.06},
    DefaultPlaylistColumn{.id = 3, .alignment = Qt::AlignLeft, .width = 0.38},
    DefaultPlaylistColumn{.id = 0, .alignment = Qt::AlignRight, .width = 0.08},
    DefaultPlaylistColumn{.id = 1, .alignment = Qt::AlignLeft, .width = 0.38},
    DefaultPlaylistColumn{.id = 7, .alignment = Qt::AlignRight, .width = 0.10},
};

qsizetype selectedRowCount(const QAbstractItemView* view)
{
    if(!view || !view->selectionModel()) {
        return 0;
    }

    qsizetype count{0};
    const QItemSelection selection = view->selectionModel()->selection();

    for(const QItemSelectionRange& range : selection) {
        if(range.isValid() && range.left() <= 0 && range.right() >= 0) {
            count += range.bottom() - range.top() + 1;
        }
    }

    return count;
}

class PlaylistWidgetHost : public EditablePlaylistSessionHost
{
public:
    explicit PlaylistWidgetHost(PlaylistWidget* widget)
        : m_widget{widget}
    { }

    [[nodiscard]] ActionManager* actionManager() const override
    {
        return m_widget->actionManager();
    }
    [[nodiscard]] PresetRegistry* presetRegistry() const override
    {
        return m_widget->presetRegistry();
    }
    void handlePresetChanged(const PlaylistPreset& preset) override
    {
        m_widget->handlePresetChanged(preset);
    }
    void changePlaylistLayout(Playlist* previousPlaylist, Playlist* playlist) override
    {
        m_widget->changePlaylistLayout(previousPlaylist, playlist);
    }
    void setHeaderVisible(bool visible) override
    {
        m_widget->setHeaderVisible(visible);
    }
    void setScrollbarVisible(bool visible) override
    {
        m_widget->setScrollbarVisible(visible);
    }
    void setMiddleClickAction(TrackAction action) override
    {
        m_widget->setMiddleClickAction(action);
    }

    [[nodiscard]] PlaylistWidget* sessionWidget() override
    {
        return m_widget;
    }
    [[nodiscard]] bool sessionVisible() const override
    {
        return m_widget->isVisible();
    }
    [[nodiscard]] PlaylistController* playlistController() const override
    {
        return m_widget->playlistController();
    }
    [[nodiscard]] PlayerController* playerController() const override
    {
        return m_widget->playerController();
    }
    [[nodiscard]] MusicLibrary* musicLibrary() const override
    {
        return m_widget->musicLibrary();
    }
    [[nodiscard]] PlaylistInteractor* playlistInteractor() const override
    {
        return m_widget->playlistInteractor();
    }
    [[nodiscard]] SettingsManager* settingsManager() const override
    {
        return m_widget->settingsManager();
    }
    [[nodiscard]] SignalThrottler* resetThrottler() const override
    {
        return m_widget->resetThrottler();
    }
    [[nodiscard]] LibraryManager* libraryManager() const override
    {
        return m_widget->libraryManager();
    }
    [[nodiscard]] TrackSelectionController* selectionController() const override
    {
        return m_widget->selectionController();
    }
    [[nodiscard]] WidgetContext* playlistContext() const override
    {
        return m_widget->playlistContext();
    }
    [[nodiscard]] PlaylistModel* playlistModel() const override
    {
        return m_widget->playlistModel();
    }
    [[nodiscard]] PlaylistView* playlistView() const override
    {
        return m_widget->playlistView();
    }
    [[nodiscard]] const PlaylistWidgetLayoutState& layoutState() const override
    {
        return m_widget->layoutState();
    }
    [[nodiscard]] bool hasDelayedStateLoad() const override
    {
        return m_widget->hasDelayedStateLoad();
    }

    void resetModel() override
    {
        m_widget->resetModel();
    }
    void resetModelThrottled() const override
    {
        m_widget->resetModelThrottled();
    }
    void resetSort(bool force) override
    {
        m_widget->resetSort(force);
    }
    bool followCurrentTrack() override
    {
        return m_widget->followCurrentTrack();
    }
    void sessionHandleRestoredState() override
    {
        m_widget->sessionHandleRestoredState();
    }
    void clearDelayedStateLoad() override
    {
        m_widget->clearDelayedStateLoad();
    }
    void setDelayedStateLoad(QMetaObject::Connection connection) override
    {
        m_widget->setDelayedStateLoad(std::move(connection));
    }

private:
    PlaylistWidget* m_widget;
};
} // namespace

PlaylistWidget* PlaylistWidget::createMainPlaylist(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                                                   TrackSelectionController* selectionController,
                                                   CoverProvider* coverProvider, Application* core,
                                                   GuiStyleProvider* styleProvider, QWidget* parent)
{
    return new PlaylistWidget(actionManager, playlistInteractor, coverProvider, core, styleProvider,
                              selectionController, PlaylistWidgetSession::createEditable(), parent);
}

PlaylistWidget* PlaylistWidget::createDetachedPlaylistSearch(ActionManager* actionManager,
                                                             PlaylistInteractor* playlistInteractor,
                                                             TrackSelectionController* selectionController,
                                                             CoverProvider* coverProvider, Application* core,
                                                             GuiStyleProvider* styleProvider, QWidget* parent)
{
    return new PlaylistWidget(actionManager, playlistInteractor, coverProvider, core, styleProvider,
                              selectionController, PlaylistWidgetSession::createDetachedPlaylist(), parent);
}

PlaylistWidget* PlaylistWidget::createDetachedLibrarySearch(ActionManager* actionManager,
                                                            PlaylistInteractor* playlistInteractor,
                                                            TrackSelectionController* selectionController,
                                                            CoverProvider* coverProvider, Application* core,
                                                            GuiStyleProvider* styleProvider, QWidget* parent)
{
    return new PlaylistWidget(actionManager, playlistInteractor, coverProvider, core, styleProvider,
                              selectionController, PlaylistWidgetSession::createDetachedLibrary(), parent);
}

PlaylistWidget::~PlaylistWidget()
{
    resetSort();
    saveRememberedLayout(m_playlistController->currentPlaylist());
    m_session->destroy(sessionHost());
}

PlaylistView* PlaylistWidget::view() const
{
    return m_playlistView;
}

PlaylistModel* PlaylistWidget::model() const
{
    return m_model;
}

int PlaylistWidget::trackCount() const
{
    return m_session->renderedTrackCount(m_playlistController);
}

void PlaylistWidget::startPlayback()
{
    if(m_selectionController->hasTracks()) {
        m_selectionController->executeAction(TrackAction::Play, m_session->playbackOptions());
    }
}

QString PlaylistWidget::name() const
{
    return tr("Playlist");
}

QString PlaylistWidget::layoutName() const
{
    return u"Playlist"_s;
}

void PlaylistWidget::saveLayoutData(QJsonObject& layout)
{
    resetSort();

    if(auto* playlist = m_playlistController->currentPlaylist(); remembersLayout(playlist)) {
        saveRememberedLayout(playlist);
    }
    else {
        m_defaultLayoutState = captureLayoutState();
    }

    const auto& state       = m_defaultLayoutState.currentPreset.isValid() ? m_defaultLayoutState : m_layoutState;
    layout["Preset"_L1]     = state.currentPreset.id;
    layout["SingleMode"_L1] = state.singleMode;

    if(!state.columns.empty()) {
        QStringList columns;

        for(int i{0}; const auto& column : state.columns) {
            const auto alignment = std::cmp_less(i, state.columnAlignments.size())
                                     ? state.columnAlignments.at(static_cast<size_t>(i))
                                     : Qt::AlignLeft;
            ++i;
            QString colStr = QString::number(column.id);

            if(alignment != Qt::AlignLeft) {
                colStr += ":"_L1 + QString::number(alignment.toInt());
            }

            columns.push_back(colStr);
        }

        layout["Columns"_L1] = columns.join(u"|"_s);
    }

    if(!state.headerState.isEmpty()) {
        const QByteArray headerState = qCompress(state.headerState, 9);
        layout["HeaderState"_L1]     = QString::fromUtf8(headerState.toBase64());
    }
}

void PlaylistWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains("Preset"_L1)) {
        const int presetId = layout.value("Preset"_L1).toInt();
        if(const auto preset = m_presetRegistry->itemById(presetId)) {
            m_layoutState.currentPreset = preset.value();
            m_settings->fileSet(PlaylistCurrentPreset, presetId);
        }
    }
    if(layout.contains("SingleMode"_L1)) {
        m_layoutState.singleMode = layout.value("SingleMode"_L1).toBool();
    }

    if(layout.contains("Columns"_L1)) {
        m_layoutState.columns.clear();
        m_layoutState.columnAlignments.clear();

        const QString columnData    = layout.value("Columns"_L1).toString();
        const QStringList columnIds = columnData.split(u'|');

        for(int i{0}; const auto& columnId : columnIds) {
            const auto column = columnId.split(u':');

            if(const auto columnItem = m_columnRegistry->itemById(column.at(0).toInt())) {
                m_layoutState.columns.push_back(columnItem.value());

                if(column.size() > 1) {
                    const auto alignment = static_cast<Qt::Alignment>(column.at(1).toInt());
                    m_layoutState.columnAlignments.push_back(alignment);
                    m_model->changeColumnAlignment(i, alignment);
                }
                else {
                    m_layoutState.columnAlignments.emplace_back(Qt::AlignLeft);
                    m_model->changeColumnAlignment(i, Qt::AlignLeft);
                }
            }
            ++i;
        }

        updateSpans();
    }

    if(layout.contains("HeaderState"_L1)) {
        const auto headerState = layout.value("HeaderState"_L1).toString().toUtf8();

        if(!headerState.isEmpty() && headerState.isValidUtf8()) {
            const auto state          = QByteArray::fromBase64(headerState);
            m_layoutState.headerState = qUncompress(state);
        }
    }
}

void PlaylistWidget::finalise()
{
    setupConnections();
    setupActions();

    if(!layoutState().currentPreset.isValid()) {
        if(const auto preset = m_presetRegistry->itemById(m_settings->fileValue(PlaylistCurrentPreset).toInt())) {
            changePreset(preset.value());
        }
    }

    m_defaultLayoutState = captureLayoutState();
    changePlaylistLayout(nullptr, m_playlistController->currentPlaylist());

    m_header->setSectionsClickable(!layoutState().singleMode);
    m_header->setSortIndicatorShown(!layoutState().singleMode);

    if(!layoutState().singleMode) {
        setSingleMode(false);
    }
    else if(m_session->restoreCurrentPlaylistOnFinalise() && m_playlistController->currentPlaylist()) {
        m_session->changePlaylist(sessionHost(), nullptr, m_playlistController->currentPlaylist());
    }

    m_session->finalise(sessionHost());
}

void PlaylistWidget::searchEvent(const SearchRequest& request)
{
    m_session->searchEvent(sessionHost(), request);
}

bool PlaylistWidget::openIntegratedSearch()
{
    if(!isVisible()) {
        return false;
    }

    return m_searchController->open();
}

void PlaylistWidget::resetModel()
{
    m_searchController->markResultsOutOfDate();
    m_playlistView->playlistAboutToBeReset();

    Playlist* currentPlaylist = m_playlistController->currentPlaylist();

    const bool forceSortedAutoPlaylist
        = currentPlaylist && currentPlaylist->isAutoPlaylist() && currentPlaylist->forceSorted();
    const bool readOnly = m_session->hasSearch() || forceSortedAutoPlaylist;

    setReadOnly(readOnly, currentPlaylist && (!currentPlaylist->isAutoPlaylist() || !currentPlaylist->forceSorted()));

    if(m_session->canResetWithoutPlaylist() || currentPlaylist) {
        m_model->reset(layoutState().currentPreset,
                       layoutState().singleMode ? PlaylistColumnList{} : layoutState().columns,
                       m_session->modelPlaylist(currentPlaylist), m_session->modelTracks(currentPlaylist));
    }
}

void PlaylistWidget::resetModelThrottled() const
{
    if(!m_styleProvider->isResolved() || !layoutState().currentPreset.isValid()) {
        return;
    }

    m_resetThrottler->throttle();
}

void PlaylistWidget::changePreset(const PlaylistPreset& preset)
{
    m_layoutState.currentPreset = preset;
    if(!remembersLayout(m_playlistController->currentPlaylist())) {
        m_settings->fileSet(PlaylistCurrentPreset, preset.id);
    }
    m_playlistView->setExtendSpansIntoParents(m_layoutState.currentPreset.insetSubheadersToImageColumns);
    resetModelThrottled();
}

void PlaylistWidget::setReadOnly(bool readOnly, bool allowSorting)
{
    m_header->setSectionsClickable(!readOnly || allowSorting);
    updateMetadataEditTriggers(readOnly);
    m_session->applyReadOnlyState(sessionHost(), readOnly);
    updateSortActionState();
}

void PlaylistWidget::doubleClicked(const QModelIndex& index)
{
    if(index.isValid()) {
        startPlayback();
        m_playlistView->clearSelection();
    }
}

void PlaylistWidget::middleClicked(const QModelIndex& /*index*/)
{
    if(m_middleClickAction == TrackAction::None) {
        return;
    }

    m_session->queueSelectedTracks(sessionHost(), m_middleClickAction == TrackAction::SendToQueue, false);
}

void PlaylistWidget::resetSort(bool force)
{
    m_session->resetSortState(force);

    if(!m_session->isSorting()) {
        m_header->setSortIndicator(-1, Qt::AscendingOrder);
    }
}

void PlaylistWidget::setHeaderVisible(bool visible)
{
    m_header->setFixedHeight(visible ? QWIDGETSIZE_MAX : 0);
    m_header->adjustSize();
}

void PlaylistWidget::setScrollbarVisible(bool visible)
{
    m_playlistView->setVerticalScrollBarPolicy(visible ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
}

void PlaylistWidget::selectAll()
{
    QMetaObject::invokeMethod(m_playlistView, [view = m_playlistView]() { view->selectAll(); }, Qt::QueuedConnection);
}

void PlaylistWidget::handlePresetChanged(const PlaylistPreset& preset)
{
    if(m_defaultLayoutState.currentPreset.id == preset.id) {
        m_defaultLayoutState.currentPreset = preset;
    }
    if(m_layoutState.currentPreset.id == preset.id) {
        changePreset(preset);
    }
}

PlaylistWidgetLayoutState PlaylistWidget::captureLayoutState() const
{
    PlaylistWidgetLayoutState state{m_layoutState};

    state.columnAlignments.clear();
    state.columnAlignments.reserve(state.columns.size());

    for(int i{0}; std::cmp_less(i, state.columns.size()); ++i) {
        state.columnAlignments.push_back(m_model->columnAlignment(i));
    }

    if(!state.singleMode && m_header->count() > 0) {
        state.headerState = m_header->saveHeaderState();
    }

    return state;
}

QString PlaylistWidget::serialiseLayoutState(const PlaylistWidgetLayoutState& state) const
{
    QJsonObject layout;
    layout["Version"_L1]    = PlaylistLayoutVersion;
    layout["Preset"_L1]     = state.currentPreset.id;
    layout["SingleMode"_L1] = state.singleMode;

    QJsonArray columns;
    for(size_t i{0}; i < state.columns.size(); ++i) {
        QJsonObject column;
        column["Id"_L1] = state.columns.at(i).id;
        if(i < state.columnAlignments.size() && state.columnAlignments.at(i) != Qt::AlignLeft) {
            column["Alignment"_L1] = static_cast<int>(state.columnAlignments.at(i).toInt());
        }
        columns.append(column);
    }
    layout["Columns"_L1] = columns;

    if(!state.headerState.isEmpty()) {
        layout["HeaderState"_L1] = QString::fromUtf8(qCompress(state.headerState, 9).toBase64());
    }

    return QString::fromUtf8(QJsonDocument{layout}.toJson(QJsonDocument::Compact));
}

std::optional<PlaylistWidgetLayoutState> PlaylistWidget::deserialiseLayoutState(const QString& encoded) const
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(encoded.toUtf8(), &error);
    if(error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }

    const QJsonObject layout = document.object();
    if(layout.value("Version"_L1).toInt() != PlaylistLayoutVersion) {
        return {};
    }

    PlaylistWidgetLayoutState state;
    if(const auto preset = m_presetRegistry->itemById(layout.value("Preset"_L1).toInt())) {
        state.currentPreset = preset.value();
    }
    else {
        state.currentPreset = m_defaultLayoutState.currentPreset;
    }
    state.singleMode = layout.value("SingleMode"_L1).toBool();

    const QJsonArray columns = layout.value("Columns"_L1).toArray();
    for(const auto& value : columns) {
        const QJsonObject columnData = value.toObject();
        if(const auto column = m_columnRegistry->itemById(columnData.value("Id"_L1).toInt(-1))) {
            state.columns.push_back(column.value());
            state.columnAlignments.push_back(
                static_cast<Qt::Alignment>(columnData.value("Alignment"_L1).toInt(Qt::AlignLeft)));
        }
    }

    if(!state.singleMode && state.columns.empty()) {
        state.columns          = m_defaultLayoutState.columns;
        state.columnAlignments = m_defaultLayoutState.columnAlignments;
    }

    const QByteArray encodedState = layout.value("HeaderState"_L1).toString().toUtf8();
    if(!encodedState.isEmpty()) {
        state.headerState = qUncompress(QByteArray::fromBase64(encodedState));
    }

    if(!state.currentPreset.isValid()) {
        return {};
    }

    return state;
}

void PlaylistWidget::applyLayoutState(const PlaylistWidgetLayoutState& state)
{
    m_layoutState = state;
    if(!m_layoutState.singleMode) {
        ensureDefaultColumns(m_layoutState);
    }

    m_model->resetColumnAlignments();
    for(int i{0}; const Qt::Alignment alignment : m_layoutState.columnAlignments) {
        m_model->changeColumnAlignment(i++, alignment);
    }

    m_header->setSectionsClickable(!m_layoutState.singleMode);
    m_header->setSortIndicatorShown(!m_layoutState.singleMode);
    m_playlistView->setExtendSpansIntoParents(m_layoutState.currentPreset.insetSubheadersToImageColumns);

    if(!m_layoutState.singleMode) {
        QObject::connect(
            m_model, &QAbstractItemModel::modelReset, this,
            [this, headerState = m_layoutState.headerState]() {
                if(!headerState.isEmpty()) {
                    m_header->restoreHeaderState(headerState);
                }
                else {
                    applyDefaultHeaderConfiguration();
                }
            },
            Qt::SingleShotConnection);
    }

    updateSpans();
}

void PlaylistWidget::saveRememberedLayout(Playlist* playlist)
{
    if(playlist && remembersLayout(playlist)) {
        const QString encoded = serialiseLayoutState(captureLayoutState());
        const QString stored  = playlist->extraProperties().value(PlaylistLayoutProperty);

        if(stored != m_loadedPlaylistLayout && encoded == m_loadedPlaylistLayout) {
            return;
        }

        playlist->setExtraProperty(PlaylistLayoutProperty, encoded);
        m_loadedPlaylistLayout = encoded;
    }
}

bool PlaylistWidget::remembersLayout(const Playlist* playlist) const
{
    return m_session->capabilities().editablePlaylist && playlist && playlist->hasExtraProperty(PlaylistLayoutProperty);
}

void PlaylistWidget::changePlaylistLayout(Playlist* previousPlaylist, const Playlist* playlist)
{
    if(!m_session->capabilities().editablePlaylist) {
        return;
    }

    if(previousPlaylist) {
        if(remembersLayout(previousPlaylist)) {
            saveRememberedLayout(previousPlaylist);
        }
        else {
            m_defaultLayoutState = captureLayoutState();
        }
    }

    if(playlist && remembersLayout(playlist)) {
        const QString encoded = playlist->extraProperties().value(PlaylistLayoutProperty);
        if(const auto state = deserialiseLayoutState(encoded)) {
            m_loadedPlaylistLayout = encoded;
            applyLayoutState(state.value());
            return;
        }
    }

    m_loadedPlaylistLayout.clear();
    applyLayoutState(m_defaultLayoutState);
}

void PlaylistWidget::setMiddleClickAction(TrackAction action)
{
    m_middleClickAction = action;
}

bool PlaylistWidget::followCurrentTrack()
{
    const PlaylistTrack playingTrack = m_model->playingTrack();

    if(!playingTrack.track.isValid() || playingTrack.playlistId != m_playlistController->currentPlaylistId()) {
        return false;
    }

    QModelIndex modelIndex = m_model->indexAtPlaylistIndex(playingTrack.indexInPlaylist, true);
    while(modelIndex.isValid() && m_header->isSectionHidden(modelIndex.column())) {
        modelIndex = modelIndex.siblingAtColumn(modelIndex.column() + 1);
    }

    if(!modelIndex.isValid()) {
        return false;
    }

    if(modelIndex.data(PlaylistItem::ItemData).value<PlaylistTrack>().track.id()
       != m_playerController->currentTrackId()) {
        return false;
    }

    const QRect indexRect    = m_playlistView->visualRect(modelIndex);
    const QRect viewportRect = m_playlistView->viewport()->rect();

    if(indexRect.top() < 0 || viewportRect.bottom() - indexRect.bottom() < 0) {
        m_playlistView->scrollTo(modelIndex, QAbstractItemView::PositionAtCenter);
    }

    m_playlistView->setCurrentIndex(modelIndex);
    return true;
}

void PlaylistWidget::sessionHandleRestoredState()
{
    m_session->handleRestoredState(sessionHost());
}

bool PlaylistWidget::hasDelayedStateLoad() const
{
    return m_delayedStateLoad;
}

void PlaylistWidget::clearDelayedStateLoad()
{
    QObject::disconnect(m_delayedStateLoad);
    m_delayedStateLoad = {};
}

void PlaylistWidget::setDelayedStateLoad(QMetaObject::Connection connection)
{
    if(m_delayedStateLoad) {
        QObject::disconnect(m_delayedStateLoad);
    }
    m_delayedStateLoad = std::move(connection);
}

const PlaylistWidgetLayoutState& PlaylistWidget::layoutState() const
{
    return m_layoutState;
}

ActionManager* PlaylistWidget::actionManager() const
{
    return m_actionManager;
}

PresetRegistry* PlaylistWidget::presetRegistry() const
{
    return m_presetRegistry;
}

PlaylistController* PlaylistWidget::playlistController() const
{
    return m_playlistController;
}

PlayerController* PlaylistWidget::playerController() const
{
    return m_playerController;
}

MusicLibrary* PlaylistWidget::musicLibrary() const
{
    return m_library;
}

PlaylistInteractor* PlaylistWidget::playlistInteractor() const
{
    return m_playlistInteractor;
}

SettingsManager* PlaylistWidget::settingsManager() const
{
    return m_settings;
}

SignalThrottler* PlaylistWidget::resetThrottler() const
{
    return m_resetThrottler;
}

LibraryManager* PlaylistWidget::libraryManager() const
{
    return m_libraryManager;
}

TrackSelectionController* PlaylistWidget::selectionController() const
{
    return m_selectionController;
}

WidgetContext* PlaylistWidget::playlistContext() const
{
    return m_playlistContext;
}

PlaylistModel* PlaylistWidget::playlistModel() const
{
    return m_model;
}

PlaylistView* PlaylistWidget::playlistView() const
{
    return m_playlistView;
}

PlaylistWidgetSessionHost& PlaylistWidget::sessionHost()
{
    return *m_host;
}

EditablePlaylistSessionHost& PlaylistWidget::editableSessionHost()
{
    return *m_host;
}

void PlaylistWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const QPoint viewportPos = m_playlistView->viewport()->mapFromGlobal(event->globalPos());
    const bool invalidIndex  = !m_playlistView->indexAt(viewportPos).isValid();

    if(invalidIndex) {
        addSingleModeAction(menu);
        addCustomLayoutAction(menu);
        menu->addSeparator();
    }

    populateTrackContextMenu(menu, {.selectedCount = selectedRowCount(m_playlistView)});

    if(invalidIndex) {
        menu->addSeparator();
        addSettingsAction(menu);
    }

    menu->popup(event->globalPos());
}

void PlaylistWidget::keyPressEvent(QKeyEvent* event)
{
    const auto key = event->key();

    if(event == QKeySequence::SelectAll) {
        selectAll();
    }
    else if(key == Qt::Key_Enter || key == Qt::Key_Return) {
        startPlayback();
    }
    else if(key == Qt::Key_F3 && m_searchController->isOpen()) {
        m_searchController->navigate(event->modifiers() & Qt::ShiftModifier ? -1 : 1);
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

PlaylistWidget::PlaylistWidget(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                               CoverProvider* coverProvider, Application* core, GuiStyleProvider* styleProvider,
                               TrackSelectionController* selectionController,
                               std::unique_ptr<PlaylistWidgetSession> session, QWidget* parent)
    : FyWidget{parent}
    , m_actionManager{actionManager}
    , m_playlistInteractor{playlistInteractor}
    , m_playlistController{m_playlistInteractor->playlistController()}
    , m_coverProvider{coverProvider}
    , m_playerController{playlistInteractor->playerController()}
    , m_libraryManager{core->libraryManager()}
    , m_selectionController{selectionController}
    , m_library{playlistInteractor->library()}
    , m_settings{core->settingsManager()}
    , m_styleProvider{styleProvider}
    , m_settingsDialog{m_settings->settingsDialog()}
    , m_session{std::move(session)}
    , m_resetThrottler{new SignalThrottler(this)}
    , m_columnRegistry{m_playlistController->columnRegistry()}
    , m_presetRegistry{m_playlistController->presetRegistry()}
    , m_sortRegistry{core->sortingRegistry()}
    , m_layout{new QVBoxLayout(this)}
    , m_model{new PlaylistModel(m_playlistInteractor, core->audioLoader().get(), coverProvider, m_settings,
                                styleProvider, this)}
    , m_delgate{new PlaylistDelegate(this)}
    , m_starDelegate{nullptr}
    , m_playlistView{new PlaylistView(this)}
    , m_header{new AutoHeaderView(Qt::Horizontal, this)}
    , m_playlistContext{new WidgetContext(
          this, Context{IdList{Constants::Context::TrackSelection, Id{Constants::Context::Playlist}.append(id())}},
          this)}
    , m_middleClickAction{static_cast<TrackAction>(m_settings->value<PlaylistMiddleClick>())}
    , m_playAction{new QAction(tr("&Play"), this)}
    , m_sortActions{std::make_unique<SortActionHandler>(m_actionManager, m_sortRegistry, m_playlistContext->context(),
                                                        this)}
    , m_bgCoverRequestId{0}
    , m_bgImageMode{PlaylistBgImage::None}
    , m_bgCoverType{Track::Cover::Front}
    , m_host{std::make_unique<PlaylistWidgetHost>(this)}
    , m_searchController{
          new PlaylistSearchController(m_playlistController, m_model, m_playlistView, m_header, m_settings, this)}
{
    Gui::setThemeIcon(m_playAction, Constants::Icons::Play);

    const auto modeCaps = m_session->capabilities();

    m_layout->setContentsMargins({});
    m_layout->setSpacing(0);

    m_header->setStretchEnabled(true);
    m_header->setSectionsClickable(true);
    m_header->setSectionsMovable(true);
    m_header->setFirstSectionMovable(true);
    m_header->setContextMenuPolicy(Qt::CustomContextMenu);
    m_header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_playlistView->setModel(m_model);
    m_playlistView->setHeader(m_header);
    m_playlistView->setItemDelegate(m_delgate);
    m_playlistView->viewport()->setAcceptDrops(modeCaps.editablePlaylist);
    m_playlistView->viewport()->installEventFilter(new ToolTipFilter(this));
    m_playlistView->setSelectBeforeDrag(m_settings->value<Settings::Gui::DragOnlyAfterSelect>());
    m_settings->subscribe<Settings::Gui::DragOnlyAfterSelect>(m_playlistView, &PlaylistView::setSelectBeforeDrag);

    QObject::connect(m_searchController, &PlaylistSearchController::playCurrentRequested, this,
                     &PlaylistWidget::startPlayback);
    QObject::connect(m_searchController, &PlaylistSearchController::queueCurrentRequested, this,
                     [this]() { m_session->queueSelectedTracks(sessionHost(), false, false); });

    m_layout->addWidget(m_playlistView);
    m_layout->addWidget(m_searchController->widget());

    applyInitialViewSettings();
    applyBackgroundSettings();

    m_model->playingTrackChanged(m_playerController->currentPlaylistTrack());
    m_model->playStateChanged(m_playlistController->playState());
    applySessionTexts();
    setObjectName(PlaylistWidget::name());
    setFeature(ExclusiveSearch);
}

void PlaylistWidget::populateTrackContextMenu(QMenu* menu, const ContextMenuRequest& request)
{
    ContextMenuState state;
    state.hasSelection = request.selectedCount > 0;
    m_session->updateContextMenuState(sessionHost(), request, state);

    ContextMenuUtils::renderStaticContextMenu(
        menu, ContextMenuIds::Playlist::DefaultItems, m_settings->value<ContextMenuPlaylistLayout>(),
        m_settings->value<ContextMenuPlaylistDisabledSections>(),
        [&](const auto& id, QMenu* targetMenu, const auto& sectionEnabled) {
            if(id == QLatin1StringView{ContextMenuIds::Playlist::Play}) {
                if(state.hasSelection && sectionEnabled(ContextMenuIds::Playlist::Play)) {
                    targetMenu->addAction(m_playAction);
                }
                return;
            }
            if(id == QLatin1StringView{ContextMenuIds::Playlist::StopAfterThis}) {
                if(state.hasSelection && sectionEnabled(ContextMenuIds::Playlist::StopAfterThis) && state.showStopAfter
                   && m_session->stopAfterAction()) {
                    targetMenu->addAction(m_session->stopAfterAction());
                }
                return;
            }
            if(id == QLatin1StringView{ContextMenuIds::Playlist::Remove}) {
                if(state.hasSelection && state.showEditablePlaylistActions
                   && sectionEnabled(ContextMenuIds::Playlist::Remove)) {
                    if(auto* removeCmd = m_actionManager->command(Constants::Actions::Remove)) {
                        targetMenu->addAction(removeCmd->action());
                    }
                }
                return;
            }
            if(id == QLatin1StringView{ContextMenuIds::Playlist::Crop}) {
                if(state.hasSelection && state.showEditablePlaylistActions
                   && sectionEnabled(ContextMenuIds::Playlist::Crop)) {
                    if(auto* cropAction = m_session->cropAction()) {
                        targetMenu->addAction(cropAction);
                    }
                }
                return;
            }
            if(id == QLatin1StringView{ContextMenuIds::Playlist::Sort}) {
                if(state.hasSelection && sectionEnabled(ContextMenuIds::Playlist::Sort) && state.showSortMenu) {
                    addSortMenu(targetMenu, state.disableSortMenu);
                }
                return;
            }
            if(id == QLatin1StringView{ContextMenuIds::Playlist::Clipboard}) {
                if(sectionEnabled(ContextMenuIds::Playlist::Clipboard) && state.showClipboard) {
                    addClipboardMenu(targetMenu, state.hasSelection);
                }
                return;
            }
            if(id == QLatin1StringView{ContextMenuIds::Playlist::Presets}) {
                if(sectionEnabled(ContextMenuIds::Playlist::Presets)) {
                    addPresetMenu(targetMenu);
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::AddToPlaylist}) {
                if(state.hasSelection && sectionEnabled(Constants::Actions::AddToPlaylist)) {
                    auto* playlistMenu = new QMenu(tr("Add to another playlist"), targetMenu);
                    m_selectionController->addTrackAddToOtherPlaylistContextMenu(playlistMenu);
                    if(!playlistMenu->actions().empty()) {
                        targetMenu->addMenu(playlistMenu);
                    }
                    else {
                        playlistMenu->deleteLater();
                    }
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::AddToQueue}) {
                if(!state.hasSelection || !sectionEnabled(Constants::Actions::AddToQueue)) {
                    return;
                }

                if(state.usePlaylistQueueCommands && m_session->addToQueueAction()) {
                    targetMenu->addAction(m_session->addToQueueAction());
                }
                else if(auto* addQueueCmd = m_actionManager->command(Constants::Actions::AddToQueue)) {
                    targetMenu->addAction(addQueueCmd->action());
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::QueueNext}) {
                if(!state.hasSelection || !sectionEnabled(Constants::Actions::QueueNext)) {
                    return;
                }

                if(state.usePlaylistQueueCommands && m_session->queueNextAction()) {
                    targetMenu->addAction(m_session->queueNextAction());
                }
                else if(auto* queueNextCmd = m_actionManager->command(Constants::Actions::QueueNext)) {
                    targetMenu->addAction(queueNextCmd->action());
                }
                return;
            }
            if(id == QLatin1StringView{Constants::Actions::RemoveFromQueue}) {
                if(!state.hasSelection || !sectionEnabled(Constants::Actions::RemoveFromQueue)) {
                    return;
                }

                if(state.usePlaylistQueueCommands) {
                    if(auto* action = m_session->removeFromQueueAction(); action && action->isVisible()) {
                        targetMenu->addAction(action);
                    }
                }
                else if(auto* removeQueueCmd = m_actionManager->command(Constants::Actions::RemoveFromQueue)) {
                    targetMenu->addAction(removeQueueCmd->action());
                }
                return;
            }
            if(id == QLatin1StringView{ContextMenuIds::Playlist::TrackActions}) {
                if(state.hasSelection && sectionEnabled(ContextMenuIds::Playlist::TrackActions)) {
                    m_selectionController->addTrackContextMenu(targetMenu);
                }
                return;
            }
        });
}

void PlaylistWidget::showHeaderMenu(const QPoint& pos)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if(!layoutState().singleMode) {
        addColumnsMenu(menu);

        menu->addSeparator();
        m_header->addHeaderContextMenu(menu, mapToGlobal(pos));
        menu->addSeparator();
        m_header->addHeaderAlignmentMenu(menu, mapToGlobal(pos));

        auto* resetAction = new QAction(tr("Reset columns to default"), menu);
        QObject::connect(resetAction, &QAction::triggered, this, [this]() { resetColumnsToDefault(); });
        menu->addAction(resetAction);
    }

    addSingleModeAction(menu);

    if(m_session->capabilities().editablePlaylist) {
        addCustomLayoutAction(menu);
    }

    menu->addSeparator();
    addPresetMenu(menu);
    menu->addSeparator();
    addSettingsAction(menu);

    menu->popup(mapToGlobal(pos));
}

void PlaylistWidget::addSortMenu(QMenu* parent, bool disabled)
{
    m_sortActions->addSortMenu(parent, disabled, SortScope::SelectedOrAll);
}

void PlaylistWidget::refreshSortActions()
{
    m_sortActions->refreshPresetActions(tr("Sort the current playlist using this preset"));
    updateSortActionState();
}

void PlaylistWidget::updateSortActionState()
{
    const auto* playlist          = m_playlistController->currentPlaylist();
    const bool canReorderPlaylist = playlist && (!playlist->isAutoPlaylist() || !playlist->forceSorted());
    const bool canSortTracks      = canReorderPlaylist && playlist->trackCount() > 1;

    if(auto* randomiseAction = m_session->randomiseAction()) {
        randomiseAction->setEnabled(canSortTracks);
    }
    if(auto* reverseAction = m_session->reverseAction()) {
        reverseAction->setEnabled(canSortTracks);
    }
    if(m_sortActions) {
        m_sortActions->setEnabled(canSortTracks);
    }
}

void PlaylistWidget::addClipboardMenu(QMenu* parent, bool hasSelection) const
{
    if(hasSelection) {
        if(auto* cutAction = m_session->cutAction()) {
            parent->addAction(cutAction);
        }
        if(auto* copyAction = m_session->copyAction()) {
            parent->addAction(copyAction);
        }
    }

    if(auto* pasteAction = m_session->pasteAction(); pasteAction && !m_playlistController->clipboardEmpty()) {
        parent->addAction(pasteAction);
    }
}

void PlaylistWidget::addSingleModeAction(QMenu* parent)
{
    auto* columnModeAction = new QAction(tr("Single-column mode"), parent);
    columnModeAction->setCheckable(true);
    columnModeAction->setChecked(layoutState().singleMode);
    QObject::connect(columnModeAction, &QAction::triggered, this,
                     [this]() { setSingleMode(!layoutState().singleMode); });
    parent->addAction(columnModeAction);
}

void PlaylistWidget::addCustomLayoutAction(QMenu* parent)
{
    auto* currentPlaylist = m_playlistController->currentPlaylist();

    auto* action = new QAction(tr("Use custom layout for this playlist"), parent);
    action->setStatusTip(tr("Use a separate view layout instead of the default"));
    action->setCheckable(true);
    action->setChecked(remembersLayout(currentPlaylist));
    action->setEnabled(currentPlaylist != nullptr);

    QObject::connect(action, &QAction::toggled, this, [this, currentPlaylist](bool enabled) {
        if(!currentPlaylist || currentPlaylist != m_playlistController->currentPlaylist()) {
            return;
        }

        if(enabled) {
            resetSort();
            m_loadedPlaylistLayout = serialiseLayoutState(captureLayoutState());
            currentPlaylist->setExtraProperty(PlaylistLayoutProperty, m_loadedPlaylistLayout);
            return;
        }

        currentPlaylist->removeExtraProperty(PlaylistLayoutProperty);
        m_loadedPlaylistLayout.clear();
        applyLayoutState(m_defaultLayoutState);
        resetModelThrottled();
    });

    parent->addAction(action);
}

void PlaylistWidget::addPresetMenu(QMenu* parent)
{
    auto* presetsMenu = new QMenu(tr("Presets"), parent);

    const auto presets = m_presetRegistry->items();
    for(const auto& preset : presets) {
        auto* switchPreset = new QAction(preset.name, presetsMenu);
        if(preset == layoutState().currentPreset) {
            presetsMenu->setDefaultAction(switchPreset);
        }

        const int presetId = preset.id;
        QObject::connect(switchPreset, &QAction::triggered, this, [this, presetId]() {
            if(const auto item = m_presetRegistry->itemById(presetId)) {
                changePreset(item.value());
            }
        });

        presetsMenu->addAction(switchPreset);
    }

    auto* moreSettings = new QAction(tr("More…"), presetsMenu);
    QObject::connect(moreSettings, &QAction::triggered, this,
                     [this]() { m_settingsDialog->openAtPage(Constants::Page::PlaylistPresets); });

    presetsMenu->addSeparator();
    presetsMenu->addAction(moreSettings);

    parent->addMenu(presetsMenu);
}

void PlaylistWidget::addColumnsMenu(QMenu* parent)
{
    auto* columnsMenu = new QMenu(tr("Columns"), parent);
    auto* columnGroup = new QActionGroup{parent};
    columnGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::None);

    auto hasColumn = [this](int id) {
        return std::ranges::any_of(layoutState().columns,
                                   [id](const PlaylistColumn& column) { return column.id == id; });
    };

    for(const auto& column : m_columnRegistry->items()) {
        const bool columnVisible = hasColumn(column.id);
        if(!column.enabled && !columnVisible) {
            continue;
        }

        auto* columnAction = new QAction(column.name, columnsMenu);
        columnAction->setData(column.id);
        columnAction->setCheckable(true);
        columnAction->setChecked(columnVisible);
        columnsMenu->addAction(columnAction);
        columnGroup->addAction(columnAction);
    }

    QObject::connect(columnGroup, &QActionGroup::triggered, this,
                     [this](QAction* action) { setColumnVisible(action->data().toInt(), action->isChecked()); });

    auto* moreSettings = new QAction(tr("More…"), columnsMenu);
    QObject::connect(moreSettings, &QAction::triggered, this,
                     [this]() { m_settingsDialog->openAtPage(Constants::Page::PlaylistColumns); });
    columnsMenu->addSeparator();
    columnsMenu->addAction(moreSettings);

    parent->addMenu(columnsMenu);
}

void PlaylistWidget::addSettingsAction(QMenu* menu)
{
    if(!menu) {
        return;
    }

    auto* settings = new QAction(tr("Playlist settings…"), menu);
    QObject::connect(settings, &QAction::triggered, this,
                     [this]() { m_settingsDialog->openAtPage(Constants::Page::PlaylistAppearance); });
    menu->addAction(settings);
}

void PlaylistWidget::applyInitialViewSettings()
{
    setHeaderVisible(m_settings->value<PlaylistHeader>());
    setScrollbarVisible(m_settings->value<PlaylistScrollBar>());
    m_playlistView->setAlternatingRowColors(m_settings->value<PlaylistAltColours>());
}

void PlaylistWidget::applySessionTexts()
{
    m_playlistView->setEmptyText(m_session->emptyText());
    m_playlistView->setLoadingText(m_session->loadingText());
}

void PlaylistWidget::refreshViewStyle()
{
    const auto& resolvedStyle = m_styleProvider->style();

    const QFont playlistFont = resolvedStyle.font(u"Fooyin::PlaylistView"_s);

    m_playlistView->setFont(playlistFont);
    m_header->setFont(playlistFont);

    Gui::updateItemViewStyle(m_playlistView, resolvedStyle.palette);
    updateVisibleCoverPins();
}

void PlaylistWidget::updateMetadataEditTriggers(bool readOnly)
{
    const bool canEditMetadata
        = m_session->capabilities().editablePlaylist && !readOnly && m_settings->value<PlaylistInlineTagEditing>();
    m_playlistView->setEditTriggers(canEditMetadata ? QAbstractItemView::EditKeyPressed
                                                    : QAbstractItemView::NoEditTriggers);
}

void PlaylistWidget::handleColumnChanged(const PlaylistColumn& changedColumn)
{
    const auto updateColumn = [&changedColumn](PlaylistColumnList& columns) {
        auto existingIt = std::ranges::find_if(columns, [&changedColumn](const auto& column) {
            return (column.isDefault && changedColumn.isDefault && column.name == changedColumn.name)
                || column.id == changedColumn.id;
        });
        if(existingIt == columns.end()) {
            return false;
        }
        *existingIt = changedColumn;
        return true;
    };

    updateColumn(m_defaultLayoutState.columns);
    if(updateColumn(m_layoutState.columns)) {
        resetModelThrottled();
    }
}

void PlaylistWidget::handleColumnRemoved(int id)
{
    const auto removeColumn = [id](PlaylistWidgetLayoutState& state) {
        const auto it = std::ranges::find(state.columns, id, &PlaylistColumn::id);
        if(it == state.columns.end()) {
            return false;
        }
        const auto index = static_cast<int>(std::distance(state.columns.begin(), it));
        state.columns.erase(it);
        if(std::cmp_less(index, state.columnAlignments.size())) {
            state.columnAlignments.erase(state.columnAlignments.begin() + index);
        }
        return true;
    };

    removeColumn(m_defaultLayoutState);
    if(removeColumn(m_layoutState)) {
        resetModelThrottled();
    }
}

void PlaylistWidget::resetColumnsToDefault()
{
    m_layoutState.columns.clear();
    m_layoutState.headerState.clear();
    setSingleMode(false);
}

void PlaylistWidget::setColumnVisible(int columnId, bool visible)
{
    if(visible) {
        const bool alreadyVisible = std::ranges::any_of(
            m_layoutState.columns, [columnId](const PlaylistColumn& column) { return column.id == columnId; });
        if(alreadyVisible) {
            return;
        }

        if(const auto column = m_columnRegistry->itemById(columnId)) {
            m_layoutState.columns.push_back(column.value());
            updateSpans();
            changePreset(m_layoutState.currentPreset);
        }

        return;
    }

    auto colIt = std::ranges::find_if(m_layoutState.columns,
                                      [columnId](const PlaylistColumn& col) { return col.id == columnId; });
    if(colIt == m_layoutState.columns.end()) {
        return;
    }

    const int removedIndex = static_cast<int>(std::distance(m_layoutState.columns.begin(), colIt));
    m_layoutState.columns.erase(colIt);
    updateSpans();
    m_model->removeColumn(removedIndex);
}

void PlaylistWidget::setSingleMode(bool enabled)
{
    const bool wasSingleMode = std::exchange(m_layoutState.singleMode, enabled);
    if(!wasSingleMode && m_layoutState.singleMode) {
        m_layoutState.headerState = m_header->saveHeaderState();
        m_header->showSection(m_header->logicalIndex(0));
    }

    m_header->setSectionsClickable(!m_layoutState.singleMode);
    m_header->setSortIndicatorShown(!m_layoutState.singleMode);

    if(!m_layoutState.singleMode) {
        ensureDefaultColumns(m_layoutState);

        auto resetColumns = [this]() {
            applyDefaultHeaderConfiguration();
        };

        auto resetState = [this, resetColumns]() {
            if(!m_layoutState.headerState.isEmpty()) {
                m_header->restoreHeaderState(m_layoutState.headerState);
            }
            else {
                resetColumns();
            }
            updateSpans();
        };

        if(std::cmp_equal(m_header->count(), m_layoutState.columns.size())) {
            resetColumns();
            updateSpans();
        }
        else {
            QObject::connect(
                m_model, &QAbstractItemModel::modelReset, this, [resetState]() { resetState(); },
                Qt::SingleShotConnection);
        }
    }

    resetModelThrottled();
}

void PlaylistWidget::ensureDefaultColumns(PlaylistWidgetLayoutState& state) const
{
    if(!state.columns.empty()) {
        return;
    }

    for(const auto& defaultColumn : DefaultPlaylistColumns) {
        if(const auto column = m_columnRegistry->itemById(defaultColumn.id)) {
            state.columns.push_back(column.value());
            state.columnAlignments.push_back(defaultColumn.alignment);
        }
    }
}

void PlaylistWidget::applyDefaultHeaderConfiguration()
{
    m_model->resetColumnAlignments();
    m_header->resetSectionPositions();

    std::map<int, double> widths;
    for(int i{0}; const auto& defaultColumn : DefaultPlaylistColumns) {
        widths.emplace(i, defaultColumn.width);
        m_model->changeColumnAlignment(i++, defaultColumn.alignment);
    }
    m_header->setHeaderSectionWidths(widths);
}

void PlaylistWidget::updateSpans()
{
    auto isPixmap = [](const QString& field) {
        return field == QLatin1String(Constants::FrontCover) || field == QLatin1String(Constants::BackCover)
            || field == QLatin1String(Constants::ArtistPicture);
    };

    m_playlistView->setExtendSpansIntoParents(m_layoutState.currentPreset.insetSubheadersToImageColumns);

    bool hasRating{false};
    for(int i{0}; const auto& column : m_layoutState.columns) {
        m_playlistView->setSpan(i, isPixmap(column.field));

        if(column.field == QLatin1String{Constants::RatingEditor}) {
            hasRating = true;
            if(!m_starDelegate) {
                m_starDelegate = new StarDelegate(this);
            }
            m_playlistView->setItemDelegateForColumn(i, m_starDelegate);
        }
        else {
            m_playlistView->setItemDelegateForColumn(i, m_delgate);
        }
        ++i;
    }

    if(!hasRating && m_starDelegate) {
        m_starDelegate->deleteLater();
        m_starDelegate = nullptr;
    }
}

void PlaylistWidget::applyBackgroundSettings()
{
    if(!m_session->capabilities().editablePlaylist) {
        return;
    }

    PlaylistView::BackgroundOptions options;
    options.imageMode      = static_cast<PlaylistBgImage>(m_settings->value<PlaylistBackgroundImageMode>());
    options.scaling        = static_cast<PlaylistBgScaling>(m_settings->value<PlaylistBackgroundScaling>());
    options.position       = static_cast<PlaylistBgImagePosition>(m_settings->value<PlaylistBackgroundPosition>());
    options.maxSize        = m_settings->value<PlaylistBackgroundMaxSize>();
    options.blur           = m_settings->value<PlaylistBackgroundBlur>();
    options.opacity        = m_settings->value<PlaylistBackgroundOpacity>();
    options.fadeDurationMs = m_settings->value<PlaylistBackgroundFadeDuration>();
    options.fadeChanges    = options.fadeDurationMs > 0;

    m_playlistView->setBackgroundOptions(options);

    const bool modeChanged      = std::exchange(m_bgImageMode, options.imageMode) != options.imageMode;
    const auto coverType        = static_cast<Track::Cover>(m_settings->value<PlaylistBackgroundCoverType>());
    const bool coverTypeChanged = std::exchange(m_bgCoverType, coverType) != coverType;

    switch(options.imageMode) {
        case PlaylistBgImage::AlbumCover:
            if(modeChanged || coverTypeChanged || !m_bgCoverTrack.isValid()) {
                reloadBackgroundCover();
            }
            break;
        case PlaylistBgImage::Custom: {
            const QString customImage = m_settings->value<PlaylistBackgroundCustomImage>();
            if(!modeChanged && std::exchange(m_bgCustomImage, customImage) == customImage) {
                break;
            }
            ++m_bgCoverRequestId;
            m_bgCoverTrack = {};
            m_playlistView->setBackgroundPixmap(QPixmap{customImage});
            break;
        }
        case PlaylistBgImage::None:
            if(modeChanged) {
                ++m_bgCoverRequestId;
                m_bgCoverTrack = {};
                m_playlistView->setBackgroundPixmap({});
            }
            break;
    }
}

void PlaylistWidget::reloadBackgroundCover(const Track& track)
{
    if(!m_session->capabilities().editablePlaylist) {
        return;
    }

    if(static_cast<PlaylistBgImage>(m_settings->value<PlaylistBackgroundImageMode>()) != PlaylistBgImage::AlbumCover) {
        return;
    }

    const int requestId{++m_bgCoverRequestId};
    const Track coverTrack = track.isValid() ? track : m_playerController->currentTrack();
    if(!coverTrack.isValid()) {
        m_bgCoverTrack = {};
        m_playlistView->setBackgroundPixmap({});
        return;
    }

    m_bgCoverTrack = coverTrack;
    m_coverProvider->trackCoverFull(coverTrack, m_bgCoverType).then(this, [this, requestId](const QPixmap& cover) {
        if(requestId == m_bgCoverRequestId) {
            m_playlistView->setBackgroundPixmap(cover);
        }
    });
}

void PlaylistWidget::updateVisibleCoverPins()
{
    if(!m_playlistView->playlistLoaded()) {
        m_coverProvider->clearVisibleThumbnailKeys(this);
        return;
    }

    std::set<QString> keys;
    const int columnCount     = m_model->columnCount({});
    const auto visibleIndexes = m_playlistView->visibleIndexes();

    for(const QModelIndex& rowIndex : visibleIndexes) {
        if(!rowIndex.isValid()) {
            continue;
        }

        for(int column{0}; column < columnCount; ++column) {
            const QModelIndex index = rowIndex.siblingAtColumn(column);
            if(!index.isValid()) {
                continue;
            }

            const QString key = index.data(PlaylistItem::Role::CoverKey).toString();
            if(!key.isEmpty()) {
                keys.emplace(key);
            }
        }
    }

    m_coverProvider->setVisibleThumbnailKeys(this, keys);
}

void PlaylistWidget::handleMetadataWriteRequested(const TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    m_playlistView->setSingleWriteInProgress(true);

    WriteRequest request = m_library->writeTrackMetadata(tracks);
    if(!request.finished.isValid()) {
        m_playlistView->handleSingleWriteFinished();
        return;
    }

    request.finished.then(this, [this](const WriteResult& /*result*/) { m_playlistView->handleSingleWriteFinished(); });
}

void PlaylistWidget::handleBulkWriteRequested(const TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    m_playlistView->setBulkWriteInProgress(true);

    WriteRequest request = m_library->writeTrackMetadata(tracks);
    if(!request.finished.isValid()) {
        m_playlistView->handleBulkWriteFinished();
        return;
    }

    auto* dialog = new ElapsedProgressDialog(tr("Writing metadata…"), tr("Abort"), 0, 1, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(true);
    dialog->setMinimumDuration(2000ms);
    dialog->setBusy(true);
    dialog->setShowRemaining(false);
    dialog->setWindowTitle(tr("Writing Metadata"));
    dialog->setText(tr("Writing metadata to %Ln track(s)…", nullptr, static_cast<int>(tracks.size())));
    dialog->startTimer();

    request.finished.then(this, [this, dialog](const WriteResult& /*result*/) {
        if(dialog) {
            dialog->close();
        }

        m_playlistView->handleBulkWriteFinished();
    });

    QObject::connect(dialog, &ElapsedProgressDialog::cancelled, dialog, [cancel = request.cancel]() {
        if(cancel) {
            cancel();
        }
    });
}

void PlaylistWidget::setupConnections()
{
    // clang-format off
    QObject::connect(m_header, &QHeaderView::sectionCountChanged, m_playlistView, &PlaylistView::setupRatingDelegate);
    QObject::connect(m_header, &QHeaderView::sectionResized, this, [this](int column, int /*oldSize*/, int newSize) { m_model->setPixmapColumnSize(column, newSize); });
    QObject::connect(m_header, &QHeaderView::sortIndicatorChanged, this, [this](int column, Qt::SortOrder order) { m_session->sortColumn(sessionHost(), column, order); });
    QObject::connect(m_header, &QHeaderView::customContextMenuRequested, this, &PlaylistWidget::showHeaderMenu);
    QObject::connect(m_playlistView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        m_session->selectionChanged(sessionHost());
        updateSortActionState();
    });
    QObject::connect(m_playlistView, &PlaylistView::tracksRated, m_library, qOverload<const TrackList&>(&MusicLibrary::updateTrackStats));
    QObject::connect(m_playlistView, &PlaylistView::displayChanged, this, &PlaylistWidget::updateVisibleCoverPins);
    QObject::connect(m_playlistView->verticalScrollBar(), &QScrollBar::valueChanged, this, &PlaylistWidget::updateVisibleCoverPins);
    QObject::connect(m_playlistView->horizontalScrollBar(), &QScrollBar::valueChanged, this, &PlaylistWidget::updateVisibleCoverPins);
    QObject::connect(m_playlistView, &QAbstractItemView::doubleClicked, this, &PlaylistWidget::doubleClicked);
    QObject::connect(m_model, &QAbstractItemModel::modelAboutToBeReset, m_playlistView, &PlaylistView::playlistAboutToBeReset);
    QObject::connect(m_model, &QAbstractItemModel::modelAboutToBeReset, this, [this]() { m_coverProvider->clearVisibleThumbnailKeys(this); });
    QObject::connect(m_model, &QAbstractItemModel::modelAboutToBeReset, this, [this]() { m_session->handleAboutToBeReset(sessionHost()); });
    QObject::connect(m_model, &QAbstractItemModel::modelReset, this, &PlaylistWidget::updateVisibleCoverPins);
    QObject::connect(m_model, &PlaylistModel::loadingStateChanged, m_playlistView->viewport(), qOverload<>(&QWidget::update));
    QObject::connect(m_model, &PlaylistModel::loadingStateChanged, m_header->viewport(), qOverload<>(&QWidget::update));
    QObject::connect(m_model, &PlaylistModel::playlistLoaded, m_playlistView->viewport(), [this]() {
        m_playlistView->viewport()->update();
        updateVisibleCoverPins();
        m_session->handleDeferredFollowTrack(sessionHost());
    }, Qt::QueuedConnection);
    QObject::connect(m_coverProvider, &CoverProvider::coverAdded, this, &PlaylistWidget::updateVisibleCoverPins);
    QObject::connect(m_coverProvider, &CoverProvider::coverAdded, this, [this](const Track& track) {
        if(m_bgCoverTrack.isValid() && sameTrackIdentity(track, m_bgCoverTrack)) {
            reloadBackgroundCover(m_bgCoverTrack);
        }
    });
    QObject::connect(m_model, &PlaylistModel::metadataWriteRequested, this, &PlaylistWidget::handleMetadataWriteRequested);
    QObject::connect(m_playlistView, &PlaylistView::bulkWriteRequested, this, &PlaylistWidget::handleBulkWriteRequested);
    QObject::connect(m_playlistController, &PlaylistController::currentPlaylistTracksUpdated, m_model, [this](const std::vector<int>& indexes) { m_model->refreshTracks(indexes); });
    QObject::connect(m_playlistController, &PlaylistController::currentPlaylistUpdated, this, &PlaylistWidget::resetModelThrottled);
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &PlaylistWidget::reloadBackgroundCover);
    QObject::connect(m_playerController, &PlayerController::currentTrackUpdated, this, &PlaylistWidget::reloadBackgroundCover);

    QObject::connect(m_columnRegistry, &PlaylistColumnRegistry::itemRemoved, this, &PlaylistWidget::handleColumnRemoved);
    QObject::connect(m_columnRegistry, &PlaylistColumnRegistry::columnChanged, this, &PlaylistWidget::handleColumnChanged);

    QObject::connect(m_resetThrottler, &SignalThrottler::triggered, this, &PlaylistWidget::resetModel);
    // clang-format on

    m_session->setupConnections(sessionHost());

    m_styleProvider->subscribe(this, [this] {
        refreshViewStyle();
        resetModelThrottled();
    });

    m_settings->subscribe<Settings::Core::UseVariousForCompilations>(
        this, [this]() { m_session->changePlaylist(sessionHost(), m_playlistController->currentPlaylist(), nullptr); });
    m_settings->subscribe<PlaylistInlineTagEditing>(this, [this]() {
        const Playlist* currentPlaylist = m_playlistController->currentPlaylist();

        const bool forceSortedAutoPlaylist
            = currentPlaylist && currentPlaylist->isAutoPlaylist() && currentPlaylist->forceSorted();

        updateMetadataEditTriggers(m_session->hasSearch() || forceSortedAutoPlaylist);
    });

    m_settings->subscribe<PlaylistBackgroundImageMode>(this, &PlaylistWidget::applyBackgroundSettings);
    m_settings->subscribe<PlaylistBackgroundCustomImage>(this, &PlaylistWidget::applyBackgroundSettings);
    m_settings->subscribe<PlaylistBackgroundCoverType>(this, &PlaylistWidget::applyBackgroundSettings);
    m_settings->subscribe<PlaylistBackgroundScaling>(this, &PlaylistWidget::applyBackgroundSettings);
    m_settings->subscribe<PlaylistBackgroundPosition>(this, &PlaylistWidget::applyBackgroundSettings);
    m_settings->subscribe<PlaylistBackgroundMaxSize>(this, &PlaylistWidget::applyBackgroundSettings);
    m_settings->subscribe<PlaylistBackgroundBlur>(this, &PlaylistWidget::applyBackgroundSettings);
    m_settings->subscribe<PlaylistBackgroundOpacity>(this, &PlaylistWidget::applyBackgroundSettings);
    m_settings->subscribe<PlaylistBackgroundFadeDuration>(this, &PlaylistWidget::applyBackgroundSettings);
}

void PlaylistWidget::setupActions()
{
    m_playAction->setStatusTip(tr("Start playback of the selected track"));
    QObject::connect(m_playAction, &QAction::triggered, this, [this]() { startPlayback(); });

    m_actionManager->addContextObject(m_playlistContext);
    Context actionContext = m_playlistContext->context();
    actionContext.erase(Constants::Context::TrackSelection);

    auto* editMenu = m_actionManager->actionContainer(Constants::Menus::Edit);

    const QStringList editCategory = {tr("Edit")};
    m_session->setupActions(sessionHost(), editMenu, editCategory);

    if(auto* randomiseAction = m_session->randomiseAction()) {
        m_sortActions->registerRandomiseAction(randomiseAction, tr("Randomise the current playlist"));
    }
    if(auto* reverseAction = m_session->reverseAction()) {
        m_sortActions->registerReverseAction(reverseAction, tr("Reverse the current playlist"));
    }

    QObject::connect(m_sortActions.get(), &SortActionHandler::randomiseRequested, this,
                     [this](SortScope scope) { m_session->randomiseTracks(sessionHost(), scope); });
    QObject::connect(m_sortActions.get(), &SortActionHandler::reverseRequested, this,
                     [this](SortScope scope) { m_session->reverseTracks(sessionHost(), scope); });
    QObject::connect(
        m_sortActions.get(), &SortActionHandler::sortPresetRequested, this,
        [this](const QString& script, SortScope scope) { m_session->sortTracks(sessionHost(), script, scope); });
    QObject::connect(m_sortActions.get(), &SortActionHandler::settingsRequested, this,
                     [this]() { m_settingsDialog->openAtPage(Constants::Page::LibrarySorting); });

    QObject::connect(m_sortRegistry, &RegistryBase::itemAdded, this, &PlaylistWidget::refreshSortActions);
    QObject::connect(m_sortRegistry, &RegistryBase::itemChanged, this, &PlaylistWidget::refreshSortActions);
    QObject::connect(m_sortRegistry, &RegistryBase::itemRemoved, this, &PlaylistWidget::refreshSortActions);
    refreshSortActions();

    auto* selectAllAction = new QAction(tr("Select &all"), this);
    selectAllAction->setStatusTip(tr("Select all tracks in the current playlist"));
    auto* selectAllCmd = m_actionManager->registerAction(selectAllAction, Constants::Actions::SelectAll, actionContext);
    selectAllCmd->setCategories(editCategory);
    selectAllCmd->setDefaultShortcut(QKeySequence::SelectAll);
    editMenu->addAction(selectAllCmd);
    QObject::connect(selectAllAction, &QAction::triggered, this, [this]() { selectAll(); });
}
} // namespace Fooyin

#include "moc_playlistwidget.cpp"
