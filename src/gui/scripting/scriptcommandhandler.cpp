/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include "scriptcommandhandler.h"

#include <core/player/playercontroller.h>
#include <core/track.h>
#include <gui/guiconstants.h>
#include <gui/propertiesdialog.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/fileutils.h>
#include <utils/id.h>

#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace {
struct ResolvedCommand
{
    QString id;
    Fooyin::ScriptCommandAliasType type{Fooyin::ScriptCommandAliasType::Action};
};

ResolvedCommand resolveCommandId(const QString& commandId)
{
    const QString trimmed    = commandId.trimmed();
    const QString normalized = trimmed.toLower();

    for(const auto& alias : Fooyin::ScriptCommandHandler::scriptCommandAliases()) {
        if(normalized == alias.alias) {
            if(alias.type == Fooyin::ScriptCommandAliasType::PlayingProperties) {
                return {.id = u"playingproperties"_s, .type = alias.type};
            }
            if(alias.type == Fooyin::ScriptCommandAliasType::PlayingFolder) {
                return {.id = u"playingfolder"_s, .type = alias.type};
            }
            return {.id = QString::fromLatin1(alias.actionId), .type = alias.type};
        }
    }

    return {.id = trimmed};
}
} // namespace

namespace Fooyin {
ScriptCommandHandler::ScriptCommandHandler(ActionManager* actionManager, PlayerController* playerController,
                                           PropertiesDialog* propertiesDialog)
    : m_actionManager{actionManager}
    , m_playerController{playerController}
    , m_propertiesDialog{propertiesDialog}
{ }

const ScriptCommandAliasList& ScriptCommandHandler::scriptCommandAliases()
{
    using Type = ScriptCommandAliasType;

    static const ScriptCommandAliasList Aliases = {
        {.alias       = u"play",
         .actionId    = Constants::Actions::PlayPause,
         .category    = "Playback",
         .description = "Play or pause playback"},
        {.alias       = u"playpause",
         .actionId    = Constants::Actions::PlayPause,
         .category    = "Playback",
         .description = "Play or pause playback"},
        {.alias       = u"stop",
         .actionId    = Constants::Actions::Stop,
         .category    = "Playback",
         .description = "Stop playback"},
        {.alias       = u"next",
         .actionId    = Constants::Actions::Next,
         .category    = "Playback",
         .description = "Skip to the next track"},
        {.alias       = u"prev",
         .actionId    = Constants::Actions::Previous,
         .category    = "Playback",
         .description = "Skip to the previous track"},
        {.alias       = u"previous",
         .actionId    = Constants::Actions::Previous,
         .category    = "Playback",
         .description = "Skip to the previous track"},
        {.alias       = u"random",
         .actionId    = Constants::Actions::Random,
         .category    = "Playback",
         .description = "Play a random track"},
        {.alias       = u"shuffle",
         .actionId    = Constants::Actions::ShuffleTracks,
         .category    = "Playback",
         .description = "Shuffle tracks"},
        {.alias       = u"shufflealbums",
         .actionId    = Constants::Actions::ShuffleAlbums,
         .category    = "Playback",
         .description = "Shuffle albums"},
        {.alias       = u"repeattrack",
         .actionId    = Constants::Actions::RepeatTrack,
         .category    = "Playback order",
         .description = "Repeat the current track"},
        {.alias       = u"repeatalbum",
         .actionId    = Constants::Actions::RepeatAlbum,
         .category    = "Playback order",
         .description = "Repeat the current album"},
        {.alias       = u"repeatplaylist",
         .actionId    = Constants::Actions::RepeatPlaylist,
         .category    = "Playback order",
         .description = "Repeat the active playlist"},
        {.alias       = u"defaultorder",
         .actionId    = Constants::Actions::PlaybackDefault,
         .category    = "Playback order",
         .description = "Restore the default playback order"},
        {.alias       = u"stopaftercurrent",
         .actionId    = Constants::Actions::StopAfterCurrent,
         .category    = "Playback order",
         .description = "Stop after the current track finishes"},
        {.alias       = u"stopaftercurrentreset",
         .actionId    = Constants::Actions::StopAfterCurrentReset,
         .category    = "Playback order",
         .description = "Clear stop-after-current"},
        {.alias = u"mute", .actionId = Constants::Actions::Mute, .category = "Playback", .description = "Toggle mute"},
        {.alias       = u"volup",
         .actionId    = Constants::Actions::VolumeUp,
         .category    = "Playback",
         .description = "Increase volume"},
        {.alias       = u"volumeup",
         .actionId    = Constants::Actions::VolumeUp,
         .category    = "Playback",
         .description = "Increase volume"},
        {.alias       = u"voldown",
         .actionId    = Constants::Actions::VolumeDown,
         .category    = "Playback",
         .description = "Decrease volume"},
        {.alias       = u"volumedown",
         .actionId    = Constants::Actions::VolumeDown,
         .category    = "Playback",
         .description = "Decrease volume"},
        {.alias       = u"seekforward",
         .actionId    = Constants::Actions::SeekForwardSmall,
         .category    = "Playback",
         .description = "Seek forward by a small step"},
        {.alias       = u"seekback",
         .actionId    = Constants::Actions::SeekBackwardSmall,
         .category    = "Playback",
         .description = "Seek backward by a small step"},
        {.alias       = u"seekbackward",
         .actionId    = Constants::Actions::SeekBackwardSmall,
         .category    = "Playback",
         .description = "Seek backward by a small step"},
        {.alias       = u"seekforwardlarge",
         .actionId    = Constants::Actions::SeekForwardLarge,
         .category    = "Playback",
         .description = "Seek forward by a large step"},
        {.alias       = u"seekbacklarge",
         .actionId    = Constants::Actions::SeekBackwardLarge,
         .category    = "Playback",
         .description = "Seek backward by a large step"},
        {.alias       = u"seekbackwardlarge",
         .actionId    = Constants::Actions::SeekBackwardLarge,
         .category    = "Playback",
         .description = "Seek backward by a large step"},
        {.alias       = u"settings",
         .actionId    = Constants::Actions::Settings,
         .category    = "Application",
         .description = "Open settings"},
        {.alias       = u"preferences",
         .actionId    = Constants::Actions::Settings,
         .category    = "Application",
         .description = "Open settings"},
        {.alias       = u"search",
         .actionId    = Constants::Actions::SearchPlaylist,
         .category    = "Search",
         .description = "Open playlist search"},
        {.alias       = u"quicksearch",
         .actionId    = Constants::Actions::QuickSearch,
         .category    = "Search",
         .description = "Open quick search"},
        {.alias       = u"searchlibrary",
         .actionId    = Constants::Actions::SearchLibrary,
         .category    = "Search",
         .description = "Open library search"},
        {.alias       = u"refresh",
         .actionId    = Constants::Actions::Refresh,
         .category    = "Library",
         .description = "Refresh the library"},
        {.alias       = u"refreshlibrary",
         .actionId    = Constants::Actions::Refresh,
         .category    = "Library",
         .description = "Refresh the library"},
        {.alias       = u"rescan",
         .actionId    = Constants::Actions::Rescan,
         .category    = "Library",
         .description = "Rescan the library"},
        {.alias       = u"rescanlibrary",
         .actionId    = Constants::Actions::Rescan,
         .category    = "Library",
         .description = "Rescan the library"},
        {.alias       = u"addfiles",
         .actionId    = Constants::Actions::AddFiles,
         .category    = "Playlist",
         .description = "Add files to the playlist"},
        {.alias       = u"addfolders",
         .actionId    = Constants::Actions::AddFolders,
         .category    = "Playlist",
         .description = "Add folders to the playlist"},
        {.alias       = u"newplaylist",
         .actionId    = Constants::Actions::NewPlaylist,
         .category    = "Playlist",
         .description = "Create a new playlist"},
        {.alias       = u"newautoplaylist",
         .actionId    = Constants::Actions::NewAutoPlaylist,
         .category    = "Playlist",
         .description = "Create a new autoplaylist"},
        {.alias       = u"loadplaylist",
         .actionId    = Constants::Actions::LoadPlaylist,
         .category    = "Playlist",
         .description = "Load a playlist"},
        {.alias       = u"saveplaylist",
         .actionId    = Constants::Actions::SavePlaylist,
         .category    = "Playlist",
         .description = "Save the active playlist"},
        {.alias       = u"saveallplaylists",
         .actionId    = Constants::Actions::SaveAllPlaylists,
         .category    = "Playlist",
         .description = "Save all playlists"},
        {.alias       = u"quit",
         .actionId    = Constants::Actions::Exit,
         .category    = "Application",
         .description = "Quit fooyin"},
        {.alias       = u"exit",
         .actionId    = Constants::Actions::Exit,
         .category    = "Application",
         .description = "Quit fooyin"},
        {.alias = u"log", .actionId = Constants::Actions::Log, .category = "View", .description = "Open the log view"},
        {.alias       = u"scripteditor",
         .actionId    = Constants::Actions::ScriptEditor,
         .category    = "View",
         .description = "Open the script editor"},
        {.alias       = u"nowplaying",
         .actionId    = Constants::Actions::ShowNowPlaying,
         .category    = "View",
         .description = "Show the now playing item"},
        {.alias       = u"quicksetup",
         .actionId    = Constants::Actions::QuickSetup,
         .category    = "Application",
         .description = "Open quick setup"},
        {.alias       = u"togglemenubar",
         .actionId    = Constants::Actions::ToggleMenubar,
         .category    = "View",
         .description = "Toggle the menu bar"},
        {.alias       = u"properties",
         .actionId    = Constants::Actions::OpenProperties,
         .category    = "View",
         .description = "Open properties for the current selection"},
        {.alias       = u"playingproperties",
         .type        = Type::PlayingProperties,
         .category    = "View",
         .description = "Open properties for the currently playing track"},
        {.alias       = u"nowplayingproperties",
         .type        = Type::PlayingProperties,
         .category    = "View",
         .description = "Open properties for the currently playing track"},
        {.alias       = u"playingfolder",
         .type        = Type::PlayingFolder,
         .category    = "View",
         .description = "Open the folder of the currently playing track"},
        {.alias       = u"nowplayingfolder",
         .type        = Type::PlayingFolder,
         .category    = "View",
         .description = "Open the folder of the currently playing track"},
    };

    return Aliases;
}

bool ScriptCommandHandler::execute(const QString& commandId) const
{
    if(commandId.isEmpty()) {
        return false;
    }

    const ResolvedCommand resolved = resolveCommandId(commandId);

    if(resolved.type == ScriptCommandAliasType::PlayingProperties) {
        if(!m_propertiesDialog || !m_playerController) {
            return false;
        }

        const Track currentTrack = m_playerController->currentTrack();
        if(!currentTrack.isValid()) {
            return false;
        }

        TrackList tracks;
        tracks.emplace_back(currentTrack);
        m_propertiesDialog->show(tracks);
        return true;
    }

    if(resolved.type == ScriptCommandAliasType::PlayingFolder) {
        if(!m_playerController) {
            return false;
        }

        const Track currentTrack = m_playerController->currentTrack();
        if(!currentTrack.isValid()) {
            return false;
        }

        if(currentTrack.isInArchive()) {
            Utils::File::openDirectory(QFileInfo{currentTrack.archivePath()}.absolutePath());
        }
        else {
            Utils::File::openDirectory(currentTrack.path());
        }
        return true;
    }

    if(!m_actionManager) {
        return false;
    }

    if(auto* command = m_actionManager->command(Id{resolved.id})) {
        if(command->action() && command->action()->isEnabled()) {
            command->action()->trigger();
            return true;
        }
    }

    return false;
}
} // namespace Fooyin
