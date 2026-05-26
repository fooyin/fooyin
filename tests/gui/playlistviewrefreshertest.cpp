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

#include <core/application.h>

#include <gui/playlist/playlistviewrefresher.h>

#include <QCoreApplication>
#include <QStandardPaths>

#include <gtest/gtest.h>

#include "../../src/gui/playlist/playlistviewrefresherimpl.h"

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
QCoreApplication* ensureCoreApplication()
{
    QStandardPaths::setTestModeEnabled(true);

    if(auto* app = QCoreApplication::instance()) {
        return app;
    }

    static int argc{1};
    static char appName[]        = "fooyin-playlistviewrefresher-test";
    static char* argv[]          = {appName, nullptr};
    static QCoreApplication* app = []() {
        auto* instance = new QCoreApplication(argc, argv);
        QCoreApplication::setApplicationName(QString::fromLatin1(appName));
        return instance;
    }();
    return app;
}

class FakePlaylistViewRefreshSource : public Fooyin::PlaylistViewRefreshSource
{
public:
    void refreshPlaylist(const UId& playlistId) override
    {
        refreshedPlaylists.push_back(playlistId);
    }

    void refreshEntries(const UId& playlistId, std::span<const UId> entryIds) override
    {
        refreshedEntryPlaylists.push_back(playlistId);
        refreshedEntries.emplace_back(entryIds.begin(), entryIds.end());
    }

    std::vector<UId> refreshedPlaylists;
    std::vector<UId> refreshedEntryPlaylists;
    std::vector<std::vector<UId>> refreshedEntries;
};
} // namespace

class PlaylistViewRefresherTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ensureCoreApplication();
        m_source    = std::make_unique<FakePlaylistViewRefreshSource>();
        m_refresher = std::make_unique<PlaylistViewRefresherImpl>(m_source.get());
    }

    std::unique_ptr<FakePlaylistViewRefreshSource> m_source;
    std::unique_ptr<PlaylistViewRefresher> m_refresher;
};

TEST_F(PlaylistViewRefresherTest, RefreshPlaylistDelegatesToSource)
{
    const UId playlistId;

    m_refresher->refreshPlaylist(playlistId);

    ASSERT_EQ(1, m_source->refreshedPlaylists.size());
    EXPECT_EQ(playlistId, m_source->refreshedPlaylists.front());
}

TEST_F(PlaylistViewRefresherTest, RefreshEntriesDelegatesToSource)
{
    const UId playlistId;
    const std::array entryIds{UId{}, UId{}, UId{}};

    m_refresher->refreshEntries(playlistId, entryIds);

    ASSERT_EQ(1, m_source->refreshedEntryPlaylists.size());
    EXPECT_EQ(playlistId, m_source->refreshedEntryPlaylists.front());
    ASSERT_EQ(1, m_source->refreshedEntries.size());
    EXPECT_EQ((std::vector<UId>{entryIds.begin(), entryIds.end()}), m_source->refreshedEntries.front());
}
} // namespace Fooyin::Testing
