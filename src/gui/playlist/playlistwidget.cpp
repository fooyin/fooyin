/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "internalguisettings.h"
#include "playlist/playlistinteractor.h"
#include "playlistcolumnregistry.h"
#include "playlistcontroller.h"
#include "playlistdelegate.h"
#include "playlistview.h"
#include "playlistwidgetsession.h"

#include <core/application.h>
#include <core/constants.h>
#include <core/coresettings.h>
#include <core/library/musiclibrary.h>
#include <core/library/tracksort.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/autoheaderview.h>
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
#include <QApplication>
#include <QByteArray>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QStringList>

#include <utility>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Settings::Gui::Internal;

namespace {
QModelIndexList selectedRows(const QAbstractItemView* view)
{
    if(!view || !view->selectionModel()) {
        return {};
    }

    return view->selectionModel()->selectedRows();
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
    void followCurrentTrack() override
    {
        m_widget->followCurrentTrack();
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

PlaylistWidget::PlaylistWidget(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                               CoverProvider* coverProvider, Application* core,
                               std::unique_ptr<PlaylistWidgetSession> session, QWidget* parent)
    : FyWidget{parent}
    , m_actionManager{actionManager}
    , m_playlistInteractor{playlistInteractor}
    , m_playlistController{m_playlistInteractor->playlistController()}
    , m_playerController{playlistInteractor->playerController()}
    , m_libraryManager{core->libraryManager()}
    , m_selectionController{m_playlistController->selectionController()}
    , m_library{playlistInteractor->library()}
    , m_settings{core->settingsManager()}
    , m_settingsDialog{m_settings->settingsDialog()}
    , m_session{std::move(session)}
    , m_resetThrottler{new SignalThrottler(this)}
    , m_columnRegistry{m_playlistController->columnRegistry()}
    , m_presetRegistry{m_playlistController->presetRegistry()}
    , m_sortRegistry{core->sortingRegistry()}
    , m_layout{new QHBoxLayout(this)}
    , m_model{new PlaylistModel(m_playlistInteractor, coverProvider, m_settings, this)}
    , m_delgate{new PlaylistDelegate(this)}
    , m_starDelegate{nullptr}
    , m_playlistView{new PlaylistView(this)}
    , m_header{new AutoHeaderView(Qt::Horizontal, this)}
    , m_playlistContext{new WidgetContext(this, Context{Id{Constants::Context::Playlist}.append(id())}, this)}
    , m_middleClickAction{static_cast<TrackAction>(m_settings->value<PlaylistMiddleClick>())}
    , m_playAction{new QAction(Utils::iconFromTheme(Constants::Icons::Play), tr("&Play"), this)}
    , m_host{std::make_unique<PlaylistWidgetHost>(this)}
{
    const auto modeCaps = m_session->capabilities();

    m_layout->setContentsMargins(0, 0, 0, 0);

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

    m_layout->addWidget(m_playlistView);

    applyInitialViewSettings();

    m_model->playingTrackChanged(m_playlistController->currentTrack());
    m_model->playStateChanged(m_playlistController->playState());
    applySessionTexts();
    setObjectName(PlaylistWidget::name());
    setFeature(ExclusiveSearch);
}

PlaylistWidget* PlaylistWidget::createMainPlaylist(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                                                   CoverProvider* coverProvider, Application* core, QWidget* parent)
{
    return new PlaylistWidget(actionManager, playlistInteractor, coverProvider, core,
                              PlaylistWidgetSession::createEditable(), parent);
}

PlaylistWidget* PlaylistWidget::createDetachedPlaylistSearch(ActionManager* actionManager,
                                                             PlaylistInteractor* playlistInteractor,
                                                             CoverProvider* coverProvider, Application* core,
                                                             QWidget* parent)
{
    return new PlaylistWidget(actionManager, playlistInteractor, coverProvider, core,
                              PlaylistWidgetSession::createDetachedPlaylist(), parent);
}

PlaylistWidget* PlaylistWidget::createDetachedLibrarySearch(ActionManager* actionManager,
                                                            PlaylistInteractor* playlistInteractor,
                                                            CoverProvider* coverProvider, Application* core,
                                                            QWidget* parent)
{
    return new PlaylistWidget(actionManager, playlistInteractor, coverProvider, core,
                              PlaylistWidgetSession::createDetachedLibrary(), parent);
}

PlaylistWidget::~PlaylistWidget()
{
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
    layout["Preset"_L1]     = m_layoutState.currentPreset.id;
    layout["SingleMode"_L1] = m_layoutState.singleMode;

    if(!m_layoutState.columns.empty()) {
        QStringList columns;

        for(int i{0}; const auto& column : m_layoutState.columns) {
            const auto alignment = m_model->columnAlignment(i++);
            QString colStr       = QString::number(column.id);

            if(alignment != Qt::AlignLeft) {
                colStr += ":"_L1 + QString::number(alignment.toInt());
            }

            columns.push_back(colStr);
        }

        layout["Columns"_L1] = columns.join(u"|"_s);
    }

    resetSort();

    if(!m_layoutState.singleMode || !m_layoutState.headerState.isEmpty()) {
        QByteArray state         = m_layoutState.singleMode && !m_layoutState.headerState.isEmpty()
                                     ? m_layoutState.headerState
                                     : m_header->saveHeaderState();
        state                    = qCompress(state, 9);
        layout["HeaderState"_L1] = QString::fromUtf8(state.toBase64());
    }
}

void PlaylistWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains("Preset"_L1)) {
        const int presetId = layout.value("Preset"_L1).toInt();
        if(const auto preset = m_presetRegistry->itemById(presetId)) {
            m_layoutState.currentPreset = preset.value();
        }
    }
    if(layout.contains("SingleMode"_L1)) {
        m_layoutState.singleMode = layout.value("SingleMode"_L1).toBool();
    }

    if(layout.contains("Columns"_L1)) {
        m_layoutState.columns.clear();

        const QString columnData    = layout.value("Columns"_L1).toString();
        const QStringList columnIds = columnData.split(u'|');

        for(int i{0}; const auto& columnId : columnIds) {
            const auto column = columnId.split(u':');

            if(const auto columnItem = m_columnRegistry->itemById(column.at(0).toInt())) {
                m_layoutState.columns.push_back(columnItem.value());

                if(column.size() > 1) {
                    m_model->changeColumnAlignment(i, static_cast<Qt::Alignment>(column.at(1).toInt()));
                }
                else {
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

    m_header->setSectionsClickable(!layoutState().singleMode);
    m_header->setSortIndicatorShown(!layoutState().singleMode);

    if(!layoutState().singleMode) {
        setSingleMode(false);
        m_model->reset({});
    }
    else if(m_session->restoreCurrentPlaylistOnFinalise() && m_playlistController->currentPlaylist()) {
        m_session->changePlaylist(sessionHost(), nullptr, m_playlistController->currentPlaylist());
    }

    m_session->finalise(sessionHost());
}

void PlaylistWidget::searchEvent(const QString& search)
{
    m_session->searchEvent(sessionHost(), search);
}

void PlaylistWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const auto selected = selectedRows(m_playlistView);
    populateTrackContextMenu(menu, selected);

    menu->popup(event->globalPos());
}

void PlaylistWidget::populateTrackContextMenu(QMenu* menu, const QModelIndexList& selected)
{
    if(!menu) {
        return;
    }

    ContextMenuState state;
    state.hasSelection = !selected.empty();
    m_session->updateContextMenuState(sessionHost(), selected, state);

    if(state.hasSelection) {
        menu->addAction(m_playAction);

        if(state.showStopAfter && m_session->stopAfterAction()) {
            menu->addAction(m_session->stopAfterAction());
        }

        menu->addSeparator();

        if(state.showEditablePlaylistActions) {
            if(auto* removeCmd = m_actionManager->command(Constants::Actions::Remove)) {
                menu->addAction(removeCmd->action());
            }

            if(auto* cropAction = m_session->cropAction()) {
                menu->addAction(cropAction);
            }
        }

        if(state.showEditablePlaylistActions || state.showSortMenu) {
            if(state.showSortMenu) {
                addSortMenu(menu, state.disableSortMenu);
            }
            menu->addSeparator();
        }
    }

    if(state.showClipboard) {
        addClipboardMenu(menu, state.hasSelection);
        menu->addSeparator();
    }

    addPresetMenu(menu);
    menu->addSeparator();

    if(!state.hasSelection) {
        return;
    }

    if(state.usePlaylistQueueCommands) {
        if(auto* addQueueCmd = m_actionManager->command(Constants::Actions::AddToQueue)) {
            menu->addAction(addQueueCmd->action());
        }
        if(auto* addQueueNextCmd = m_actionManager->command(Constants::Actions::QueueNext)) {
            menu->addAction(addQueueNextCmd->action());
        }
        if(auto* removeQueueCmd = m_actionManager->command(Constants::Actions::RemoveFromQueue)) {
            menu->addAction(removeQueueCmd->action());
        }

        menu->addSeparator();
    }
    else {
        m_selectionController->addTrackQueueContextMenu(menu);
        menu->addSeparator();
    }

    m_selectionController->addTrackContextMenu(menu);
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

    QWidget::keyPressEvent(event);
}

void PlaylistWidget::setupConnections()
{
    // clang-format off
    QObject::connect(m_header, &QHeaderView::sectionCountChanged, m_playlistView, &PlaylistView::setupRatingDelegate);
    QObject::connect(m_header, &QHeaderView::sectionResized, this, [this](int column, int /*oldSize*/, int newSize) { m_model->setPixmapColumnSize(column, newSize); });
    QObject::connect(m_header, &QHeaderView::sortIndicatorChanged, this, [this](int column, Qt::SortOrder order) { m_session->sortColumn(sessionHost(), column, order); });
    QObject::connect(m_header, &QHeaderView::customContextMenuRequested, this, &PlaylistWidget::showHeaderMenu);
    QObject::connect(m_playlistView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() { m_session->selectionChanged(sessionHost()); });
    QObject::connect(m_playlistView, &PlaylistView::tracksRated, m_library, qOverload<const TrackList&>(&MusicLibrary::updateTrackStats));
    QObject::connect(m_playlistView, &QAbstractItemView::doubleClicked, this, &PlaylistWidget::doubleClicked);
    QObject::connect(m_model, &QAbstractItemModel::modelAboutToBeReset, this, [this]() { m_session->handleAboutToBeReset(sessionHost()); });
    QObject::connect(m_model, &PlaylistModel::playlistLoaded, m_playlistView->viewport(), qOverload<>(&QWidget::update));
    QObject::connect(m_playlistController, &PlaylistController::currentPlaylistTracksUpdated, m_model,
                     [this](const std::vector<int>& indexes) { m_model->refreshTracks(indexes); });
    QObject::connect(m_playlistController, &PlaylistController::currentPlaylistUpdated, this, &PlaylistWidget::resetModelThrottled);

    QObject::connect(m_columnRegistry, &PlaylistColumnRegistry::itemRemoved, this, &PlaylistWidget::handleColumnRemoved);
    QObject::connect(m_columnRegistry, &PlaylistColumnRegistry::columnChanged, this, &PlaylistWidget::handleColumnChanged);

    QObject::connect(m_resetThrottler, &SignalThrottler::triggered, this, &PlaylistWidget::resetModel);
    // clang-format on

    m_session->setupConnections(sessionHost());

    auto handleStyleChange = [this]() {
        refreshViewStyle();
    };
    m_settings->subscribe<Settings::Gui::Theme>(this, handleStyleChange);
    m_settings->subscribe<Settings::Gui::Style>(this, handleStyleChange);

    m_settings->subscribe<Settings::Core::UseVariousForCompilations>(
        this, [this]() { m_session->changePlaylist(sessionHost(), m_playlistController->currentPlaylist(), nullptr); });
}

void PlaylistWidget::setupActions()
{
    m_playAction->setStatusTip(tr("Start playback of the selected track"));
    QObject::connect(m_playAction, &QAction::triggered, this, [this]() { startPlayback(); });

    m_actionManager->addContextObject(m_playlistContext);

    auto* editMenu = m_actionManager->actionContainer(Constants::Menus::Edit);

    const QStringList editCategory = {tr("Edit")};
    m_session->setupActions(sessionHost(), editMenu, editCategory);

    auto* selectAllAction = new QAction(tr("Select &all"), this);
    selectAllAction->setStatusTip(tr("Select all tracks in the current playlist"));
    auto* selectAllCmd
        = m_actionManager->registerAction(selectAllAction, Constants::Actions::SelectAll, m_playlistContext->context());
    selectAllCmd->setCategories(editCategory);
    selectAllCmd->setDefaultShortcut(QKeySequence::SelectAll);
    editMenu->addAction(selectAllCmd);
    QObject::connect(selectAllAction, &QAction::triggered, this, [this]() { selectAll(); });
}

void PlaylistWidget::resetModel()
{
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
    m_resetThrottler->throttle();
}

void PlaylistWidget::setReadOnly(bool readOnly, bool allowSorting)
{
    m_header->setSectionsClickable(!readOnly || allowSorting);
    m_session->applyReadOnlyState(sessionHost(), readOnly);
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

void PlaylistWidget::handleColumnChanged(const PlaylistColumn& changedColumn)
{
    auto existingIt = std::ranges::find_if(m_layoutState.columns, [&changedColumn](const auto& column) {
        return (column.isDefault && changedColumn.isDefault && column.name == changedColumn.name)
            || column.id == changedColumn.id;
    });

    if(existingIt != m_layoutState.columns.end()) {
        *existingIt = changedColumn;
        resetModelThrottled();
    }
}

void PlaylistWidget::handleColumnRemoved(int id)
{
    PlaylistColumnList columns;
    std::ranges::copy_if(m_layoutState.columns, std::back_inserter(columns),
                         [id](const auto& column) { return column.id != id; });
    if(std::exchange(m_layoutState.columns, columns) != columns) {
        resetModelThrottled();
    }
}

void PlaylistWidget::handlePresetChanged(const PlaylistPreset& preset)
{
    if(m_layoutState.currentPreset.id == preset.id) {
        changePreset(preset);
    }
}

void PlaylistWidget::changePreset(const PlaylistPreset& preset)
{
    m_layoutState.currentPreset = preset;
    m_playlistView->setExtendSpansIntoParents(m_layoutState.currentPreset.insetSubheadersToImageColumns);
    resetModelThrottled();
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
        if(m_layoutState.columns.empty()) {
            for(const int id : {8, 3, 0, 1, 7}) {
                if(const auto column = m_columnRegistry->itemById(id)) {
                    m_layoutState.columns.push_back(column.value());
                }
            }
        }

        auto resetColumns = [this]() {
            m_model->resetColumnAlignments();
            m_header->resetSectionPositions();
            m_header->setHeaderSectionWidths({{0, 0.06}, {1, 0.38}, {2, 0.08}, {3, 0.38}, {4, 0.10}});
            m_model->changeColumnAlignment(0, Qt::AlignCenter);
            m_model->changeColumnAlignment(2, Qt::AlignRight);
            m_model->changeColumnAlignment(4, Qt::AlignRight);
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

    resetModel();
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

void PlaylistWidget::applyInitialViewSettings()
{
    setHeaderVisible(m_settings->value<PlaylistHeader>());
    setScrollbarVisible(m_settings->value<PlaylistScrollBar>());
    m_playlistView->setAlternatingRowColors(m_settings->value<PlaylistAltColours>());
    m_model->setFont(QApplication::font("Fooyin::PlaylistView"));
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

    auto* columnModeAction = new QAction(tr("Single-column mode"), menu);
    columnModeAction->setCheckable(true);
    columnModeAction->setChecked(layoutState().singleMode);
    QObject::connect(columnModeAction, &QAction::triggered, this,
                     [this]() { setSingleMode(!layoutState().singleMode); });
    menu->addAction(columnModeAction);

    menu->addSeparator();
    addPresetMenu(menu);

    menu->popup(mapToGlobal(pos));
}

void PlaylistWidget::applySessionTexts()
{
    m_playlistView->setEmptyText(m_session->emptyText());
    m_playlistView->setLoadingText(m_session->loadingText());
}

void PlaylistWidget::refreshViewStyle()
{
    m_model->setFont(QApplication::font("Fooyin::PlaylistView"));
    resetModelThrottled();
}

void PlaylistWidget::addSortMenu(QMenu* parent, bool disabled)
{
    auto* sortMenu = new QMenu(tr("Sort"), parent);

    if(disabled) {
        sortMenu->setEnabled(false);
    }
    else {
        const auto& groups = m_sortRegistry->items();
        for(const auto& script : groups) {
            auto* switchSort = new QAction(script.name, sortMenu);
            QObject::connect(switchSort, &QAction::triggered, this,
                             [this, script]() { m_session->sortTracks(sessionHost(), script.script); });
            sortMenu->addAction(switchSort);
        }
    }

    parent->addMenu(sortMenu);
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
        auto* columnAction = new QAction(column.name, columnsMenu);
        columnAction->setData(column.id);
        columnAction->setCheckable(true);
        columnAction->setChecked(hasColumn(column.id));
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

void PlaylistWidget::setMiddleClickAction(TrackAction action)
{
    m_middleClickAction = action;
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

void PlaylistWidget::followCurrentTrack()
{
    const auto [track, playlistId, index] = m_model->playingTrack();

    if(!track.isValid() || playlistId != m_playlistController->currentPlaylistId()) {
        return;
    }

    QModelIndex modelIndex = m_model->indexAtPlaylistIndex(index, true);
    while(modelIndex.isValid() && m_header->isSectionHidden(modelIndex.column())) {
        modelIndex = modelIndex.siblingAtColumn(modelIndex.column() + 1);
    }

    if(modelIndex.data(PlaylistItem::ItemData).value<PlaylistTrack>().track.id()
       != m_playerController->currentTrackId()) {
        return;
    }

    if(!modelIndex.isValid()) {
        return;
    }

    const QRect indexRect    = m_playlistView->visualRect(modelIndex);
    const QRect viewportRect = m_playlistView->viewport()->rect();

    if(indexRect.top() < 0 || viewportRect.bottom() - indexRect.bottom() < 0) {
        m_playlistView->scrollTo(modelIndex, QAbstractItemView::PositionAtCenter);
    }

    m_playlistView->setCurrentIndex(modelIndex);
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

PlaylistWidgetSessionHost& PlaylistWidget::sessionHost()
{
    return *m_host;
}

EditablePlaylistSessionHost& PlaylistWidget::editableSessionHost()
{
    return *m_host;
}
} // namespace Fooyin

#include "moc_playlistwidget.cpp"
