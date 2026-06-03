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

#include "testutils.h"

#include <core/corepaths.h>
#include <core/coresettings.h>
#include <core/engine/input/ratingtagpolicy.h>
#include <core/playlist/playlist.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace Fooyin::Testing {
namespace {
QString ratingSettingsPath()
{
    static const QTemporaryDir configDir{QDir::tempPath() + QStringLiteral("/fooyin-rating-test-XXXXXX")};
    EXPECT_TRUE(configDir.isValid());

    QStandardPaths::setTestModeEnabled(false);
    qputenv("XDG_CONFIG_HOME", configDir.path().toUtf8());
    return Core::settingsPath();
}
} // namespace

QString testFilePath(const QString& relativePath)
{
    const QDir testsDir{QStringLiteral(FOOYIN_TEST_SOURCE_DIR)};
    return testsDir.absoluteFilePath(relativePath);
}

void resetRatingSettings()
{
    const QString settingsPath = ratingSettingsPath();
    if(QFileInfo::exists(settingsPath)) {
        ASSERT_TRUE(QFile::remove(settingsPath));
    }
    ASSERT_TRUE(QDir{}.mkpath(QFileInfo{settingsPath}.absolutePath()));
    QSettings settings{settingsPath, QSettings::IniFormat};
    settings.remove(RatingSettings::ReadTag);
    settings.remove(RatingSettings::ReadScale);
    settings.remove(RatingSettings::WriteTag);
    settings.remove(RatingSettings::WriteScale);
    settings.remove(RatingSettings::ReadId3Popm);
    settings.remove(RatingSettings::WriteId3Popm);
    settings.remove(RatingSettings::PopmOwner);
    settings.remove(RatingSettings::PopmMapping);
    settings.remove(RatingSettings::ReadAsfSharedRating);
    settings.remove(RatingSettings::WriteAsfSharedRating);
    settings.sync();
}

TempResource::TempResource(const QString& filename, QObject* parent)
    : QTemporaryFile{parent}
    , m_file{filename}
{
    setFileTemplate(QDir::tempPath() + QStringLiteral("/fooyin_test_XXXXXXXXXXXXXXX"));

    if(open()) {
        QFile resource{filename};
        if(resource.open(QIODevice::ReadOnly)) {
            write(resource.readAll());
        }
    }

    QTemporaryFile::reset();
}
void TempResource::checkValid() const
{
    QByteArray origFileData;
    QByteArray tmpFileData;
    {
        QFile origFile{m_file};
        const bool isOpen = origFile.open(QIODevice::ReadOnly);

        EXPECT_TRUE(origFile.isOpen());

        if(isOpen) {
            origFileData = origFile.readAll();
            origFile.close();
        }
    }

    {
        QFile tmpFile{fileName()};
        const bool isOpen = tmpFile.open(QIODevice::ReadOnly);

        EXPECT_TRUE(tmpFile.isOpen());

        if(isOpen) {
            tmpFileData = tmpFile.readAll();
            tmpFile.close();
        }
    }

    EXPECT_TRUE(!origFileData.isEmpty());
    EXPECT_TRUE(!tmpFileData.isEmpty());
    EXPECT_EQ(origFileData, tmpFileData);
}

std::unique_ptr<Playlist> PlaylistTestUtils::createPlaylist(const QString& name, SettingsManager* settings)
{
    return Playlist::create(name, settings);
}

void PlaylistTestUtils::replaceTracks(Playlist& playlist, const TrackList& tracks)
{
    playlist.replaceTracks(tracks);
}

void PlaylistTestUtils::changeCurrentIndex(Playlist& playlist, int index)
{
    playlist.changeCurrentIndex(index);
}
} // namespace Fooyin::Testing
