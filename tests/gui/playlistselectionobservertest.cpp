/*
 * Fooyin
 * Copyright © 2026, Piotr Wicijowski <piotr@wicijowski.pl>
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

#include <core/playlist/playlist.h>
#include <core/playlist/playlisthandler.h>
#include <gui/playlist/playlistselectionobserver.h>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QStandardPaths>

#include <gtest/gtest.h>

#include "playlist/playlistcontroller.h"
#include "playlist/playlistselectionobserverimpl.h"

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
    static char appName[]        = "fooyin-playlistselectionobserver-test";
    static char* argv[]          = {appName, nullptr};
    static QCoreApplication* app = []() {
        auto* instance = new QCoreApplication(argc, argv);
        QCoreApplication::setApplicationName(QString::fromLatin1(appName));
        return instance;
    }();
    return app;
}
} // namespace

class PlaylistSelectionObserverTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ensureCoreApplication();
        qRegisterMetaType<Fooyin::Playlist*>("Fooyin::Playlist*");

        m_app        = std::make_unique<Application>();
        m_controller = std::make_unique<PlaylistController>(m_app.get(), nullptr);
        m_observer   = std::make_unique<PlaylistSelectionObserverImpl>(m_controller.get());
    }

    Playlist* createPlaylist(const QString& name) const
    {
        return m_app->playlistHandler()->createNewPlaylist(name);
    }

    std::unique_ptr<Application> m_app;
    std::unique_ptr<PlaylistController> m_controller;
    std::unique_ptr<PlaylistSelectionObserver> m_observer;
};

TEST_F(PlaylistSelectionObserverTest, ReportsCurrentPlaylistIdentity)
{
    auto* firstPlaylist = createPlaylist(u"Observer A"_s);
    ASSERT_NE(firstPlaylist, nullptr);

    EXPECT_EQ(nullptr, m_observer->currentPlaylist());
    EXPECT_FALSE(m_observer->currentPlaylistId().isValid());

    m_observer->changeCurrentPlaylist(firstPlaylist->id());

    EXPECT_EQ(firstPlaylist, m_observer->currentPlaylist());
    EXPECT_EQ(firstPlaylist->id(), m_observer->currentPlaylistId());
}

TEST_F(PlaylistSelectionObserverTest, EmitsWhenSelectedPlaylistChanges)
{
    auto* firstPlaylist  = createPlaylist(u"Observer A"_s);
    auto* secondPlaylist = createPlaylist(u"Observer B"_s);
    ASSERT_NE(firstPlaylist, nullptr);
    ASSERT_NE(secondPlaylist, nullptr);

    QSignalSpy changedSpy{m_observer.get(), &PlaylistSelectionObserver::currentPlaylistChanged};

    m_observer->changeCurrentPlaylist(firstPlaylist->id());
    ASSERT_EQ(1, changedSpy.count());
    EXPECT_EQ(firstPlaylist, changedSpy.at(0).at(1).value<Fooyin::Playlist*>());

    m_observer->changeCurrentPlaylist(secondPlaylist->id());
    ASSERT_EQ(2, changedSpy.count());
    EXPECT_EQ(firstPlaylist, changedSpy.at(1).at(0).value<Fooyin::Playlist*>());
    EXPECT_EQ(secondPlaylist, changedSpy.at(1).at(1).value<Fooyin::Playlist*>());
}

TEST_F(PlaylistSelectionObserverTest, DoesNotEmitForActivePlaybackPlaylistChanges)
{
    auto* firstPlaylist  = createPlaylist(u"Observer A"_s);
    auto* secondPlaylist = createPlaylist(u"Observer B"_s);
    ASSERT_NE(firstPlaylist, nullptr);
    ASSERT_NE(secondPlaylist, nullptr);

    m_observer->changeCurrentPlaylist(firstPlaylist->id());

    QSignalSpy changedSpy{m_observer.get(), &PlaylistSelectionObserver::currentPlaylistChanged};

    m_app->playlistHandler()->changeActivePlaylist(secondPlaylist);

    EXPECT_EQ(0, changedSpy.count());
    EXPECT_EQ(firstPlaylist, m_observer->currentPlaylist());
    EXPECT_EQ(firstPlaylist->id(), m_observer->currentPlaylistId());
}

TEST_F(PlaylistSelectionObserverTest, DoesNotEmitWhenReselectingCurrentPlaylist)
{
    auto* firstPlaylist = createPlaylist(u"Observer A"_s);
    ASSERT_NE(firstPlaylist, nullptr);

    m_observer->changeCurrentPlaylist(firstPlaylist->id());

    QSignalSpy changedSpy{m_observer.get(), &PlaylistSelectionObserver::currentPlaylistChanged};

    m_observer->changeCurrentPlaylist(firstPlaylist->id());

    EXPECT_EQ(0, changedSpy.count());
}

TEST_F(PlaylistSelectionObserverTest, IgnoresInvalidPlaylistIds)
{
    auto* firstPlaylist = createPlaylist(u"Observer A"_s);
    ASSERT_NE(firstPlaylist, nullptr);

    m_observer->changeCurrentPlaylist(firstPlaylist->id());

    QSignalSpy changedSpy{m_observer.get(), &PlaylistSelectionObserver::currentPlaylistChanged};

    m_observer->changeCurrentPlaylist(UId{});

    EXPECT_EQ(0, changedSpy.count());
    EXPECT_EQ(firstPlaylist, m_observer->currentPlaylist());
    EXPECT_EQ(firstPlaylist->id(), m_observer->currentPlaylistId());
}
} // namespace Fooyin::Testing
