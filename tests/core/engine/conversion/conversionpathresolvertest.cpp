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

#include <core/engine/conversion/conversionpathresolver.h>

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
Track makeTrack(const QString& path, const QString& title = {})
{
    Track track{path};
    if(!title.isEmpty()) {
        track.setTitle(title);
    }
    return track;
}

ConversionDestination destination(DestinationMode mode, QString pattern = u"%title%"_s)
{
    ConversionDestination dest;
    dest.mode            = mode;
    dest.filenamePattern = std::move(pattern);
    return dest;
}
} // namespace

TEST(ConversionPathResolverTest, ResolvesSourceFolderDestination)
{
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const Track track = makeTrack(QDir{dir.path()}.filePath(u"source.wav"_s), u"Song One"_s);

    const QString path
        = ConversionPathResolver::previewPath(track, destination(DestinationMode::SourceFolder), u"flac"_s);

    EXPECT_EQ(path, QDir::cleanPath(QDir{dir.path()}.filePath(u"Song One.flac"_s)));
}

TEST(ConversionPathResolverTest, ResolvesFixedFolderDestination)
{
    const QTemporaryDir sourceDir;
    const QTemporaryDir outputDir;
    ASSERT_TRUE(sourceDir.isValid());
    ASSERT_TRUE(outputDir.isValid());

    const Track track = makeTrack(QDir{sourceDir.path()}.filePath(u"source.wav"_s), u"Song Two"_s);

    auto dest        = destination(DestinationMode::FixedFolder);
    dest.fixedFolder = outputDir.path();

    const QString path = ConversionPathResolver::previewPath(track, dest, u".wav"_s);

    EXPECT_EQ(path, QDir::cleanPath(QDir{outputDir.path()}.filePath(u"Song Two.wav"_s)));
}

TEST(ConversionPathResolverTest, ResolvesAskFolderDestination)
{
    const QTemporaryDir sourceDir;
    const QTemporaryDir outputDir;
    ASSERT_TRUE(sourceDir.isValid());
    ASSERT_TRUE(outputDir.isValid());

    const Track track = makeTrack(QDir{sourceDir.path()}.filePath(u"source.wav"_s), u"Song Three"_s);

    const QString path
        = ConversionPathResolver::previewPath(track, destination(DestinationMode::Ask), u"opus"_s, outputDir.path());

    EXPECT_EQ(path, QDir::cleanPath(QDir{outputDir.path()}.filePath(u"Song Three.opus"_s)));
}

TEST(ConversionPathResolverTest, FormatsFilenamePattern)
{
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    Track track = makeTrack(QDir{dir.path()}.filePath(u"source.wav"_s), u"Track Title"_s);
    track.setArtists({u"Artist Name"_s});

    const QString path = ConversionPathResolver::previewPath(
        track, destination(DestinationMode::SourceFolder, u"%artist% - %title%"_s), u"flac"_s);

    EXPECT_EQ(path, QDir::cleanPath(QDir{dir.path()}.filePath(u"Artist Name - Track Title.flac"_s)));
}

TEST(ConversionPathResolverTest, SanitisesInvalidFilenameCharacters)
{
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const Track track = makeTrack(QDir{dir.path()}.filePath(u"source.wav"_s), u"A:B*C?D\"E<F>G|H"_s);

    const QString path
        = ConversionPathResolver::previewPath(track, destination(DestinationMode::SourceFolder), u"wav"_s);

    EXPECT_EQ(path, QDir::cleanPath(QDir{dir.path()}.filePath(u"A_B_C_D_E_F_G_H.wav"_s)));
}

TEST(ConversionPathResolverTest, SanitisesDirectorySeparatorsFromMetadata)
{
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    Track track = makeTrack(QDir{dir.path()}.filePath(u"source.wav"_s), u"A/B\\C"_s);
    track.setTrackNumber(u"01"_s);

    const QString path = ConversionPathResolver::previewPath(
        track, destination(DestinationMode::SourceFolder, u"%track% - %title%"_s), u"wav"_s);

    EXPECT_EQ(path, QDir::cleanPath(QDir{dir.path()}.filePath(u"01 - A-B-C.wav"_s)));
}

TEST(ConversionPathResolverTest, PreservesDirectorySeparatorsFromPattern)
{
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    Track track = makeTrack(QDir{dir.path()}.filePath(u"source.wav"_s), u"Song/Title"_s);
    track.setAlbum(u"Album/Name"_s);

    const QString path = ConversionPathResolver::previewPath(
        track, destination(DestinationMode::SourceFolder, u"%album%/%title%"_s), u"flac"_s);

    EXPECT_EQ(path, QDir::cleanPath(QDir{dir.path()}.filePath(u"Album-Name/Song-Title.flac"_s)));
}

TEST(ConversionPathResolverTest, SanitisesMetadataUsedByFunctions)
{
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const Track track = makeTrack(QDir{dir.path()}.filePath(u"source.wav"_s), u"Song/Title"_s);

    const QString path = ConversionPathResolver::previewPath(
        track, destination(DestinationMode::SourceFolder, u"$upper(%title%)"_s), u"flac"_s);

    EXPECT_EQ(path, QDir::cleanPath(QDir{dir.path()}.filePath(u"SONG-TITLE.flac"_s)));
}

TEST(ConversionPathResolverTest, FallsBackWhenPatternIsEmpty)
{
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const Track track = makeTrack(QDir{dir.path()}.filePath(u"fallback-name.wav"_s));

    const QString path
        = ConversionPathResolver::previewPath(track, destination(DestinationMode::SourceFolder, u"[]"_s), u"flac"_s);

    EXPECT_EQ(path, QDir::cleanPath(QDir{dir.path()}.filePath(u"fallback-name.flac"_s)));
}

TEST(ConversionPathResolverTest, SanitisesDirectorySeparatorsFromFallbackFilename)
{
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const Track track = makeTrack(QDir{dir.path()}.filePath(u"source.wav"_s), u"Fallback/Name"_s);

    const QString path
        = ConversionPathResolver::previewPath(track, destination(DestinationMode::SourceFolder, u"[]"_s), u"flac"_s);

    EXPECT_EQ(path, QDir::cleanPath(QDir{dir.path()}.filePath(u"Fallback-Name.flac"_s)));
}

TEST(ConversionPathResolverTest, DetectsDuplicateOutputPaths)
{
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const Track first  = makeTrack(QDir{dir.path()}.filePath(u"first.wav"_s), u"Same"_s);
    const Track second = makeTrack(QDir{dir.path()}.filePath(u"second.wav"_s), u"Same"_s);

    ConversionPathResolver::Request request;
    request.tracks      = {first, second};
    request.destination = destination(DestinationMode::SourceFolder);
    request.extension   = u"flac"_s;

    const auto results = Fooyin::ConversionPathResolver::resolve(request);

    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].status, ConversionPathStatus::Ready);
    EXPECT_EQ(results[1].status, ConversionPathStatus::Error);
    EXPECT_FALSE(results[1].error.isEmpty());
}

TEST(ConversionPathResolverTest, ResolvesSinglePathForMergedTracks)
{
    const QTemporaryDir sourceDir;
    const QTemporaryDir outputDir;
    ASSERT_TRUE(sourceDir.isValid());
    ASSERT_TRUE(outputDir.isValid());

    const Track first  = makeTrack(QDir{sourceDir.path()}.filePath(u"first.wav"_s), u"Merged Album"_s);
    const Track second = makeTrack(QDir{sourceDir.path()}.filePath(u"second.wav"_s), u"Second Track"_s);

    ConversionPathResolver::Request request;
    request.tracks                  = {first, second};
    request.destination             = destination(DestinationMode::FixedFolder);
    request.destination.fixedFolder = outputDir.path();
    request.destination.outputStyle = OutputStyle::MergeTracks;
    request.extension               = u"flac"_s;

    const auto results = Fooyin::ConversionPathResolver::resolve(request);

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results.front().status, ConversionPathStatus::Ready);
    EXPECT_EQ(results.front().outputPath, QDir{outputDir.path()}.filePath(u"Merged Album.flac"_s));
    EXPECT_EQ(results.front().tracks.size(), 2);
}

TEST(ConversionPathResolverTest, GroupsMultiTrackFilesByResolvedOutputPath)
{
    const QTemporaryDir sourceDir;
    const QTemporaryDir outputDir;
    ASSERT_TRUE(sourceDir.isValid());
    ASSERT_TRUE(outputDir.isValid());

    Track first = makeTrack(QDir{sourceDir.path()}.filePath(u"first.wav"_s), u"First Track"_s);
    first.setAlbum(u"Album A"_s);
    Track second = makeTrack(QDir{sourceDir.path()}.filePath(u"second.wav"_s), u"Second Track"_s);
    second.setAlbum(u"Album A"_s);
    Track third = makeTrack(QDir{sourceDir.path()}.filePath(u"third.wav"_s), u"Third Track"_s);
    third.setAlbum(u"Album B"_s);

    ConversionPathResolver::Request request;
    request.tracks                  = {first, second, third};
    request.destination             = destination(DestinationMode::FixedFolder, u"%album%"_s);
    request.destination.fixedFolder = outputDir.path();
    request.destination.outputStyle = OutputStyle::MultiTrackFiles;
    request.extension               = u"flac"_s;

    const auto results = Fooyin::ConversionPathResolver::resolve(request);

    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].status, ConversionPathStatus::Ready);
    EXPECT_EQ(results[0].outputPath, QDir{outputDir.path()}.filePath(u"Album A.flac"_s));
    EXPECT_EQ(results[0].tracks.size(), 2);
    EXPECT_EQ(results[1].status, ConversionPathStatus::Ready);
    EXPECT_EQ(results[1].outputPath, QDir{outputDir.path()}.filePath(u"Album B.flac"_s));
    EXPECT_EQ(results[1].tracks.size(), 1);
}

TEST(ConversionPathResolverTest, AppliesSkipExistingFilePolicy)
{
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString output = QDir{dir.path()}.filePath(u"Existing.flac"_s);
    QFile file{output};
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.close();

    const Track track = makeTrack(QDir{dir.path()}.filePath(u"source.wav"_s), u"Existing"_s);

    ConversionPathResolver::Request request;
    request.tracks                       = {track};
    request.destination                  = destination(DestinationMode::SourceFolder);
    request.destination.existingFileMode = ExistingFileMode::Skip;
    request.extension                    = u"flac"_s;

    const auto results = Fooyin::ConversionPathResolver::resolve(request);

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].status, ConversionPathStatus::Skipped);
    EXPECT_EQ(results[0].outputPath, QDir::cleanPath(output));
}

TEST(ConversionPathResolverTest, AppliesAskExistingFilePolicy)
{
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString output = QDir{dir.path()}.filePath(u"Existing.flac"_s);
    QFile file{output};
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.close();

    const Track track = makeTrack(QDir{dir.path()}.filePath(u"source.wav"_s), u"Existing"_s);

    ConversionPathResolver::Request request;
    request.tracks                       = {track};
    request.destination                  = destination(DestinationMode::SourceFolder);
    request.destination.existingFileMode = ExistingFileMode::Ask;
    request.extension                    = u"flac"_s;

    const auto results = Fooyin::ConversionPathResolver::resolve(request);

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].status, ConversionPathStatus::NeedsOverwriteDecision);
}

TEST(ConversionPathResolverTest, AppliesOverwriteExistingFilePolicy)
{
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString output = QDir{dir.path()}.filePath(u"Existing.flac"_s);
    QFile file{output};
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.close();

    const Track track = makeTrack(QDir{dir.path()}.filePath(u"source.wav"_s), u"Existing"_s);

    ConversionPathResolver::Request request;
    request.tracks                       = {track};
    request.destination                  = destination(DestinationMode::SourceFolder);
    request.destination.existingFileMode = ExistingFileMode::Overwrite;
    request.extension                    = u"flac"_s;

    const auto results = ConversionPathResolver::resolve(request);

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].status, ConversionPathStatus::Ready);
}
} // namespace Fooyin::Testing
