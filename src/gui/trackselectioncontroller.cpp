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

#include <gui/trackselectioncontroller.h>

#include "artwork/artworkexporter.h"
#include "contextmenuids.h"
#include "internalguisettings.h"
#include "playlist/playlistcontroller.h"
#include "statusevent.h"

#include <core/library/libraryutils.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <gui/contextmenuutils.h>
#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFileInfo>
#include <QMainWindow>
#include <QMenuBar>
#include <QPointer>

#include <functional>
#include <ranges>
#include <unordered_map>

using namespace Qt::StringLiterals;

constexpr auto TempSelectionPlaylist = "␟TempSelectionPlaylist␟";
constexpr auto SeparatorIdPrefix     = "separator:"_L1;

namespace {
QString promptForArtworkPath(QWidget* parent)
{
    static const QStringList imagePatterns = {u"*.png"_s, u"*.jpg"_s, u"*.jpeg"_s, u"*.webp"_s, u"*.bmp"_s, u"*.gif"_s};
    const QString filter                   = QObject::tr("Images") + u" (%1)"_s.arg(imagePatterns.join(u' '));
    return QFileDialog::getOpenFileName(parent, QObject::tr("Open Image"), QDir::homePath(), filter, nullptr,
                                        QFileDialog::DontResolveSymlinks);
}
} // namespace

namespace Fooyin {
namespace {
enum class MenuNodeType : uint8_t
{
    Root = 0,
    Action,
    Submenu,
    Separator
};

struct MenuNode
{
    MenuNodeType type{MenuNodeType::Root};
    QPointer<QObject> owner;
    Id id;
    QString title;
    TrackContextMenuArea area{TrackContextMenuArea::Track};
    TrackSelectionController::TrackContextMenuRenderer renderer;
    std::vector<std::unique_ptr<MenuNode>> children;
    MenuNode* parent{nullptr};
};

struct TopLevelRenderEntry
{
    const MenuNode* node{nullptr};
    bool isBoundary{false};
};
} // namespace

class TrackSelectionControllerPrivate : public QObject
{
    Q_OBJECT

public:
    TrackSelectionControllerPrivate(TrackSelectionController* self, ActionManager* actionManager,
                                    AudioLoader* audioLoader, SettingsManager* settings,
                                    PlaylistController* playlistController);

    void setupBuiltInMenus();

    [[nodiscard]] MenuNode* rootForArea(TrackContextMenuArea area);

    bool registerSubmenu(QObject* owner, TrackContextMenuArea area, const Id& parentId, const Id& id,
                         const QString& title, const Id& beforeId = {});
    bool registerAction(QObject* owner, TrackContextMenuArea area, const Id& parentId, const Id& id,
                        const QString& title, const TrackSelectionController::TrackContextMenuRenderer& renderer,
                        const Id& beforeId = {});
    bool registerSeparator(TrackContextMenuArea area, const Id& parentId, const Id& id = {}, const Id& beforeId = {});

    void renderArea(QMenu* menu, TrackContextMenuArea area, const TrackSelection& selection);
    void renderNode(QMenu* menu, const MenuNode& node, const TrackSelection& selection) const;
    void renderChildNodes(QMenu* menu, const std::vector<std::unique_ptr<MenuNode>>& nodes,
                          const TrackSelection& selection) const;
    void appendNodeInfo(const MenuNode& node, std::vector<TrackContextMenuNodeInfo>& nodes) const;
    [[nodiscard]] QStringList topLevelLayout(TrackContextMenuArea area) const;
    [[nodiscard]] std::vector<TopLevelRenderEntry> orderedRootEntries(const MenuNode& root) const;

    void refreshDisabledNodes();
    [[nodiscard]] bool isNodeDisabled(const Id& id) const;
    [[nodiscard]] static bool hasVisibleActions(const QMenu* menu);

    bool hasTracks() const;
    bool hasContextTracks() const;

    WidgetContext* contextObject(QWidget* widget) const;
    bool addContextObject(WidgetContext* context);
    void removeContextObject(WidgetContext* context);
    void updateActiveContext(QWidget* widget);

    void handleActions(PlaylistAction::ActionOptions options, Playlist* playlist = nullptr) const;
    void sendToNewPlaylist(PlaylistAction::ActionOptions options, const QString& playlistName) const;
    void sendToCurrentPlaylist(PlaylistAction::ActionOptions options) const;
    void addToCurrentPlaylist(bool startPlaybackIfStopped = false) const;
    void addToActivePlaylist() const;
    void addToPlaylist(const UId& playlistId, const TrackList& tracks) const;
    void addPlaylistTargets(QMenu* menu, const TrackSelection& selection,
                            const std::optional<UId>& excludedPlaylistId = {}) const;
    void startPlayback(PlaylistAction::ActionOptions options);
    void addToQueue() const;
    void queueNext() const;
    void openFolder(const TrackSelection& selection) const;
    void copyLocation(const TrackSelection& selection) const;
    void copyDirectoryPath(const TrackSelection& selection) const;
    void searchArtwork(const TrackSelection& selection, bool quick) const;
    void extractArtwork(const TrackSelection& selection) const;
    void attachArtwork(const TrackSelection& selection, Track::Cover coverType, QWidget* parent) const;
    void removeArtwork(const TrackSelection& selection) const;
    void openProperties(const TrackSelection& selection) const;
    void sendToQueue(PlaylistAction::ActionOptions options) const;
    [[nodiscard]] QueueTracks queueTracksForSelection(const TrackSelection& selection) const;
    [[nodiscard]] bool canDequeue(const TrackSelection& selection) const;
    [[nodiscard]] bool allTracksInSameFolder(const TrackSelection& selection) const;
    [[nodiscard]] bool canWrite(const TrackSelection& selection) const;

    const TrackSelection* currentSelection() const;

    void updateActionState();

    TrackSelectionController* m_self;

    ActionManager* m_actionManager;
    AudioLoader* m_audioLoader;
    SettingsManager* m_settings;
    PlaylistController* m_playlistController;
    PlaylistHandler* m_playlistHandler;

    std::unordered_map<QWidget*, WidgetContext*> m_contextWidgets;
    std::unordered_map<WidgetContext*, TrackSelection> m_contextSelection;
    WidgetContext* m_activeContext{nullptr};
    TrackSelection m_tracks;
    std::optional<TrackSelection> m_menuSelection;
    QPointer<QMenu> m_menuSelectionMenu;
    Playlist* m_tempPlaylist{nullptr};

    MenuNode m_trackRoot;
    MenuNode m_queueRoot;
    MenuNode m_playlistRoot;
    std::unordered_map<Id, MenuNode*, Id::IdHash> m_menuNodes;
    IdSet m_disabledNodeIds;

    QAction* m_addCurrent;
    QAction* m_addActive;
    QAction* m_sendCurrent;
    QAction* m_sendNew;
    QAction* m_addToQueue;
    QAction* m_queueNext;
    QAction* m_removeFromQueue;
    QAction* m_openFolder;
    QAction* m_copyLocation;
    QAction* m_copyDirectoryPath;
    QAction* m_searchArtwork;
    QAction* m_extractArtwork;
    QAction* m_attachFrontArtwork;
    QAction* m_attachBackArtwork;
    QAction* m_attachArtistArtwork;
    QAction* m_removeArtwork;
    QAction* m_openProperties;
};

TrackSelectionControllerPrivate::TrackSelectionControllerPrivate(TrackSelectionController* self,
                                                                 ActionManager* actionManager, AudioLoader* audioLoader,
                                                                 SettingsManager* settings,
                                                                 PlaylistController* playlistController)
    : QObject{self}
    , m_self{self}
    , m_actionManager{actionManager}
    , m_audioLoader{audioLoader}
    , m_settings{settings}
    , m_playlistController{playlistController}
    , m_playlistHandler{m_playlistController->playlistHandler()}
    , m_trackRoot{.type     = MenuNodeType::Root,
                  .owner    = m_self,
                  .id       = Constants::Menus::Context::TrackSelection,
                  .title    = tr("Track actions"),
                  .area     = TrackContextMenuArea::Track,
                  .renderer = {},
                  .children = {},
                  .parent   = nullptr}
    , m_queueRoot{.type     = MenuNodeType::Root,
                  .owner    = m_self,
                  .id       = Constants::Menus::Context::TrackQueue,
                  .title    = tr("Playback queue"),
                  .area     = TrackContextMenuArea::Queue,
                  .renderer = {},
                  .children = {},
                  .parent   = nullptr}
    , m_playlistRoot{.type     = MenuNodeType::Root,
                     .owner    = m_self,
                     .id       = Constants::Menus::Context::TracksPlaylist,
                     .title    = tr("Playlist actions"),
                     .area     = TrackContextMenuArea::Playlist,
                     .renderer = {},
                     .children = {},
                     .parent   = nullptr}
    , m_addCurrent{new QAction(tr("Add to current playlist"), self)}
    , m_addActive{new QAction(tr("Add to active playlist"), self)}
    , m_sendCurrent{new QAction(tr("Replace current playlist"), self)}
    , m_sendNew{new QAction(tr("Create new playlist"), self)}
    , m_addToQueue{new QAction(tr("Add to playback queue"), self)}
    , m_queueNext{new QAction(tr("Queue to play next"), self)}
    , m_removeFromQueue{new QAction(tr("Remove from playback queue"), self)}
    , m_openFolder{new QAction(tr("Open containing folder"), self)}
    , m_copyLocation{new QAction(tr("Copy file path"), self)}
    , m_copyDirectoryPath{new QAction(tr("Copy directory path"), self)}
    , m_searchArtwork{new QAction(tr("Search for artwork…"), self)}
    , m_extractArtwork{new QAction(tr("Auto-extract artwork to files"), self)}
    , m_attachFrontArtwork{new QAction(tr("Front cover…"), self)}
    , m_attachBackArtwork{new QAction(tr("Back cover…"), self)}
    , m_attachArtistArtwork{new QAction(tr("Artist picture…"), self)}
    , m_removeArtwork{new QAction(tr("Remove all artwork"), self)}
    , m_openProperties{new QAction(tr("Properties"), self)}
{
    m_menuNodes.emplace(m_trackRoot.id, &m_trackRoot);
    m_menuNodes.emplace(m_queueRoot.id, &m_queueRoot);
    m_menuNodes.emplace(m_playlistRoot.id, &m_playlistRoot);

    setupBuiltInMenus();
    refreshDisabledNodes();
    m_settings->subscribe<Settings::Gui::Internal::ContextMenuTrackDisabledSections>(
        this, &TrackSelectionControllerPrivate::refreshDisabledNodes);
    updateActionState();
}

void TrackSelectionControllerPrivate::setupBuiltInMenus()
{
    Gui::setThemeIcon(m_addToQueue, Constants::Icons::Add);
    Gui::setThemeIcon(m_removeFromQueue, Constants::Icons::Remove);

    const QStringList tracksCategory = {tr("Tracks")};
    const QStringList queueCategory  = {tr("Tracks"), tr("Queue")};

    m_addCurrent->setStatusTip(tr("Append selected tracks to the current playlist"));
    auto* addCurrentCmd = m_actionManager->registerAction(m_addCurrent, Constants::Actions::AddToCurrent);
    addCurrentCmd->setCategories(tracksCategory);
    QObject::connect(m_addCurrent, &QAction::triggered, m_self, [this]() { addToCurrentPlaylist(); });
    registerAction(m_self, TrackContextMenuArea::Playlist, m_playlistRoot.id, Constants::Actions::AddToCurrent,
                   m_addCurrent->text(), [this](QMenu* menu, const TrackSelection&) { menu->addAction(m_addCurrent); });

    m_addActive->setStatusTip(tr("Append selected tracks to the active playlist"));
    auto* addActiveCmd = m_actionManager->registerAction(m_addActive, Constants::Actions::AddToActive);
    addActiveCmd->setCategories(tracksCategory);
    QObject::connect(m_addActive, &QAction::triggered, m_self, [this]() { addToActivePlaylist(); });
    registerAction(m_self, TrackContextMenuArea::Playlist, m_playlistRoot.id, Constants::Actions::AddToActive,
                   m_addActive->text(), [this](QMenu* menu, const TrackSelection&) { menu->addAction(m_addActive); });

    m_sendCurrent->setStatusTip(tr("Replace contents of the current playlist with the selected tracks"));
    auto* sendCurrentCmd = m_actionManager->registerAction(m_sendCurrent, Constants::Actions::SendToCurrent);
    sendCurrentCmd->setCategories(tracksCategory);
    QObject::connect(m_sendCurrent, &QAction::triggered, m_self, [this]() {
        if(hasContextTracks()) {
            const auto& selection = m_contextSelection.at(m_activeContext);
            sendToCurrentPlaylist(selection.playbackOnSend ? PlaylistAction::StartPlayback : PlaylistAction::Switch);
        }
    });
    registerAction(m_self, TrackContextMenuArea::Playlist, m_playlistRoot.id, Constants::Actions::SendToCurrent,
                   m_sendCurrent->text(),
                   [this](QMenu* menu, const TrackSelection&) { menu->addAction(m_sendCurrent); });

    m_sendNew->setStatusTip(tr("Create a new playlist containing the selected tracks"));
    auto* sendNewCmd = m_actionManager->registerAction(m_sendNew, Constants::Actions::SendToNew);
    sendNewCmd->setCategories(tracksCategory);
    QObject::connect(m_sendNew, &QAction::triggered, m_self, [this]() {
        if(hasContextTracks()) {
            const auto& selection = m_contextSelection.at(m_activeContext);
            const auto options    = PlaylistAction::Switch
                                  | (selection.playbackOnSend ? PlaylistAction::StartPlayback : PlaylistAction::Switch);
            sendToNewPlaylist(static_cast<PlaylistAction::ActionOptions>(options), {});
        }
    });
    registerAction(m_self, TrackContextMenuArea::Playlist, m_playlistRoot.id, Constants::Actions::SendToNew,
                   m_sendNew->text(), [this](QMenu* menu, const TrackSelection&) { menu->addAction(m_sendNew); });

    m_addToQueue->setStatusTip(tr("Add the selected tracks to the playback queue"));
    auto* addQueueCmd = m_actionManager->registerAction(m_addToQueue, Constants::Actions::AddToQueue);
    addQueueCmd->setCategories(queueCategory);
    QObject::connect(m_addToQueue, &QAction::triggered, m_self, [this]() {
        if(const auto* selection = m_self->selectedSelection()) {
            m_playlistController->playerController()->queueTracks(queueTracksForSelection(*selection));
            updateActionState();
        }
    });
    registerAction(m_self, TrackContextMenuArea::Queue, m_queueRoot.id, Constants::Actions::AddToQueue,
                   m_addToQueue->text(), [this](QMenu* menu, const TrackSelection&) { menu->addAction(m_addToQueue); });

    m_queueNext->setStatusTip(tr("Add the selected tracks to the front of the playback queue"));
    auto* queueNextCmd = m_actionManager->registerAction(m_queueNext, Constants::Actions::QueueNext);
    queueNextCmd->setCategories(queueCategory);
    QObject::connect(m_queueNext, &QAction::triggered, m_self, [this]() {
        if(const auto* selection = m_self->selectedSelection()) {
            m_playlistController->playerController()->queueTracksNext(queueTracksForSelection(*selection));
            updateActionState();
        }
    });
    registerAction(m_self, TrackContextMenuArea::Queue, m_queueRoot.id, Constants::Actions::QueueNext,
                   m_queueNext->text(), [this](QMenu* menu, const TrackSelection&) { menu->addAction(m_queueNext); });

    m_removeFromQueue->setStatusTip(tr("Remove the selected tracks from the playback queue"));
    auto* removeQueueCmd = m_actionManager->registerAction(m_removeFromQueue, Constants::Actions::RemoveFromQueue);
    removeQueueCmd->setCategories(queueCategory);
    QObject::connect(m_removeFromQueue, &QAction::triggered, m_self, [this]() {
        if(const auto* selection = m_self->selectedSelection()) {
            m_playlistController->playerController()->dequeueTracks(queueTracksForSelection(*selection));
            updateActionState();
        }
    });
    registerAction(m_self, TrackContextMenuArea::Queue, m_queueRoot.id, Constants::Actions::RemoveFromQueue,
                   m_removeFromQueue->text(), [this](QMenu* menu, const TrackSelection& selection) {
                       if(canDequeue(selection)) {
                           menu->addAction(m_removeFromQueue);
                       }
                   });

    m_openFolder->setStatusTip(tr("Open the directory containing the selected tracks"));
    auto* openFolderCmd = m_actionManager->registerAction(m_openFolder, Constants::Actions::OpenFolder);
    openFolderCmd->setCategories(tracksCategory);
    QObject::connect(m_openFolder, &QAction::triggered, m_self, [this]() {
        if(const auto* selection = m_self->selectedSelection()) {
            openFolder(*selection);
        }
    });
    registerAction(m_self, TrackContextMenuArea::Track, m_trackRoot.id, Constants::Actions::OpenFolder,
                   m_openFolder->text(), [this](QMenu* menu, const TrackSelection&) { menu->addAction(m_openFolder); });

    m_copyLocation->setStatusTip(tr("Copy the file paths of the selected tracks"));
    auto* copyLocationCmd = m_actionManager->registerAction(m_copyLocation, Constants::Actions::CopyLocation);
    copyLocationCmd->setCategories(tracksCategory);
    QObject::connect(m_copyLocation, &QAction::triggered, m_self, [this]() {
        if(const auto* selection = m_self->selectedSelection()) {
            copyLocation(*selection);
        }
    });
    registerAction(m_self, TrackContextMenuArea::Track, m_trackRoot.id, Constants::Actions::CopyLocation,
                   m_copyLocation->text(),
                   [this](QMenu* menu, const TrackSelection&) { menu->addAction(m_copyLocation); });

    m_copyDirectoryPath->setStatusTip(tr("Copy the containing directories of the selected tracks"));
    auto* copyDirectoryPathCmd
        = m_actionManager->registerAction(m_copyDirectoryPath, Constants::Actions::CopyDirectoryPath);
    copyDirectoryPathCmd->setCategories(tracksCategory);
    QObject::connect(m_copyDirectoryPath, &QAction::triggered, m_self, [this]() {
        if(const auto* selection = m_self->selectedSelection()) {
            copyDirectoryPath(*selection);
        }
    });
    registerAction(m_self, TrackContextMenuArea::Track, m_trackRoot.id, Constants::Actions::CopyDirectoryPath,
                   m_copyDirectoryPath->text(),
                   [this](QMenu* menu, const TrackSelection&) { menu->addAction(m_copyDirectoryPath); });

    registerSubmenu(m_self, TrackContextMenuArea::Track, m_trackRoot.id, Constants::Menus::Context::Artwork,
                    TrackSelectionController::tr("Artwork"));

    m_searchArtwork->setStatusTip(tr("Search for artwork for the selected tracks"));
    auto* searchArtworkCmd = m_actionManager->registerAction(m_searchArtwork, Constants::Actions::SearchArtwork);
    searchArtworkCmd->setCategories(tracksCategory);
    QObject::connect(m_searchArtwork, &QAction::triggered, m_self, [this]() {
        if(const auto* selection = m_self->selectedSelection()) {
            searchArtwork(*selection, false);
        }
    });
    registerAction(m_self, TrackContextMenuArea::Track, Constants::Menus::Context::Artwork,
                   Constants::Actions::SearchArtwork, m_searchArtwork->text(),
                   [this](QMenu* menu, const TrackSelection&) { menu->addAction(m_searchArtwork); });

    registerSeparator(TrackContextMenuArea::Track, Constants::Menus::Context::Artwork,
                      ContextMenuIds::TrackSelection::ArtworkSearchSeparator);

    m_extractArtwork->setStatusTip(
        tr("Extract embedded artwork for the selected tracks to files in their directories without prompting"));
    auto* extractArtworkCmd = m_actionManager->registerAction(m_extractArtwork, Constants::Actions::ExportArtwork);
    extractArtworkCmd->setCategories(tracksCategory);
    QObject::connect(m_extractArtwork, &QAction::triggered, m_self, [this]() {
        if(const auto* selection = m_self->selectedSelection()) {
            extractArtwork(*selection);
        }
    });
    registerAction(m_self, TrackContextMenuArea::Track, Constants::Menus::Context::Artwork,
                   Constants::Actions::ExportArtwork, m_extractArtwork->text(),
                   [this](QMenu* menu, const TrackSelection&) { menu->addAction(m_extractArtwork); });

    registerSubmenu(m_self, TrackContextMenuArea::Track, Constants::Menus::Context::Artwork,
                    Constants::Menus::Context::ArtworkAttach, tr("Attach image"));

    m_attachFrontArtwork->setStatusTip(tr("Attach an image file as the front cover for the selected tracks"));
    QObject::connect(m_attachFrontArtwork, &QAction::triggered, m_self, [this]() {
        const auto* selection = m_self->selectedSelection();
        if(!selection) {
            return;
        }
        attachArtwork(*selection, Track::Cover::Front, Utils::getMainWindow());
    });
    registerAction(m_self, TrackContextMenuArea::Track, Constants::Menus::Context::ArtworkAttach,
                   Id{u"%1.Front"_s.arg(QString::fromLatin1(Constants::Menus::Context::ArtworkAttach))},
                   m_attachFrontArtwork->text(),
                   [this](QMenu* menu, const TrackSelection&) { menu->addAction(m_attachFrontArtwork); });

    m_attachBackArtwork->setStatusTip(tr("Attach an image file as the back cover for the selected tracks"));
    QObject::connect(m_attachBackArtwork, &QAction::triggered, m_self, [this]() {
        const auto* selection = m_self->selectedSelection();
        if(!selection) {
            return;
        }
        attachArtwork(*selection, Track::Cover::Back, Utils::getMainWindow());
    });
    registerAction(m_self, TrackContextMenuArea::Track, Constants::Menus::Context::ArtworkAttach,
                   Id{u"%1.Back"_s.arg(QString::fromLatin1(Constants::Menus::Context::ArtworkAttach))},
                   m_attachBackArtwork->text(),
                   [this](QMenu* menu, const TrackSelection&) { menu->addAction(m_attachBackArtwork); });

    m_attachArtistArtwork->setStatusTip(tr("Attach an image file as the artist picture for the selected tracks"));
    QObject::connect(m_attachArtistArtwork, &QAction::triggered, m_self, [this]() {
        const auto* selection = m_self->selectedSelection();
        if(!selection) {
            return;
        }
        attachArtwork(*selection, Track::Cover::Artist, Utils::getMainWindow());
    });
    registerAction(m_self, TrackContextMenuArea::Track, Constants::Menus::Context::ArtworkAttach,
                   Id{u"%1.Artist"_s.arg(QString::fromLatin1(Constants::Menus::Context::ArtworkAttach))},
                   m_attachArtistArtwork->text(),
                   [this](QMenu* menu, const TrackSelection&) { menu->addAction(m_attachArtistArtwork); });

    registerSeparator(TrackContextMenuArea::Track, Constants::Menus::Context::Artwork,
                      ContextMenuIds::TrackSelection::ArtworkAttachSeparator);

    m_removeArtwork->setStatusTip(tr("Remove all artwork associated with the selected tracks (embedded, directory)"));
    auto* removeArtworkCmd = m_actionManager->registerAction(m_removeArtwork, Constants::Actions::RemoveArtwork);
    removeArtworkCmd->setCategories(tracksCategory);
    QObject::connect(m_removeArtwork, &QAction::triggered, m_self, [this]() {
        if(const auto* selection = m_self->selectedSelection()) {
            removeArtwork(*selection);
        }
    });
    registerAction(m_self, TrackContextMenuArea::Track, Constants::Menus::Context::Artwork,
                   Constants::Actions::RemoveArtwork, m_removeArtwork->text(),
                   [this](QMenu* menu, const TrackSelection&) { menu->addAction(m_removeArtwork); });

    registerSeparator(TrackContextMenuArea::Track, m_trackRoot.id, Constants::Menus::Context::TrackFinalSeparator);

    m_openProperties->setStatusTip(tr("Open the properties dialog"));
    auto* openPropsCmd = m_actionManager->registerAction(m_openProperties, Constants::Actions::OpenProperties);
    openPropsCmd->setCategories(tracksCategory);
    QObject::connect(m_openProperties, &QAction::triggered, m_self, [this]() {
        if(const auto* selection = m_self->selectedSelection()) {
            openProperties(*selection);
        }
    });
    registerAction(m_self, TrackContextMenuArea::Track, m_trackRoot.id, Constants::Actions::OpenProperties,
                   m_openProperties->text(),
                   [this](QMenu* menu, const TrackSelection&) { menu->addAction(m_openProperties); });
}

MenuNode* TrackSelectionControllerPrivate::rootForArea(TrackContextMenuArea area)
{
    switch(area) {
        case TrackContextMenuArea::Track:
            return &m_trackRoot;
        case TrackContextMenuArea::Queue:
            return &m_queueRoot;
        case TrackContextMenuArea::Playlist:
            return &m_playlistRoot;
    }

    return &m_trackRoot;
}

bool TrackSelectionControllerPrivate::registerSubmenu(QObject* owner, TrackContextMenuArea area, const Id& parentId,
                                                      const Id& id, const QString& title, const Id& beforeId)
{
    if(!owner || !id.isValid() || !parentId.isValid() || m_menuNodes.contains(id)) {
        return false;
    }

    if(!m_menuNodes.contains(parentId)) {
        return false;
    }

    MenuNode* parent = m_menuNodes.at(parentId);

    auto node    = std::make_unique<MenuNode>();
    node->type   = MenuNodeType::Submenu;
    node->owner  = owner;
    node->id     = id;
    node->title  = title;
    node->area   = area;
    node->parent = parent;

    auto* nodePtr = node.get();

    if(beforeId.isValid()) {
        const auto beforeIt = std::ranges::find_if(
            parent->children, [&beforeId](const auto& child) { return child && child->id == beforeId; });
        if(beforeIt == parent->children.cend()) {
            return false;
        }
        parent->children.insert(beforeIt, std::move(node));
    }
    else {
        parent->children.emplace_back(std::move(node));
    }

    m_menuNodes.emplace(id, nodePtr);
    return true;
}

bool TrackSelectionControllerPrivate::registerAction(QObject* owner, TrackContextMenuArea area, const Id& parentId,
                                                     const Id& id, const QString& title,
                                                     const TrackSelectionController::TrackContextMenuRenderer& renderer,
                                                     const Id& beforeId)
{
    if(!owner || !id.isValid() || !parentId.isValid() || !renderer || m_menuNodes.contains(id)) {
        return false;
    }

    if(!m_menuNodes.contains(parentId)) {
        return false;
    }

    MenuNode* parent = m_menuNodes.at(parentId);

    auto node      = std::make_unique<MenuNode>();
    node->type     = MenuNodeType::Action;
    node->owner    = owner;
    node->id       = id;
    node->title    = title;
    node->area     = area;
    node->renderer = renderer;
    node->parent   = parent;

    auto* nodePtr = node.get();

    if(beforeId.isValid()) {
        const auto beforeIt = std::ranges::find_if(
            parent->children, [&beforeId](const auto& child) { return child && child->id == beforeId; });
        if(beforeIt == parent->children.cend()) {
            return false;
        }
        parent->children.insert(beforeIt, std::move(node));
    }
    else {
        parent->children.emplace_back(std::move(node));
    }

    m_menuNodes.emplace(id, nodePtr);
    return true;
}

bool TrackSelectionControllerPrivate::registerSeparator(TrackContextMenuArea area, const Id& parentId, const Id& id,
                                                        const Id& beforeId)
{
    if(!parentId.isValid() || !m_menuNodes.contains(parentId) || (id.isValid() && m_menuNodes.contains(id))) {
        return false;
    }
    MenuNode* parent = m_menuNodes.at(parentId);

    auto node    = std::make_unique<MenuNode>();
    node->type   = MenuNodeType::Separator;
    node->owner  = m_self;
    node->id     = id;
    node->area   = area;
    node->parent = parent;

    auto* nodePtr = node.get();

    if(beforeId.isValid()) {
        const auto beforeIt = std::ranges::find_if(
            parent->children, [&beforeId](const auto& child) { return child && child->id == beforeId; });
        if(beforeIt == parent->children.cend()) {
            return false;
        }
        parent->children.insert(beforeIt, std::move(node));
    }
    else {
        parent->children.emplace_back(std::move(node));
    }

    if(id.isValid()) {
        m_menuNodes.emplace(id, nodePtr);
    }
    return true;
}

void TrackSelectionControllerPrivate::renderArea(QMenu* menu, TrackContextMenuArea area,
                                                 const TrackSelection& selection)
{
    if(!menu) {
        return;
    }

    m_menuSelection     = selection;
    m_menuSelectionMenu = menu;

    QObject::connect(menu, &QObject::destroyed, m_self, [this, menu]() {
        if(m_menuSelectionMenu == menu) {
            m_menuSelection.reset();
            m_menuSelectionMenu.clear();
        }
    });

    if(const auto* root = rootForArea(area)) {
        const auto entries = orderedRootEntries(*root);

        ContextMenuUtils::renderGroupedMenu(
            menu, entries, [](const auto& entry) { return entry.isBoundary; },
            [this, &selection](const auto& entry, QMenu* targetMenu) {
                if(!entry.node) {
                    return false;
                }

                const int actionCount = static_cast<int>(targetMenu->actions().size());
                renderNode(targetMenu, *entry.node, selection);
                return static_cast<int>(targetMenu->actions().size()) > actionCount;
            });
    }
}

void TrackSelectionControllerPrivate::renderNode(QMenu* menu, const MenuNode& node,
                                                 const TrackSelection& selection) const
{
    if(!menu || !node.owner || (node.id.isValid() && isNodeDisabled(node.id))) {
        return;
    }

    switch(node.type) {
        case MenuNodeType::Separator: {
            menu->addSeparator();
            return;
        }
        case MenuNodeType::Action: {
            node.renderer(menu, selection);
            return;
        }
        case MenuNodeType::Submenu: {
            auto submenu = std::make_unique<QMenu>(node.title, menu);
            if(node.renderer) {
                node.renderer(submenu.get(), selection);
            }
            renderChildNodes(submenu.get(), node.children, selection);
            if(hasVisibleActions(submenu.get())) {
                menu->addMenu(submenu.release());
            }
            return;
        }
        case MenuNodeType::Root:
            return;
    }
}

void TrackSelectionControllerPrivate::renderChildNodes(QMenu* menu, const std::vector<std::unique_ptr<MenuNode>>& nodes,
                                                       const TrackSelection& selection) const
{
    if(!menu) {
        return;
    }

    ContextMenuUtils::renderGroupedMenu(
        menu, nodes, [](const auto& node) { return node && node->type == MenuNodeType::Separator; },
        [this, &selection](const auto& node, QMenu* targetMenu) {
            if(!node) {
                return false;
            }

            const auto actionCount = targetMenu->actions().size();
            renderNode(targetMenu, *node, selection);
            return targetMenu->actions().size() > actionCount;
        });
}

void TrackSelectionControllerPrivate::appendNodeInfo(const MenuNode& node,
                                                     std::vector<TrackContextMenuNodeInfo>& nodes) const
{
    if(node.type != MenuNodeType::Root && node.id.isValid() && node.owner) {
        nodes.push_back(
            {.id       = node.id,
             .parentId = node.parent && node.parent->id.isValid() ? std::optional<Id>{node.parent->id} : std::nullopt,
             .title    = node.title,
             .area     = node.area,
             .isSeparator = node.type == MenuNodeType::Separator,
             .isSubmenu   = node.type == MenuNodeType::Submenu});
    }

    for(const auto& child : node.children) {
        if(child) {
            appendNodeInfo(*child, nodes);
        }
    }
}

QStringList TrackSelectionControllerPrivate::topLevelLayout(TrackContextMenuArea area) const
{
    switch(area) {
        case TrackContextMenuArea::Track:
            return m_settings->value<Settings::Gui::Internal::ContextMenuTrackLayout>();
        case TrackContextMenuArea::Queue:
        case TrackContextMenuArea::Playlist:
            return {};
    }

    return {};
}

std::vector<TopLevelRenderEntry> TrackSelectionControllerPrivate::orderedRootEntries(const MenuNode& root) const
{
    struct RootEntry
    {
        QString id;
        TopLevelRenderEntry entry;
        bool isSeparator{false};
    };

    std::vector<RootEntry> rootEntries;

    for(const auto& child : root.children) {
        if(!child || !child->id.isValid()) {
            continue;
        }

        rootEntries.emplace_back(child->id.name(), TopLevelRenderEntry{.node = child.get()},
                                 child->type == MenuNodeType::Separator);
    }

    QStringList effectiveLayout;
    const QStringList defaultLayout = [&rootEntries] {
        QStringList ids;
        ids.reserve(static_cast<qsizetype>(rootEntries.size()));

        for(const auto& entry : rootEntries) {
            ids.emplace_back(entry.id);
        }

        return ids;
    }();
    const QStringList savedLayout = topLevelLayout(root.area);
    effectiveLayout = savedLayout.isEmpty() ? defaultLayout
                                            : ContextMenuUtils::effectiveContextMenuLayout(defaultLayout, savedLayout);

    std::vector<TopLevelRenderEntry> orderedEntries;
    orderedEntries.reserve(rootEntries.size()
                           + static_cast<size_t>(std::ranges::count_if(
                               effectiveLayout, [](const auto& id) { return id.startsWith(SeparatorIdPrefix); })));

    for(const auto& id : std::as_const(effectiveLayout)) {
        if(id.startsWith(SeparatorIdPrefix)) {
            orderedEntries.push_back({.isBoundary = true});
            continue;
        }

        const auto entryIt = std::ranges::find_if(rootEntries, [&id](const auto& entry) { return entry.id == id; });
        if(entryIt == rootEntries.cend()) {
            continue;
        }

        orderedEntries.push_back(entryIt->entry);
    }

    return orderedEntries;
}

void TrackSelectionControllerPrivate::refreshDisabledNodes()
{
    m_disabledNodeIds.clear();

    const auto disabledIds = m_settings->value<Settings::Gui::Internal::ContextMenuTrackDisabledSections>();
    for(const QString& idName : disabledIds) {
        const Id id{idName};
        if(id.isValid()) {
            m_disabledNodeIds.insert(id);
        }
    }
}

bool TrackSelectionControllerPrivate::isNodeDisabled(const Id& id) const
{
    return m_disabledNodeIds.contains(id);
}

bool TrackSelectionControllerPrivate::hasVisibleActions(const QMenu* menu)
{
    return menu && std::ranges::any_of(menu->actions(), [](const QAction* action) {
               return action && action->isVisible() && !action->isSeparator();
           });
}

bool TrackSelectionControllerPrivate::hasTracks() const
{
    if(!m_tracks.tracks.empty()) {
        return true;
    }

    return hasContextTracks();
}

bool TrackSelectionControllerPrivate::hasContextTracks() const
{
    if(!m_activeContext) {
        return false;
    }

    return m_contextSelection.contains(m_activeContext) && !m_contextSelection.at(m_activeContext).tracks.empty();
}

WidgetContext* TrackSelectionControllerPrivate::contextObject(QWidget* widget) const
{
    if(m_contextWidgets.contains(widget)) {
        return m_contextWidgets.at(widget);
    }
    return nullptr;
}

bool TrackSelectionControllerPrivate::addContextObject(WidgetContext* context)
{
    if(!context) {
        return false;
    }

    QWidget* widget = context->widget();
    if(m_contextWidgets.contains(widget)) {
        return true;
    }

    m_contextWidgets.emplace(widget, context);
    QObject::connect(context, &QObject::destroyed, m_self, [this, context] { removeContextObject(context); });

    return true;
}

void TrackSelectionControllerPrivate::removeContextObject(WidgetContext* context)
{
    if(!context) {
        return;
    }

    QObject::disconnect(context, &QObject::destroyed, m_self, nullptr);

    if(!std::erase_if(m_contextWidgets, [context](const auto& v) { return v.second == context; })) {
        return;
    }

    m_contextSelection.erase(context);

    if(m_activeContext == context) {
        m_activeContext = nullptr;
    }
}

void TrackSelectionControllerPrivate::updateActiveContext(QWidget* widget)
{
    if(qobject_cast<QMenuBar*>(widget) || qobject_cast<QMenu*>(widget)) {
        return;
    }

    if(QWidget* focusedWidget = QApplication::focusWidget()) {
        WidgetContext* widgetContext{nullptr};
        while(focusedWidget) {
            widgetContext = contextObject(focusedWidget);
            if(widgetContext) {
                if(std::exchange(m_activeContext, widgetContext) != widgetContext) {
                    updateActionState();
                    QMetaObject::invokeMethod(m_self, &TrackSelectionController::selectionChanged);
                }
                return;
            }
            focusedWidget = focusedWidget->parentWidget();
        }
    }

    if(std::exchange(m_activeContext, nullptr)) {
        updateActionState();
        QMetaObject::invokeMethod(m_self, &TrackSelectionController::selectionChanged);
    }
}

void TrackSelectionControllerPrivate::handleActions(PlaylistAction::ActionOptions options, Playlist* playlist) const
{
    if(playlist && options & PlaylistAction::Switch) {
        m_playlistController->changeCurrentPlaylist(playlist);
    }

    if(options & PlaylistAction::StartPlayback) {
        if(playlist) {
            m_playlistController->playerController()->startPlayback(playlist);
        }
        else {
            m_playlistController->playerController()->next();
        }
    }
}

void TrackSelectionControllerPrivate::sendToNewPlaylist(PlaylistAction::ActionOptions options,
                                                        const QString& playlistName) const
{
    if(!hasTracks()) {
        return;
    }

    const auto& selection = m_contextSelection.at(m_activeContext);
    const QString newName = !playlistName.isEmpty() ? playlistName : Track::findCommonField(selection.tracks);

    if(options & PlaylistAction::KeepActive) {
        const auto* activePlaylist = m_playlistHandler->activePlaylist();

        if(!activePlaylist || activePlaylist->name() != newName) {
            auto* playlist = m_playlistHandler->createPlaylist(newName, selection.tracks);
            handleActions(options, playlist);
            return;
        }
        const QString keepActiveName = newName + u" ("_s + tr("Playback") + u")"_s;

        if(auto* keepActivePlaylist = m_playlistHandler->playlistByName(keepActiveName)) {
            m_playlistHandler->movePlaylistTracks(activePlaylist->id(), keepActivePlaylist->id());
        }
        else {
            m_playlistHandler->renamePlaylist(activePlaylist->id(), keepActiveName);
        }
    }

    if(auto* playlist = m_playlistHandler->createPlaylist(newName, selection.tracks)) {
        playlist->changeCurrentIndex(-1);
        handleActions(options, playlist);
        Q_EMIT m_self->actionExecuted(TrackAction::SendNewPlaylist);
    }
}

void TrackSelectionControllerPrivate::sendToCurrentPlaylist(PlaylistAction::ActionOptions options) const
{
    if(!hasTracks()) {
        return;
    }

    const auto& selection = m_contextSelection.at(m_activeContext);
    auto* playlist        = m_playlistController->currentPlaylist();

    if(!playlist || playlist->isAutoPlaylist()) {
        return;
    }

    m_playlistHandler->createPlaylist(playlist->name(), selection.tracks);
    handleActions(options, playlist);
    Q_EMIT m_self->actionExecuted(TrackAction::SendCurrentPlaylist);
}

void TrackSelectionControllerPrivate::addToCurrentPlaylist(bool startPlaybackIfStopped) const
{
    if(!hasTracks()) {
        return;
    }

    const auto& selection = m_contextSelection.at(m_activeContext);
    auto* playlist        = m_playlistController->currentPlaylist();
    if(!playlist || playlist->isAutoPlaylist()) {
        return;
    }

    const int firstAddedIndex = playlist->trackCount();
    m_playlistHandler->appendToPlaylist(playlist->id(), selection.tracks);

    if(startPlaybackIfStopped && m_playlistController->playerController()->playState() == Player::PlayState::Stopped) {
        playlist->changeCurrentIndex(firstAddedIndex);
        m_playlistController->playerController()->startPlayback(playlist);
    }

    Q_EMIT m_self->actionExecuted(startPlaybackIfStopped ? TrackAction::AddCurrentPlaylistAndPlayIfStopped
                                                         : TrackAction::AddCurrentPlaylist);
}

void TrackSelectionControllerPrivate::addToActivePlaylist() const
{
    if(m_self->hasTracks()) {
        const auto& selection = m_contextSelection.at(m_activeContext);
        if(const auto* playlist = m_playlistHandler->activePlaylist()) {
            m_playlistHandler->appendToPlaylist(playlist->id(), selection.tracks);
            Q_EMIT m_self->actionExecuted(TrackAction::AddActivePlaylist);
        }
    }
}

void TrackSelectionControllerPrivate::addToPlaylist(const UId& playlistId, const TrackList& tracks) const
{
    if(tracks.empty()) {
        return;
    }

    if(const auto* playlist = m_playlistHandler->playlistById(playlistId); playlist && !playlist->isAutoPlaylist()) {
        m_playlistHandler->appendToPlaylist(playlistId, tracks);
    }
}

void TrackSelectionControllerPrivate::addPlaylistTargets(QMenu* menu, const TrackSelection& selection,
                                                         const std::optional<UId>& excludedPlaylistId) const
{
    if(!menu || selection.tracks.empty()) {
        return;
    }

    const auto playlists = m_playlistHandler->playlists();
    if(playlists.empty()) {
        return;
    }

    for(const auto* playlist : playlists) {
        if(!playlist || playlist->isAutoPlaylist() || playlist->isTemporary()) {
            continue;
        }
        if(excludedPlaylistId && playlist->id() == *excludedPlaylistId) {
            continue;
        }

        const auto* action = menu->addAction(playlist->name());
        QObject::connect(
            action, &QAction::triggered, m_self,
            [this, playlistId = playlist->id(), tracks = selection.tracks]() { addToPlaylist(playlistId, tracks); });
    }
}

void TrackSelectionControllerPrivate::startPlayback(PlaylistAction::ActionOptions options)
{
    if(!hasTracks()) {
        return;
    }

    if(options & PlaylistAction::TempPlaylist) {
        if(!m_tempPlaylist) {
            m_tempPlaylist = m_playlistHandler->createTempPlaylist(QString::fromLatin1(TempSelectionPlaylist));
            if(!m_tempPlaylist) {
                return;
            }
        }

        const auto& selection = m_contextSelection.at(m_activeContext);
        m_playlistHandler->replacePlaylistTracks(m_tempPlaylist->id(), selection.tracks);
        m_tempPlaylist->changeCurrentIndex(selection.primaryPlaylistIndex.value_or(0));
        m_playlistController->playerController()->startPlayback(m_tempPlaylist);
    }
    else {
        const auto& selection = m_contextSelection.at(m_activeContext);

        Playlist* playlist = m_playlistController->currentPlaylist();
        if(selection.playlistId) {
            playlist = m_playlistHandler->playlistById(*selection.playlistId);
        }

        if(playlist) {
            if(selection.primaryPlaylistIndex) {
                playlist->changeCurrentIndex(*selection.primaryPlaylistIndex);
            }
            m_playlistController->playerController()->startPlayback(playlist);
        }
    }
}

void TrackSelectionControllerPrivate::addToQueue() const
{
    if(!hasTracks()) {
        return;
    }

    const auto& selection = m_contextSelection.at(m_activeContext);
    m_playlistController->playerController()->queueTracks(queueTracksForSelection(selection));
    Q_EMIT m_self->actionExecuted(TrackAction::AddToQueue);
}

void TrackSelectionControllerPrivate::queueNext() const
{
    if(!hasTracks()) {
        return;
    }

    const auto& selection = m_contextSelection.at(m_activeContext);
    m_playlistController->playerController()->queueTracksNext(queueTracksForSelection(selection));
    Q_EMIT m_self->actionExecuted(TrackAction::QueueNext);
}

void TrackSelectionControllerPrivate::openFolder(const TrackSelection& selection) const
{
    if(selection.tracks.empty()) {
        return;
    }

    const Track& track = selection.tracks.front();
    if(track.isInArchive()) {
        Utils::File::openDirectory(QFileInfo{track.archivePath()}.absolutePath());
    }
    else {
        Utils::File::openDirectory(track.path());
    }
}

void TrackSelectionControllerPrivate::copyLocation(const TrackSelection& selection) const
{
    if(selection.tracks.empty()) {
        return;
    }

    QStringList paths;
    for(const auto& track : selection.tracks) {
        paths.append(track.prettyFilepath());
    }
    QApplication::clipboard()->setText(paths.join("\n"_L1));
}

void TrackSelectionControllerPrivate::copyDirectoryPath(const TrackSelection& selection) const
{
    if(selection.tracks.empty()) {
        return;
    }

    QStringList paths;
    for(const auto& track : selection.tracks) {
        const QString path = track.isInArchive() ? QFileInfo{track.archivePath()}.absolutePath() : track.path();
        if(!path.isEmpty() && !paths.contains(path)) {
            paths.emplace_back(path);
        }
    }

    QApplication::clipboard()->setText(paths.join("\n"_L1));
}

void TrackSelectionControllerPrivate::searchArtwork(const TrackSelection& selection, bool quick) const
{
    if(selection.tracks.empty()) {
        return;
    }

    Q_EMIT m_self->requestArtworkSearch(selection.tracks, quick);
}

void TrackSelectionControllerPrivate::extractArtwork(const TrackSelection& selection) const
{
    if(selection.tracks.empty()) {
        return;
    }

    const auto summary = ArtworkExporter::extractTracks(
        m_audioLoader, selection.tracks, {Track::Cover::Front, Track::Cover::Back, Track::Cover::Artist});
    StatusEvent::post(ArtworkExporter::statusMessage(summary));
}

void TrackSelectionControllerPrivate::attachArtwork(const TrackSelection& selection, Track::Cover coverType,
                                                    QWidget* parent) const
{
    if(selection.tracks.empty()) {
        return;
    }

    const QString filepath = promptForArtworkPath(parent);
    if(!filepath.isEmpty()) {
        Q_EMIT m_self->requestArtworkAttach(selection.tracks, coverType, filepath);
    }
}

void TrackSelectionControllerPrivate::removeArtwork(const TrackSelection& selection) const
{
    if(selection.tracks.empty()) {
        return;
    }

    Q_EMIT m_self->requestArtworkRemoval(selection.tracks);
}

void TrackSelectionControllerPrivate::openProperties(const TrackSelection& selection) const
{
    if(selection.tracks.empty()) {
        return;
    }

    Q_EMIT m_self->requestPropertiesDialog(selection.tracks);
}

void TrackSelectionControllerPrivate::sendToQueue(PlaylistAction::ActionOptions options) const
{
    if(!hasTracks()) {
        return;
    }

    const auto& selection = m_contextSelection.at(m_activeContext);
    m_playlistController->playerController()->replaceTracks(queueTracksForSelection(selection));
    handleActions(options);
    Q_EMIT m_self->actionExecuted(TrackAction::SendToQueue);
}

QueueTracks TrackSelectionControllerPrivate::queueTracksForSelection(const TrackSelection& selection) const
{
    if(selection.tracks.empty()) {
        return {};
    }

    QueueTracks queueTracks;
    queueTracks.reserve(selection.tracks.size());

    const bool playlistBacked      = selection.playlistBacked && selection.playlistId
                                  && selection.playlistIndexes.size() == selection.tracks.size();
    const bool hasPlaylistEntryIds = selection.playlistEntryIds.size() == selection.tracks.size();
    if(playlistBacked) {
        for(size_t i{0}; i < selection.tracks.size(); ++i) {
            queueTracks.push_back(
                PlaylistTrack{.track           = selection.tracks.at(i),
                              .playlistId      = *selection.playlistId,
                              .entryId         = hasPlaylistEntryIds ? selection.playlistEntryIds.at(i) : UId{},
                              .indexInPlaylist = selection.playlistIndexes.at(i)});
        }
        return queueTracks;
    }

    for(const Track& track : selection.tracks) {
        queueTracks.emplace_back(track);
    }

    return queueTracks;
}

bool TrackSelectionControllerPrivate::canDequeue(const TrackSelection& selection) const
{
    if(selection.tracks.empty()) {
        return false;
    }

    const auto queuedTracks = m_playlistController->playerController()->playbackQueue().tracks();
    if(queuedTracks.empty()) {
        return false;
    }

    std::set<int> selectedTrackIds;
    for(const Track& track : selection.tracks) {
        selectedTrackIds.insert(track.id());
    }

    return std::ranges::any_of(queuedTracks, [&selectedTrackIds](const PlaylistTrack& track) {
        return selectedTrackIds.contains(track.track.id());
    });
}

bool TrackSelectionControllerPrivate::allTracksInSameFolder(const TrackSelection& selection) const
{
    if(selection.tracks.empty()) {
        return false;
    }

    const Track& firstTrack = selection.tracks.front();
    const QString firstPath = firstTrack.isInArchive() ? firstTrack.archivePath() : firstTrack.path();
    return std::ranges::all_of(selection.tracks | std::ranges::views::transform([](const Track& track) {
                                   return track.isInArchive() ? track.archivePath() : track.path();
                               }),
                               [&firstPath](const QString& folderPath) { return folderPath == firstPath; });
}

bool TrackSelectionControllerPrivate::canWrite(const TrackSelection& selection) const
{
    if(selection.tracks.empty()) {
        return false;
    }

    std::unordered_map<QString, bool> writableByExtension;
    writableByExtension.reserve(8);

    return std::ranges::all_of(selection.tracks, [this, &writableByExtension](const Track& track) {
        if(track.hasCue() || track.isInArchive()) {
            return false;
        }

        const QString extension = track.extension().toLower();
        if(const auto it = writableByExtension.find(extension); it != writableByExtension.cend()) {
            return it->second;
        }

        const bool canWrite = m_audioLoader->canWriteMetadata(track);
        writableByExtension.emplace(extension, canWrite);
        return canWrite;
    });
}

const TrackSelection* TrackSelectionControllerPrivate::currentSelection() const
{
    if(m_menuSelection) {
        return &*m_menuSelection;
    }

    if(!m_tracks.tracks.empty()) {
        return &m_tracks;
    }

    if(hasContextTracks()) {
        return &m_contextSelection.at(m_activeContext);
    }

    return nullptr;
}

void TrackSelectionControllerPrivate::updateActionState()
{
    const auto* selection         = currentSelection();
    const bool haveTracks         = selection && !selection->tracks.empty();
    const auto* currentPlaylist   = m_playlistController->currentPlaylist();
    const bool canEditCurrent     = currentPlaylist && !currentPlaylist->isAutoPlaylist();
    const bool sameFolder         = haveTracks && allTracksInSameFolder(*selection);
    const bool writable           = haveTracks && canWrite(*selection);
    const bool writableCover      = haveTracks
                                 && (writable
                                     || m_settings->value<Settings::Gui::Internal::ArtworkSaveMethods>()
                                                .value<ArtworkSaveMethods>()
                                                .value(Track::Cover::Front)
                                                .method
                                            == ArtworkSaveMethod::Directory);
    const bool canRemoveFromQueue = haveTracks && canDequeue(*selection);

    m_addCurrent->setEnabled(haveTracks && canEditCurrent);
    m_addActive->setEnabled(haveTracks && m_playlistHandler->activePlaylist());
    m_sendCurrent->setEnabled(haveTracks && canEditCurrent);
    m_sendNew->setEnabled(haveTracks);
    m_openFolder->setEnabled(sameFolder);
    m_copyLocation->setEnabled(haveTracks);
    m_copyDirectoryPath->setEnabled(haveTracks);
    m_searchArtwork->setEnabled(writableCover);
    m_extractArtwork->setEnabled(haveTracks);
    m_attachFrontArtwork->setEnabled(writable);
    m_attachBackArtwork->setEnabled(writable);
    m_attachArtistArtwork->setEnabled(writable);
    m_openProperties->setEnabled(haveTracks);
    m_addToQueue->setEnabled(haveTracks);
    m_queueNext->setEnabled(haveTracks);
    m_removeFromQueue->setEnabled(canRemoveFromQueue);
}

TrackSelectionController::TrackSelectionController(ActionManager* actionManager, AudioLoader* audioLoader,
                                                   SettingsManager* settings, PlaylistController* playlistController,
                                                   QObject* parent)
    : QObject{parent}
    , p{std::make_unique<TrackSelectionControllerPrivate>(this, actionManager, audioLoader, settings,
                                                          playlistController)}
{
    QObject::connect(qApp, &QApplication::focusChanged, this,
                     [this](QWidget* /*old*/, QWidget* now) { p->updateActiveContext(now); });
}

TrackSelectionController::~TrackSelectionController() = default;

bool TrackSelectionController::hasTracks() const
{
    return p->hasTracks();
}

const TrackSelection* TrackSelectionController::selectedSelection() const
{
    return p->currentSelection();
}

Track TrackSelectionController::selectedTrack() const
{
    if(const auto* selection = selectedSelection(); selection && !selection->tracks.empty()) {
        return selection->tracks.front();
    }

    return {};
}

TrackList TrackSelectionController::selectedTracks() const
{
    if(const auto* selection = selectedSelection()) {
        return selection->tracks;
    }

    return {};
}

int TrackSelectionController::selectedTrackCount() const
{
    if(const auto* selection = selectedSelection()) {
        return static_cast<int>(selection->tracks.size());
    }

    return 0;
}

void TrackSelectionController::changeSelectedTracks(WidgetContext* context, const TrackSelection& selection)
{
    if(!p->addContextObject(context)) {
        return;
    }

    auto updatedSelection           = selection;
    auto& existing                  = p->m_contextSelection[context];
    updatedSelection.playbackOnSend = existing.playbackOnSend;

    if(!updatedSelection.tracks.empty()) {
        p->m_activeContext = context;
    }

    if(std::exchange(existing, updatedSelection) == updatedSelection) {
        return;
    }

    p->updateActionState();
    Q_EMIT selectionChanged();
}

void TrackSelectionController::changeSelectedTracks(const TrackSelection& selection)
{
    if(std::exchange(p->m_tracks, selection) == selection) {
        return;
    }

    p->updateActionState();
    Q_EMIT selectionChanged();
}

void TrackSelectionController::changePlaybackOnSend(WidgetContext* context, bool enabled)
{
    if(p->addContextObject(context)) {
        auto& selection = p->m_contextSelection[context];
        if(selection.playbackOnSend == enabled) {
            return;
        }

        selection.playbackOnSend = enabled;
        p->updateActionState();
    }
}

void TrackSelectionController::addTrackContextMenu(QMenu* menu) const
{
    p->renderArea(menu, TrackContextMenuArea::Track, selectedSelection() ? *selectedSelection() : TrackSelection{});
}

void TrackSelectionController::addTrackContextMenu(QMenu* menu, const TrackSelection& selection) const
{
    p->renderArea(menu, TrackContextMenuArea::Track, selection);
}

void TrackSelectionController::addTrackQueueContextMenu(QMenu* menu) const
{
    p->renderArea(menu, TrackContextMenuArea::Queue, selectedSelection() ? *selectedSelection() : TrackSelection{});
}

void TrackSelectionController::addTrackPlaylistContextMenu(QMenu* menu) const
{
    if(!menu) {
        return;
    }

    const auto beforeCount    = static_cast<int>(menu->actions().size());
    const bool hadItemsBefore = beforeCount > 0;

    if(hadItemsBefore) {
        menu->addSeparator();
    }

    const auto beforeRenderCount = static_cast<int>(menu->actions().size());

    p->renderArea(menu, TrackContextMenuArea::Playlist, selectedSelection() ? *selectedSelection() : TrackSelection{});

    const auto actions = menu->actions();

    if(actions.size() > beforeRenderCount) {
        menu->addSeparator();
    }
    else if(hadItemsBefore && actions.size() == beforeCount + 1) {
        if(auto* separator = actions.back(); separator && separator->isSeparator()) {
            menu->removeAction(separator);
            separator->deleteLater();
        }
    }
}

void TrackSelectionController::addTrackAddToPlaylistContextMenu(QMenu* menu) const
{
    p->addPlaylistTargets(menu, selectedSelection() ? *selectedSelection() : TrackSelection{});
}

void TrackSelectionController::addTrackAddToOtherPlaylistContextMenu(QMenu* menu) const
{
    const auto* selection = selectedSelection();
    if(!selection || !selection->playlistId) {
        return;
    }

    p->addPlaylistTargets(menu, *selection, selection->playlistId);
}

bool TrackSelectionController::registerTrackContextSubmenu(QObject* owner, TrackContextMenuArea area,
                                                           const Id& parentId, const Id& id, const QString& title,
                                                           const Id& beforeId)
{
    return p->registerSubmenu(owner, area, parentId, id, title, beforeId);
}

bool TrackSelectionController::registerTrackContextAction(QObject* owner, TrackContextMenuArea area, const Id& parentId,
                                                          const Id& id, const QString& title,
                                                          const TrackContextMenuRenderer& renderer, const Id& beforeId)
{
    return p->registerAction(owner, area, parentId, id, title, renderer, beforeId);
}

bool TrackSelectionController::registerTrackContextDynamicSubmenu(QObject* owner, TrackContextMenuArea area,
                                                                  const Id& parentId, const Id& id,
                                                                  const QString& title,
                                                                  const TrackContextMenuRenderer& renderer,
                                                                  const Id& beforeId)
{
    if(!p->registerSubmenu(owner, area, parentId, id, title, beforeId)) {
        return false;
    }
    if(!p->m_menuNodes.contains(id)) {
        return false;
    }

    p->m_menuNodes.at(id)->renderer = renderer;
    return true;
}

std::vector<TrackContextMenuNodeInfo> TrackSelectionController::trackContextMenuNodes() const
{
    std::vector<TrackContextMenuNodeInfo> nodes;
    p->appendNodeInfo(p->m_playlistRoot, nodes);
    p->appendNodeInfo(p->m_queueRoot, nodes);
    p->appendNodeInfo(p->m_trackRoot, nodes);
    return nodes;
}

void TrackSelectionController::executeAction(TrackAction action, PlaylistAction::ActionOptions options,
                                             const QString& playlistName)
{
    switch(action) {
        case TrackAction::SendCurrentPlaylist:
            p->sendToCurrentPlaylist(options);
            break;
        case TrackAction::SendNewPlaylist:
            p->sendToNewPlaylist(options, playlistName);
            break;
        case TrackAction::AddCurrentPlaylist:
            p->addToCurrentPlaylist();
            break;
        case TrackAction::AddCurrentPlaylistAndPlayIfStopped:
            p->addToCurrentPlaylist(true);
            break;
        case TrackAction::AddActivePlaylist:
            p->addToActivePlaylist();
            break;
        case TrackAction::Play:
            p->startPlayback(options);
            break;
        case TrackAction::AddToQueue:
            p->addToQueue();
            break;
        case TrackAction::QueueNext:
            p->queueNext();
            break;
        case TrackAction::SendToQueue:
            p->sendToQueue(options);
            break;
        case TrackAction::None:
            break;
    }
}

void TrackSelectionController::tracksUpdated(const TrackList& tracks)
{
    Utils::updateCommonTracks(p->m_tracks.tracks, tracks, Utils::CommonOperation::Update);
    for(auto& [_, selection] : p->m_contextSelection) {
        Utils::updateCommonTracks(selection.tracks, tracks, Utils::CommonOperation::Update);
    }

    p->updateActionState();
    Q_EMIT selectionChanged();
}

void TrackSelectionController::tracksRemoved(const TrackList& tracks)
{
    Utils::updateCommonTracks(p->m_tracks.tracks, tracks, Utils::CommonOperation::Remove);
    for(auto& [_, selection] : p->m_contextSelection) {
        Utils::updateCommonTracks(selection.tracks, tracks, Utils::CommonOperation::Remove);
    }

    p->updateActionState();
    Q_EMIT selectionChanged();
}
} // namespace Fooyin

#include "gui/moc_trackselectioncontroller.cpp"
#include "trackselectioncontroller.moc"
