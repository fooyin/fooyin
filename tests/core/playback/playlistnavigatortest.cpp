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

#include <core/playlist/playlist.h>

#include <core/playback/playlistnavigator.h>
#include <utils/settings/settingsmanager.h>

#include "testutils.h"

#include <QDir>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
Track makeTrack(const QString& path, int subsong, uint64_t durationMs)
{
    Track track{path, subsong};
    track.setDuration(durationMs);
    track.generateHash();
    return track;
}
} // namespace

TEST(PlaylistNavigatorTest, CurrentPreviewAndAdvanceBehaveAsExpected)
{
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playlistnavigator_test.ini"_s};
    auto playlist = PlaylistTestUtils::createPlaylist(u"Navigator"_s, &settings);
    ASSERT_TRUE(playlist);

    const TrackList tracks{
        makeTrack(u"/tmp/a.flac"_s, 0, 2000),
        makeTrack(u"/tmp/b.flac"_s, 0, 3000),
        makeTrack(u"/tmp/c.flac"_s, 0, 4000),
    };
    PlaylistTestUtils::replaceTracks(*playlist, tracks);
    PlaylistTestUtils::changeCurrentIndex(*playlist, 0);

    const PlaylistNavigator navigator{nullptr};

    const PlaylistTrack current = navigator.currentTrack(playlist.get());
    EXPECT_TRUE(current.isValid());
    EXPECT_EQ(current.indexInPlaylist, 0);
    EXPECT_EQ(current.track.uniqueFilepath(), tracks[0].uniqueFilepath());

    const PlaylistTrack preview = navigator.previewRelativeTrack(playlist.get(), Playlist::Default, 1);
    EXPECT_TRUE(preview.isValid());
    EXPECT_EQ(preview.indexInPlaylist, 1);
    EXPECT_EQ(preview.track.uniqueFilepath(), tracks[1].uniqueFilepath());
    EXPECT_EQ(playlist->currentTrackIndex(), 0);

    const PlaylistTrack advanced = navigator.advanceRelativeTrack(playlist.get(), Playlist::Default, 1);
    EXPECT_TRUE(advanced.isValid());
    EXPECT_EQ(advanced.indexInPlaylist, 1);
    EXPECT_EQ(advanced.track.uniqueFilepath(), tracks[1].uniqueFilepath());
    EXPECT_EQ(playlist->currentTrackIndex(), 1);
}

TEST(PlaylistNavigatorTest, HandlesNullPlaylistAndOutOfRangePreview)
{
    const PlaylistNavigator navigator{nullptr};

    EXPECT_FALSE(navigator.currentTrack(nullptr).isValid());
    EXPECT_FALSE(navigator.previewRelativeTrack(nullptr, Playlist::Default, 1).isValid());
    EXPECT_FALSE(navigator.advanceRelativeTrack(nullptr, Playlist::Default, 1).isValid());

    SettingsManager settings{QDir::tempPath() + u"/fooyin_playlistnavigator_empty_test.ini"_s};
    auto playlist = PlaylistTestUtils::createPlaylist(u"Empty"_s, &settings);
    ASSERT_TRUE(playlist);

    EXPECT_FALSE(navigator.previewRelativeTrack(playlist.get(), Playlist::Default, 1).isValid());
    EXPECT_FALSE(navigator.advanceRelativeTrack(playlist.get(), Playlist::Default, 1).isValid());
}
} // namespace Fooyin::Testing
