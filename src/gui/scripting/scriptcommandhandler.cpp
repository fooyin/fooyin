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

#include <array>

using namespace Qt::StringLiterals;

namespace {
struct ResolvedCommand
{
    enum class Type : uint8_t
    {
        Action = 0,
        PlayingProperties,
        PlayingFolder,
    };

    QString id;
    Type type{Type::Action};
};

ResolvedCommand resolveCommandId(const QString& commandId)
{
    const QString trimmed    = commandId.trimmed();
    const QString normalized = trimmed.toLower();

    struct CommandAlias
    {
        QStringView alias;
        const char* actionId{nullptr};
        ResolvedCommand::Type type{ResolvedCommand::Type::Action};
    };

    static constexpr auto aliases = std::to_array<CommandAlias>({
        {.alias = u"play", .actionId = Fooyin::Constants::Actions::PlayPause},
        {.alias = u"playpause", .actionId = Fooyin::Constants::Actions::PlayPause},
        {.alias = u"stop", .actionId = Fooyin::Constants::Actions::Stop},
        {.alias = u"next", .actionId = Fooyin::Constants::Actions::Next},
        {.alias = u"prev", .actionId = Fooyin::Constants::Actions::Previous},
        {.alias = u"previous", .actionId = Fooyin::Constants::Actions::Previous},
        {.alias = u"random", .actionId = Fooyin::Constants::Actions::Random},
        {.alias = u"shuffle", .actionId = Fooyin::Constants::Actions::ShuffleTracks},
        {.alias = u"shufflealbums", .actionId = Fooyin::Constants::Actions::ShuffleAlbums},
        {.alias = u"repeattrack", .actionId = Fooyin::Constants::Actions::RepeatTrack},
        {.alias = u"repeatalbum", .actionId = Fooyin::Constants::Actions::RepeatAlbum},
        {.alias = u"repeatplaylist", .actionId = Fooyin::Constants::Actions::RepeatPlaylist},
        {.alias = u"defaultorder", .actionId = Fooyin::Constants::Actions::PlaybackDefault},
        {.alias = u"stopaftercurrent", .actionId = Fooyin::Constants::Actions::StopAfterCurrent},
        {.alias = u"stopaftercurrentreset", .actionId = Fooyin::Constants::Actions::StopAfterCurrentReset},
        {.alias = u"mute", .actionId = Fooyin::Constants::Actions::Mute},
        {.alias = u"volup", .actionId = Fooyin::Constants::Actions::VolumeUp},
        {.alias = u"volumeup", .actionId = Fooyin::Constants::Actions::VolumeUp},
        {.alias = u"voldown", .actionId = Fooyin::Constants::Actions::VolumeDown},
        {.alias = u"volumedown", .actionId = Fooyin::Constants::Actions::VolumeDown},
        {.alias = u"seekforward", .actionId = Fooyin::Constants::Actions::SeekForwardSmall},
        {.alias = u"seekback", .actionId = Fooyin::Constants::Actions::SeekBackwardSmall},
        {.alias = u"seekbackward", .actionId = Fooyin::Constants::Actions::SeekBackwardSmall},
        {.alias = u"seekforwardlarge", .actionId = Fooyin::Constants::Actions::SeekForwardLarge},
        {.alias = u"seekbacklarge", .actionId = Fooyin::Constants::Actions::SeekBackwardLarge},
        {.alias = u"seekbackwardlarge", .actionId = Fooyin::Constants::Actions::SeekBackwardLarge},
        {.alias = u"settings", .actionId = Fooyin::Constants::Actions::Settings},
        {.alias = u"preferences", .actionId = Fooyin::Constants::Actions::Settings},
        {.alias = u"search", .actionId = Fooyin::Constants::Actions::SearchPlaylist},
        {.alias = u"quicksearch", .actionId = Fooyin::Constants::Actions::QuickSearch},
        {.alias = u"searchlibrary", .actionId = Fooyin::Constants::Actions::SearchLibrary},
        {.alias = u"refresh", .actionId = Fooyin::Constants::Actions::Refresh},
        {.alias = u"refreshlibrary", .actionId = Fooyin::Constants::Actions::Refresh},
        {.alias = u"rescan", .actionId = Fooyin::Constants::Actions::Rescan},
        {.alias = u"rescanlibrary", .actionId = Fooyin::Constants::Actions::Rescan},
        {.alias = u"addfiles", .actionId = Fooyin::Constants::Actions::AddFiles},
        {.alias = u"addfolders", .actionId = Fooyin::Constants::Actions::AddFolders},
        {.alias = u"newplaylist", .actionId = Fooyin::Constants::Actions::NewPlaylist},
        {.alias = u"newautoplaylist", .actionId = Fooyin::Constants::Actions::NewAutoPlaylist},
        {.alias = u"loadplaylist", .actionId = Fooyin::Constants::Actions::LoadPlaylist},
        {.alias = u"saveplaylist", .actionId = Fooyin::Constants::Actions::SavePlaylist},
        {.alias = u"saveallplaylists", .actionId = Fooyin::Constants::Actions::SaveAllPlaylists},
        {.alias = u"quit", .actionId = Fooyin::Constants::Actions::Exit},
        {.alias = u"exit", .actionId = Fooyin::Constants::Actions::Exit},
        {.alias = u"log", .actionId = Fooyin::Constants::Actions::Log},
        {.alias = u"scripteditor", .actionId = Fooyin::Constants::Actions::ScriptEditor},
        {.alias = u"nowplaying", .actionId = Fooyin::Constants::Actions::ShowNowPlaying},
        {.alias = u"quicksetup", .actionId = Fooyin::Constants::Actions::QuickSetup},
        {.alias = u"togglemenubar", .actionId = Fooyin::Constants::Actions::ToggleMenubar},
        {.alias = u"properties", .actionId = Fooyin::Constants::Actions::OpenProperties},
        {.alias = u"playingproperties", .actionId = nullptr, .type = ResolvedCommand::Type::PlayingProperties},
        {.alias = u"nowplayingproperties", .actionId = nullptr, .type = ResolvedCommand::Type::PlayingProperties},
        {.alias = u"playingfolder", .actionId = nullptr, .type = ResolvedCommand::Type::PlayingFolder},
        {.alias = u"nowplayingfolder", .actionId = nullptr, .type = ResolvedCommand::Type::PlayingFolder},
    });

    for(const auto& [alias, actionId, type] : aliases) {
        if(normalized == alias) {
            if(type == ResolvedCommand::Type::PlayingProperties) {
                return {.id = u"playingproperties"_s, .type = type};
            }
            if(type == ResolvedCommand::Type::PlayingFolder) {
                return {.id = u"playingfolder"_s, .type = type};
            }
            return {.id = QString::fromLatin1(actionId), .type = type};
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

bool ScriptCommandHandler::execute(const QString& commandId) const
{
    if(commandId.isEmpty()) {
        return false;
    }

    const ResolvedCommand resolved = resolveCommandId(commandId);

    if(resolved.type == ResolvedCommand::Type::PlayingProperties) {
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

    if(resolved.type == ResolvedCommand::Type::PlayingFolder) {
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
