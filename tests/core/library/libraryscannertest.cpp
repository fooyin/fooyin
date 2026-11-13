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

#include "core/library/libraryscansession.h"
#include "core/library/libraryscanstate.h"
#include "core/library/libraryscanutils.h"
#include "core/library/libraryscanwriter.h"
#include "core/library/librarytrackresolver.h"
#include "core/playlist/parsers/cueparser.h"
#include "core/playlist/playlistloader.h"

#include <core/engine/audioloader.h>
#include <core/trackmetadatastore.h>

#include <QCoreApplication>
#include <QFile>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace {
class DummyScanHost : public Fooyin::LibraryScanHost
{
public:
    [[nodiscard]] bool stopRequested() const override
    {
        return false;
    }

    void reportProgress(int /*current*/, const QString& /*file*/, int /*total*/, int /*phase*/,
                        int /*discovered*/) override
    { }
    void reportScanUpdate(const Fooyin::ScanResult& /*result*/) override { }
};

class FakeEmbeddedCueReader : public Fooyin::AudioReader
{
public:
    struct State
    {
        int readCalls{0};
        QString cueSheet;
    };

    explicit FakeEmbeddedCueReader(std::shared_ptr<State> state)
        : m_state{std::move(state)}
    { }

    QStringList extensions() const override
    {
        return {u"flac"_s, u"bin"_s};
    }

    bool canReadCover() const override
    {
        return false;
    }

    bool canWriteMetaData() const override
    {
        return false;
    }

    bool readTrack(const Fooyin::AudioSource&, Fooyin::Track& track) override
    {
        ++m_state->readCalls;
        track.setTitle(u"Embedded Album"_s);
        if(!m_state->cueSheet.isEmpty()) {
            track.replaceExtraTag(u"CUESHEET"_s, m_state->cueSheet);
        }
        return true;
    }

private:
    std::shared_ptr<State> m_state;
};

void writeFile(const QString& path, const QByteArray& data)
{
    QFile file{path};
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(data.size(), file.write(data));
}

void sortTracks(Fooyin::TrackList& tracks)
{
    std::ranges::sort(tracks, [](const Fooyin::Track& lhs, const Fooyin::Track& rhs) {
        if(lhs.filepath() != rhs.filepath()) {
            return lhs.filepath() < rhs.filepath();
        }
        if(lhs.offset() != rhs.offset()) {
            return lhs.offset() < rhs.offset();
        }
        return lhs.title() < rhs.title();
    });
}
} // namespace

namespace Fooyin::Testing {
TEST(LibraryScannerTest, MatchingCuePrefersCueNamedForTargetFile)
{
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString flacPath       = dir.filePath(u"Abbey Road.flac"_s);
    const QString genericCuePath = dir.filePath(u"Abbey Road.cue"_s);
    const QString exactCuePath   = dir.filePath(u"Abbey Road.flac.cue"_s);

    writeFile(flacPath, "flac");
    writeFile(genericCuePath, "FILE \"Abbey Road.wav\" WAVE\n");
    writeFile(exactCuePath, "FILE \"Abbey Road.flac\" WAVE\n");

    const auto match = findMatchingCue(QFileInfo{flacPath});
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(normalisePath(exactCuePath), normalisePath(match->absoluteFilePath()));
}

TEST(LibraryScannerTest, LocalCueDoesNotBlockEmbeddedCueTracks)
{
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString flacPath = dir.filePath(u"album.flac"_s);
    const QString cuePath  = dir.filePath(u"album.cue"_s);

    writeFile(flacPath, "flac");
    writeFile(cuePath, "FILE \"album.flac\" WAVE\n"
                       "  TRACK 01 AUDIO\n"
                       "    TITLE \"Local One\"\n"
                       "    INDEX 01 00:00:00\n");

    PlaylistLoader playlistLoader;
    playlistLoader.addParser(std::make_unique<CueParser>());

    AudioLoader audioLoader;
    const auto readerState = std::make_shared<FakeEmbeddedCueReader::State>();
    readerState->cueSheet  = uR"(FILE "album.flac" FLAC
  TRACK 01 AUDIO
    TITLE "Embedded One"
    INDEX 01 00:00:00
  TRACK 02 AUDIO
    TITLE "Embedded Two"
    INDEX 01 01:00:00
)"_s;
    audioLoader.addReader(
        u"fake-embedded"_s, [readerState]() { return std::make_unique<FakeEmbeddedCueReader>(readerState); }, 0);

    DummyScanHost host;
    LibraryScanState state{&host};
    LibraryScanWriter writer{nullptr, [](const ScanResult&) { }};
    auto metadataStore = std::make_shared<Fooyin::TrackMetadataStore>();
    LibraryTrackResolver resolver{{},      &playlistLoader, &audioLoader, false, metadataStore,
                                  nullptr, &state,          &writer,      {},    [] { }};

    resolver.readCue(QFileInfo{cuePath}, false);
    const int callsAfterCue = readerState->readCalls;

    writer.reset();
    resolver.readFile(QFileInfo{flacPath}, false);

    EXPECT_GT(readerState->readCalls, callsAfterCue);
    EXPECT_FALSE(writer.empty());
}

TEST(LibraryScannerTest, DroppingLocalCueAndEmbeddedFileAddsOnlyEmbeddedCueTracks)
{
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString flacPath = dir.filePath(u"album.flac"_s);
    const QString cuePath  = dir.filePath(u"album.cue"_s);

    writeFile(flacPath, "flac");
    writeFile(cuePath, "FILE \"album.flac\" WAVE\n"
                       "  TRACK 01 AUDIO\n"
                       "    TITLE \"Local One\"\n"
                       "    PERFORMER \"Local Artist\"\n"
                       "    INDEX 01 00:00:00\n");

    auto playlistLoader = std::make_shared<PlaylistLoader>();
    playlistLoader->addParser(std::make_unique<CueParser>());

    auto audioLoader       = std::make_shared<AudioLoader>();
    const auto readerState = std::make_shared<FakeEmbeddedCueReader::State>();
    readerState->cueSheet  = uR"(PERFORMER "Embedded Artist"
FILE "album.flac" FLAC
  TRACK 01 AUDIO
    TITLE "Embedded One"
    INDEX 01 00:00:00
  TRACK 02 AUDIO
    TITLE "Embedded Two"
    INDEX 01 01:00:00
)"_s;
    audioLoader->addReader(
        u"fake-embedded"_s, [readerState]() { return std::make_unique<FakeEmbeddedCueReader>(readerState); }, 0);

    LibraryScanConfig config;
    config.externalRestrictExt = {u"cue"_s, u"flac"_s};
    DummyScanHost host;
    auto metadataStore = std::make_shared<Fooyin::TrackMetadataStore>();
    LibraryScanSession session{nullptr, playlistLoader.get(), audioLoader.get(), metadataStore, config, &host};
    LibraryScanFilesResult result;

    ASSERT_TRUE(session.scanFiles({}, {QUrl::fromLocalFile(cuePath), QUrl::fromLocalFile(flacPath)}, result));
    sortTracks(result.tracksScanned);
    const TrackList& scannedTracks = result.tracksScanned;

    ASSERT_EQ(2, scannedTracks.size());
    EXPECT_EQ(u"Embedded Artist"_s, scannedTracks.at(0).artist());
    EXPECT_EQ(u"Embedded Artist"_s, scannedTracks.at(1).artist());
    EXPECT_TRUE(scannedTracks.at(0).albumArtist().isEmpty());
    EXPECT_TRUE(scannedTracks.at(1).albumArtist().isEmpty());
    EXPECT_EQ(u"Embedded One"_s, scannedTracks.at(0).title());
    EXPECT_EQ(u"Embedded Two"_s, scannedTracks.at(1).title());
}

TEST(LibraryScannerTest, DroppingLocalCueAndBackingFileAddsOnlyCueTracks)
{
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString binPath = dir.filePath(u"album.bin"_s);
    const QString cuePath = dir.filePath(u"album.cue"_s);

    writeFile(binPath, "bin");
    writeFile(cuePath, "PERFORMER \"Local Artist\"\n"
                       "TITLE \"Local Album\"\n"
                       "FILE \"album.bin\" BINARY\n"
                       "  TRACK 01 AUDIO\n"
                       "    TITLE \"Local One\"\n"
                       "    INDEX 01 00:00:00\n"
                       "  TRACK 02 AUDIO\n"
                       "    TITLE \"Local Two\"\n"
                       "    INDEX 01 01:00:00\n");

    auto playlistLoader = std::make_shared<PlaylistLoader>();
    playlistLoader->addParser(std::make_unique<CueParser>());

    auto audioLoader       = std::make_shared<AudioLoader>();
    const auto readerState = std::make_shared<FakeEmbeddedCueReader::State>();
    audioLoader->addReader(
        u"fake-embedded"_s, [readerState]() { return std::make_unique<FakeEmbeddedCueReader>(readerState); }, 0);

    LibraryScanConfig config;
    config.externalRestrictExt = {u"cue"_s, u"bin"_s};
    DummyScanHost host;
    auto metadataStore = std::make_shared<Fooyin::TrackMetadataStore>();
    LibraryScanSession session{nullptr, playlistLoader.get(), audioLoader.get(), metadataStore, config, &host};
    LibraryScanFilesResult result;

    ASSERT_TRUE(session.scanFiles({}, {QUrl::fromLocalFile(cuePath), QUrl::fromLocalFile(binPath)}, result));
    sortTracks(result.tracksScanned);
    const TrackList& scannedTracks = result.tracksScanned;

    ASSERT_EQ(2, scannedTracks.size());
    EXPECT_EQ(u"Local One"_s, scannedTracks.at(0).title());
    EXPECT_EQ(u"Local Two"_s, scannedTracks.at(1).title());
    EXPECT_EQ(u"Local Artist"_s, scannedTracks.at(0).artist());
    EXPECT_EQ(u"Local Artist"_s, scannedTracks.at(1).artist());
    EXPECT_TRUE(scannedTracks.at(0).albumArtist().isEmpty());
    EXPECT_TRUE(scannedTracks.at(1).albumArtist().isEmpty());
}

TEST(LibraryScannerTest, DroppingBackingFileBeforeLocalCueAddsOnlyCueTracks)
{
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString binPath = dir.filePath(u"album.bin"_s);
    const QString cuePath = dir.filePath(u"album.cue"_s);

    writeFile(binPath, "bin");
    writeFile(cuePath, "PERFORMER \"Local Artist\"\n"
                       "TITLE \"Local Album\"\n"
                       "FILE \"album.bin\" BINARY\n"
                       "  TRACK 01 AUDIO\n"
                       "    TITLE \"Local One\"\n"
                       "    INDEX 01 00:00:00\n"
                       "  TRACK 02 AUDIO\n"
                       "    TITLE \"Local Two\"\n"
                       "    INDEX 01 01:00:00\n");

    auto playlistLoader = std::make_shared<PlaylistLoader>();
    playlistLoader->addParser(std::make_unique<CueParser>());

    auto audioLoader       = std::make_shared<AudioLoader>();
    const auto readerState = std::make_shared<FakeEmbeddedCueReader::State>();
    audioLoader->addReader(
        u"fake-embedded"_s, [readerState]() { return std::make_unique<FakeEmbeddedCueReader>(readerState); }, 0);

    LibraryScanConfig config;
    config.externalRestrictExt = {u"cue"_s, u"bin"_s};
    DummyScanHost host;
    auto metadataStore = std::make_shared<Fooyin::TrackMetadataStore>();
    LibraryScanSession session{nullptr, playlistLoader.get(), audioLoader.get(), metadataStore, config, &host};
    LibraryScanFilesResult result;

    ASSERT_TRUE(session.scanFiles({}, {QUrl::fromLocalFile(binPath), QUrl::fromLocalFile(cuePath)}, result));
    sortTracks(result.tracksScanned);
    const TrackList& scannedTracks = result.tracksScanned;

    ASSERT_EQ(2, scannedTracks.size());
    EXPECT_EQ(u"Local One"_s, scannedTracks.at(0).title());
    EXPECT_EQ(u"Local Two"_s, scannedTracks.at(1).title());
    EXPECT_EQ(u"Local Artist"_s, scannedTracks.at(0).artist());
    EXPECT_EQ(u"Local Artist"_s, scannedTracks.at(1).artist());
    EXPECT_TRUE(scannedTracks.at(0).albumArtist().isEmpty());
    EXPECT_TRUE(scannedTracks.at(1).albumArtist().isEmpty());
}

TEST(LibraryScannerTest, DroppingDirectoryWithLocalCueAndEmbeddedFileAddsOnlyEmbeddedCueTracks)
{
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    const QString flacPath = dir.filePath(u"album.flac"_s);
    const QString cuePath  = dir.filePath(u"album.cue"_s);

    writeFile(flacPath, "flac");
    writeFile(cuePath, "FILE \"album.flac\" WAVE\n"
                       "  TRACK 01 AUDIO\n"
                       "    TITLE \"Local One\"\n"
                       "    PERFORMER \"Local Artist\"\n"
                       "    INDEX 01 00:00:00\n");

    auto playlistLoader = std::make_shared<PlaylistLoader>();
    playlistLoader->addParser(std::make_unique<CueParser>());

    auto audioLoader       = std::make_shared<AudioLoader>();
    const auto readerState = std::make_shared<FakeEmbeddedCueReader::State>();
    readerState->cueSheet  = uR"(PERFORMER "Embedded Artist"
FILE "album.flac" FLAC
  TRACK 01 AUDIO
    TITLE "Embedded One"
    INDEX 01 00:00:00
  TRACK 02 AUDIO
    TITLE "Embedded Two"
    INDEX 01 01:00:00
)"_s;
    audioLoader->addReader(
        u"fake-embedded"_s, [readerState]() { return std::make_unique<FakeEmbeddedCueReader>(readerState); }, 0);

    LibraryScanConfig config;
    config.externalRestrictExt = {u"cue"_s, u"flac"_s};
    DummyScanHost host;
    auto metadataStore = std::make_shared<Fooyin::TrackMetadataStore>();
    LibraryScanSession session{nullptr, playlistLoader.get(), audioLoader.get(), metadataStore, config, &host};
    LibraryScanFilesResult result;

    ASSERT_TRUE(session.scanFiles({}, {QUrl::fromLocalFile(dir.path())}, result));
    sortTracks(result.tracksScanned);
    const TrackList& scannedTracks = result.tracksScanned;

    ASSERT_EQ(2, scannedTracks.size());
    EXPECT_EQ(u"Embedded Artist"_s, scannedTracks.at(0).artist());
    EXPECT_EQ(u"Embedded Artist"_s, scannedTracks.at(1).artist());
    EXPECT_TRUE(scannedTracks.at(0).albumArtist().isEmpty());
    EXPECT_TRUE(scannedTracks.at(1).albumArtist().isEmpty());
    EXPECT_EQ(u"Embedded One"_s, scannedTracks.at(0).title());
    EXPECT_EQ(u"Embedded Two"_s, scannedTracks.at(1).title());
}
} // namespace Fooyin::Testing

int main(int argc, char* argv[])
{
    QStandardPaths::setTestModeEnabled(true);
    QLoggingCategory::setFilterRules(u"fy.db.info=false"_s);
    const QCoreApplication application{argc, argv};
    QCoreApplication::setApplicationName(u"fooyin-libraryscanner-test"_s);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
