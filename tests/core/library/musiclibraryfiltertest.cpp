/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "testutils.h"

#include <core/library/libraryfilter.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QDataStream>
#include <QSignalSpy>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
Track makeTrack(int id, const QString& title)
{
    Track track;
    track.setId(id);
    track.setTitle(title);
    return track;
}

LibraryFilter makeFilter(int id, const QString& expression)
{
    return {.id = id, .name = u"Filter %1"_s.arg(id), .expression = expression};
}
} // namespace

TEST(MusicLibraryFilterTest, FiltersAndClearsVisibleLibraryTracks)
{
    StubMusicLibrary library;
    library.setTracks({makeTrack(1, u"Jazz Song"_s), makeTrack(2, u"Rock Song"_s)});

    QSignalSpy tracksChanged{&library, &MusicLibrary::visibleLibraryTracksChanged};
    library.setActiveLibraryFilters({makeFilter(1, u"title:jazz"_s)});

    ASSERT_TRUE(tracksChanged.wait());
    ASSERT_TRUE(library.hasActiveLibraryFilters());
    ASSERT_EQ(1, library.activeLibraryFilters().size());
    EXPECT_EQ(1, library.activeLibraryFilters().front().id);
    ASSERT_EQ(1, library.visibleLibraryTracks().size());
    EXPECT_EQ(1, library.visibleLibraryTracks().front().id());
    EXPECT_EQ(2, library.libraryTracks().size());

    library.clearActiveLibraryFilters();

    EXPECT_FALSE(library.hasActiveLibraryFilters());
    EXPECT_EQ(2, library.visibleLibraryTracks().size());
    EXPECT_EQ(2, tracksChanged.count());
}

TEST(MusicLibraryFilterTest, ReappliesActiveFilterWhenLibraryChanges)
{
    StubMusicLibrary library;
    library.setTracks({makeTrack(1, u"Jazz Song"_s)});

    QSignalSpy tracksChanged{&library, &MusicLibrary::visibleLibraryTracksChanged};
    library.setActiveLibraryFilters({makeFilter(1, u"title:jazz"_s)});
    ASSERT_TRUE(tracksChanged.wait());

    library.setTracks({makeTrack(1, u"Jazz Song"_s), makeTrack(2, u"Jazz Suite"_s)});
    library.emitTracksLoaded();

    ASSERT_TRUE(tracksChanged.wait());
    EXPECT_EQ(2, library.visibleLibraryTracks().size());
}

TEST(MusicLibraryFilterTest, IncludesMatchingTracksAddedWhileFilterIsActive)
{
    StubMusicLibrary library;
    library.setTracks({makeTrack(1, u"Jazz Song"_s)});

    QSignalSpy tracksChanged{&library, &MusicLibrary::visibleLibraryTracksChanged};
    library.setActiveLibraryFilters({makeFilter(1, u"title:jazz"_s)});
    ASSERT_TRUE(tracksChanged.wait());

    const Track matchingTrack = makeTrack(2, u"Jazz Suite"_s);
    const Track excludedTrack = makeTrack(3, u"Rock Song"_s);
    library.setTracks({makeTrack(1, u"Jazz Song"_s), matchingTrack, excludedTrack});
    Q_EMIT library.tracksAdded({matchingTrack, excludedTrack});

    ASSERT_TRUE(tracksChanged.wait());
    ASSERT_EQ(2, library.visibleLibraryTracks().size());
    EXPECT_EQ(1, library.visibleLibraryTracks().at(0).id());
    EXPECT_EQ(2, library.visibleLibraryTracks().at(1).id());
}

TEST(MusicLibraryFilterTest, CombinesActiveFiltersWithAnd)
{
    StubMusicLibrary library;
    library.setTracks({makeTrack(1, u"Jazz Song"_s), makeTrack(2, u"Jazz Suite"_s), makeTrack(3, u"Rock Suite"_s)});

    QSignalSpy tracksChanged{&library, &MusicLibrary::visibleLibraryTracksChanged};
    library.setActiveLibraryFilters({makeFilter(1, u"title:jazz"_s), makeFilter(2, u"title:suite"_s)});

    ASSERT_TRUE(tracksChanged.wait());
    ASSERT_EQ(1, library.visibleLibraryTracks().size());
    EXPECT_EQ(2, library.visibleLibraryTracks().front().id());
}
} // namespace Fooyin::Testing

int main(int argc, char** argv)
{
    const QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
