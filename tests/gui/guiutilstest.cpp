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

#include <gui/guiutils.h>

#include <gtest/gtest.h>

#include <QFile>
#include <QMimeData>
#include <QTemporaryDir>
#include <QUrl>

namespace Fooyin::Testing {
TEST(GuiUtilsTest, PopulateExternalTrackMimeDataIncludesUniqueExistingLocalFiles)
{
    const QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    const QString firstPath  = tempDir.filePath(QStringLiteral("first.flac"));
    const QString secondPath = tempDir.filePath(QStringLiteral("second.mp3"));

    QFile firstFile{firstPath};
    ASSERT_TRUE(firstFile.open(QIODevice::WriteOnly));
    firstFile.close();

    QFile secondFile{secondPath};
    ASSERT_TRUE(secondFile.open(QIODevice::WriteOnly));
    secondFile.close();

    const TrackList tracks{
        Track{firstPath},
        Track{firstPath},
        Track{tempDir.filePath(QStringLiteral("missing.ogg"))},
        Track{secondPath},
    };

    QMimeData mimeData;
    Gui::populateExternalTrackMimeData(tracks, &mimeData);

    ASSERT_TRUE(mimeData.hasUrls());
    const QList<QUrl> urls = mimeData.urls();
    ASSERT_EQ(2, urls.size());
    EXPECT_EQ(QUrl::fromLocalFile(firstPath), urls.at(0));
    EXPECT_EQ(QUrl::fromLocalFile(secondPath), urls.at(1));
    EXPECT_EQ(firstPath + QStringLiteral("\n") + secondPath, mimeData.text());
}

TEST(GuiUtilsTest, PopulateExternalTrackMimeDataSkipsInvalidSelections)
{
    QMimeData mimeData;
    Gui::populateExternalTrackMimeData({Track{}, Track{QStringLiteral("/definitely/missing.flac")}}, &mimeData);

    EXPECT_FALSE(mimeData.hasUrls());
    EXPECT_FALSE(mimeData.hasText());
}
} // namespace Fooyin::Testing
