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

#include <core/coresettings.h>
#include <core/engine/audioloader.h>
#include <core/network/remotesourceprovider.h>
#include <core/network/remotestreamdevice.h>

#include <gtest/gtest.h>

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <map>
#include <memory>
#include <utility>
#include <vector>

using namespace Qt::StringLiterals;

namespace {
class FakeRemoteStreamDevice : public QBuffer,
                               public Fooyin::RemoteStreamDevice
{
public:
    bool readWouldBlock() const override
    {
        return false;
    }

    qsizetype bufferedByteCount() const override
    {
        return bytesAvailable();
    }

    void setNonBlockingReadsEnabled(bool enabled) override
    {
        nonBlockingReadsEnabled = enabled;
    }

    bool nonBlockingReadsEnabled{false};
};

class FakeRemoteSourceProvider : public Fooyin::RemoteSourceProvider
{
public:
    mutable int calls{0};

    Fooyin::RemoteStreamSource createStreamSource(const QUrl& url) const override
    {
        ++calls;
        auto buffer = std::make_unique<FakeRemoteStreamDevice>();
        buffer->setData(url.toString().toUtf8());

        Fooyin::RemoteStreamSource source;
        source.remoteDevice = buffer.get();
        source.device       = std::move(buffer);
        return source;
    }
};

QCoreApplication* ensureCoreApplication()
{
    QStandardPaths::setTestModeEnabled(true);

    if(auto* app = QCoreApplication::instance()) {
        return app;
    }

    static int argc{1};
    static char appName[] = "fooyin-audioloader-test";
    static char* argv[]   = {appName, nullptr};
    static QCoreApplication app{argc, argv};
    QCoreApplication::setApplicationName(QString::fromLatin1(appName));
    return &app;
}

void clearLoaderSettings()
{
    Fooyin::FySettings settings;
    settings.clear();
    settings.sync();
}

Fooyin::AudioFormat testFormat()
{
    return {Fooyin::SampleFormat::S16, 44100, 2};
}

QString makeArchiveTrackPath(const QString& archivePath, const QString& innerPath, const QString& type = u"zip"_s)
{
    return u"unpack://%1|%2|file://%3!%4"_s.arg(type).arg(archivePath.size()).arg(archivePath, innerPath);
}

struct DecoderState
{
    QString label;
    QStringList extensions;
    QStringList preferredExtensions;
    bool supportsRemoteSources{false};
    bool initSucceeds{true};
    bool requireDevice{false};
    bool requireArchiveReader{false};
    Fooyin::AudioFormat format{testFormat()};

    int creatorCalls{0};
    int initCalls{0};
    int stopCalls{0};
    int seekCalls{0};
    int readBufferCalls{0};

    QString lastSourcePath;
    bool lastHadDevice{false};
    bool lastHadRemoteStreamDevice{false};
    bool lastDeviceOpen{false};
    bool lastHadArchiveReader{false};
    uint64_t lastSourceSize{0};
    uint64_t lastSourceModifiedTime{0};
    Fooyin::AudioDecoder::DecoderOptions lastOptions{Fooyin::AudioDecoder::None};
    Fooyin::AudioDecoder::PlaybackHints playbackHintsAtInit{Fooyin::AudioDecoder::NoHints};
};

class FakeDecoder : public Fooyin::AudioDecoder
{
public:
    explicit FakeDecoder(std::shared_ptr<DecoderState> state)
        : m_state{std::move(state)}
    { }

    [[nodiscard]] QString label() const
    {
        return m_state->label;
    }

    QStringList extensions() const override
    {
        return m_state->extensions;
    }

    QStringList preferredExtensions() const override
    {
        return m_state->preferredExtensions;
    }

    bool supportsRemoteSources() const override
    {
        return m_state->supportsRemoteSources;
    }

    bool isSeekable() const override
    {
        return true;
    }

    std::optional<Fooyin::AudioFormat> init(const Fooyin::AudioSource& source, const Fooyin::Track& /*track*/,
                                            DecoderOptions options) override
    {
        ++m_state->initCalls;
        m_state->lastSourcePath            = source.filepath;
        m_state->lastHadDevice             = source.device != nullptr;
        m_state->lastHadRemoteStreamDevice = source.remoteStreamDevice != nullptr;
        m_state->lastDeviceOpen            = source.device && source.device->isOpen();
        m_state->lastHadArchiveReader      = source.archiveReader != nullptr;
        m_state->lastSourceSize            = source.size;
        m_state->lastSourceModifiedTime    = source.modifiedTime;
        m_state->lastOptions               = options;
        m_state->playbackHintsAtInit       = playbackHints();

        if(m_state->requireDevice && (!source.device || !source.device->isOpen())) {
            return {};
        }
        if(m_state->requireArchiveReader && !source.archiveReader) {
            return {};
        }

        return m_state->initSucceeds ? std::optional<Fooyin::AudioFormat>{m_state->format} : std::nullopt;
    }

    void stop() override
    {
        ++m_state->stopCalls;
    }

    void seek(uint64_t /*pos*/) override
    {
        ++m_state->seekCalls;
    }

    Fooyin::AudioBuffer readBuffer(size_t /*bytes*/) override
    {
        ++m_state->readBufferCalls;
        return {};
    }

private:
    std::shared_ptr<DecoderState> m_state;
};

struct ReaderState
{
    QString label;
    QStringList extensions;
    QStringList preferredExtensions;
    bool supportsRemoteSources{false};
    bool canReadCover{false};
    bool canWriteMetadata{false};
    bool canWriteCover{false};
    bool initSucceeds{true};
    bool readSucceeds{true};
    bool writeTrackSucceeds{true};
    bool writeCoverSucceeds{true};
    bool requireDevice{false};
    bool requireArchiveReader{false};
    int subsongCount{1};
    QString title;
    QByteArray coverData;

    int creatorCalls{0};
    int initCalls{0};
    int readCalls{0};
    int readCoverCalls{0};
    int writeTrackCalls{0};
    int writeCoverCalls{0};

    QString lastSourcePath;
    bool lastHadDevice{false};
    bool lastHadRemoteStreamDevice{false};
    bool lastDeviceOpen{false};
    bool lastDeviceWritable{false};
    bool lastHadArchiveReader{false};
    uint64_t lastSourceSize{0};
    uint64_t lastSourceModifiedTime{0};
    Fooyin::AudioReader::WriteOptions lastWriteOptions{Fooyin::AudioReader::None};
    Fooyin::Track::Cover lastCover{Fooyin::Track::Cover::Front};
    size_t lastWrittenCoverCount{0};
};

class FakeReader : public Fooyin::AudioReader
{
public:
    explicit FakeReader(std::shared_ptr<ReaderState> state)
        : m_state{std::move(state)}
    { }

    [[nodiscard]] QString label() const
    {
        return m_state->label;
    }

    QStringList extensions() const override
    {
        return m_state->extensions;
    }

    QStringList preferredExtensions() const override
    {
        return m_state->preferredExtensions;
    }

    bool supportsRemoteSources() const override
    {
        return m_state->supportsRemoteSources;
    }

    bool canReadCover() const override
    {
        return m_state->canReadCover;
    }

    bool canWriteCover() const override
    {
        return m_state->canWriteCover;
    }

    bool canWriteMetaData() const override
    {
        return m_state->canWriteMetadata;
    }

    int subsongCount() const override
    {
        return m_state->subsongCount;
    }

    bool init(const Fooyin::AudioSource& source) override
    {
        ++m_state->initCalls;
        recordSource(source);
        return validateSource(source, false) && m_state->initSucceeds;
    }

    bool readTrack(const Fooyin::AudioSource& source, Fooyin::Track& track) override
    {
        ++m_state->readCalls;
        recordSource(source);
        if(!validateSource(source, false)) {
            return false;
        }

        if(!m_state->title.isEmpty()) {
            track.setTitle(m_state->title);
        }
        if(source.size > 0) {
            track.setFileSize(source.size);
        }
        if(source.modifiedTime > 0) {
            track.setModifiedTime(source.modifiedTime);
        }

        return m_state->readSucceeds;
    }

    QByteArray readCover(const Fooyin::AudioSource& source, const Fooyin::Track& /*track*/,
                         Fooyin::Track::Cover cover) override
    {
        ++m_state->readCoverCalls;
        m_state->lastCover = cover;
        recordSource(source);
        return validateSource(source, false) ? m_state->coverData : QByteArray{};
    }

    bool writeTrack(const Fooyin::AudioSource& source, const Fooyin::Track& /*track*/, WriteOptions options) override
    {
        ++m_state->writeTrackCalls;
        m_state->lastWriteOptions = options;
        recordSource(source);
        return validateSource(source, true) && m_state->writeTrackSucceeds;
    }

    bool writeCover(const Fooyin::AudioSource& source, const Fooyin::Track& /*track*/,
                    const Fooyin::TrackCovers& covers, WriteOptions options) override
    {
        ++m_state->writeCoverCalls;
        m_state->lastWriteOptions      = options;
        m_state->lastWrittenCoverCount = covers.size();
        recordSource(source);
        return validateSource(source, true) && m_state->writeCoverSucceeds;
    }

private:
    bool validateSource(const Fooyin::AudioSource& source, bool requireWritable) const
    {
        if(m_state->requireDevice && (!source.device || !source.device->isOpen())) {
            return false;
        }
        if(requireWritable && (!source.device || !source.device->isWritable())) {
            return false;
        }
        if(m_state->requireArchiveReader && !source.archiveReader) {
            return false;
        }
        return true;
    }

    void recordSource(const Fooyin::AudioSource& source)
    {
        m_state->lastSourcePath            = source.filepath;
        m_state->lastHadDevice             = source.device != nullptr;
        m_state->lastHadRemoteStreamDevice = source.remoteStreamDevice != nullptr;
        m_state->lastDeviceOpen            = source.device && source.device->isOpen();
        m_state->lastDeviceWritable        = source.device && source.device->isWritable();
        m_state->lastHadArchiveReader      = source.archiveReader != nullptr;
        m_state->lastSourceSize            = source.size;
        m_state->lastSourceModifiedTime    = source.modifiedTime;
    }

    std::shared_ptr<ReaderState> m_state;
};

struct ArchiveEntrySpec
{
    QString path;
    QByteArray data;
    uint64_t modifiedTime{0};
};

struct ArchiveReaderState
{
    QString label;
    QStringList extensions;
    QString type;
    bool initSucceeds{true};
    std::map<QString, ArchiveEntrySpec> entries;
    QByteArray coverData;

    int creatorCalls{0};
    int initCalls{0};
    int entryCalls{0};
    int readTracksCalls{0};
    int readCoverCalls{0};

    QString lastInitFile;
    QString lastEntryFile;
    Fooyin::Track::Cover lastCover{Fooyin::Track::Cover::Front};
};

class FakeArchiveReader : public Fooyin::ArchiveReader
{
public:
    explicit FakeArchiveReader(std::shared_ptr<ArchiveReaderState> state)
        : m_state{std::move(state)}
    { }

    [[nodiscard]] QString label() const
    {
        return m_state->label;
    }

    QStringList extensions() const override
    {
        return m_state->extensions;
    }

    QString type() const override
    {
        return m_state->type;
    }

    bool init(const QString& file) override
    {
        ++m_state->initCalls;
        m_state->lastInitFile = file;
        return m_state->initSucceeds;
    }

    Fooyin::ArchiveEntryData entry(const QString& file) override
    {
        ++m_state->entryCalls;
        m_state->lastEntryFile = file;

        const auto it = m_state->entries.find(file);
        if(it == m_state->entries.end()) {
            return {};
        }

        auto buffer = std::make_unique<QBuffer>();
        buffer->setData(it->second.data);
        buffer->open(QIODevice::ReadOnly);

        return {
            .info   = {.path          = it->second.path.isEmpty() ? file : it->second.path,
                       .modifiedTime  = it->second.modifiedTime,
                       .size          = static_cast<uint64_t>(it->second.data.size()),
                       .isRegularFile = true},
            .device = std::move(buffer),
        };
    }

    bool readTracks(ReadEntryCallback readEntry) override
    {
        ++m_state->readTracksCalls;
        for(const auto& [path, spec] : m_state->entries) {
            auto entryData = entry(path);
            if(entryData.device) {
                readEntry(std::move(entryData));
            }
        }
        return true;
    }

    QByteArray readCover(const Fooyin::Track& /*track*/, Fooyin::Track::Cover cover) override
    {
        ++m_state->readCoverCalls;
        m_state->lastCover = cover;
        return m_state->coverData;
    }

private:
    std::shared_ptr<ArchiveReaderState> m_state;
};

template <typename CreatorT>
QStringList entryNames(const std::vector<Fooyin::AudioLoader::LoaderEntry<CreatorT>>& entries)
{
    QStringList names;
    for(const auto& entry : entries) {
        names.append(entry.name);
    }
    return names;
}

template <typename CreatorT>
std::vector<uint8_t> entryEnabledStates(const std::vector<Fooyin::AudioLoader::LoaderEntry<CreatorT>>& entries)
{
    std::vector<uint8_t> enabled;
    enabled.reserve(entries.size());
    for(const auto& entry : entries) {
        enabled.push_back(entry.enabled ? 1 : 0);
    }
    return enabled;
}

QString decoderLabel(const std::unique_ptr<Fooyin::AudioDecoder>& decoder)
{
    const auto* fakeDecoder = dynamic_cast<const FakeDecoder*>(decoder.get());
    EXPECT_NE(fakeDecoder, nullptr);
    return fakeDecoder ? fakeDecoder->label() : QString{};
}

QString readerLabel(const std::unique_ptr<Fooyin::AudioReader>& reader)
{
    const auto* fakeReader = dynamic_cast<const FakeReader*>(reader.get());
    EXPECT_NE(fakeReader, nullptr);
    return fakeReader ? fakeReader->label() : QString{};
}

QString archiveReaderLabel(const std::unique_ptr<Fooyin::ArchiveReader>& reader)
{
    const auto* fakeReader = dynamic_cast<const FakeArchiveReader*>(reader.get());
    EXPECT_NE(fakeReader, nullptr);
    return fakeReader ? fakeReader->label() : QString{};
}

QStringList decoderLabels(const std::vector<std::unique_ptr<Fooyin::AudioDecoder>>& decoders)
{
    QStringList labels;
    for(const auto& decoder : decoders) {
        labels.append(decoderLabel(decoder));
    }
    return labels;
}

QStringList readerLabels(const std::vector<std::unique_ptr<Fooyin::AudioReader>>& readers)
{
    QStringList labels;
    for(const auto& reader : readers) {
        labels.append(readerLabel(reader));
    }
    return labels;
}
} // namespace

namespace Fooyin::Testing {
class AudioLoaderTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        ensureCoreApplication();
    }

    void SetUp() override
    {
        clearLoaderSettings();
        ASSERT_TRUE(m_tempDir.isValid());
    }

    void TearDown() override
    {
        clearLoaderSettings();
    }

    std::shared_ptr<DecoderState> addDecoder(AudioLoader& loader, const QString& label, const QStringList& extensions,
                                             int priority = -1, bool isArchiveWrapper = false)
    {
        auto state        = std::make_shared<DecoderState>();
        state->label      = label;
        state->extensions = extensions;

        loader.addDecoder(
            label,
            [state]() {
                ++state->creatorCalls;
                return std::make_unique<FakeDecoder>(state);
            },
            priority, isArchiveWrapper);
        return state;
    }

    std::shared_ptr<ReaderState> addReader(AudioLoader& loader, const QString& label, const QStringList& extensions,
                                           int priority = -1, bool isArchiveWrapper = false)
    {
        auto state        = std::make_shared<ReaderState>();
        state->label      = label;
        state->extensions = extensions;

        loader.addReader(
            label,
            [state]() {
                ++state->creatorCalls;
                return std::make_unique<FakeReader>(state);
            },
            priority, isArchiveWrapper);
        return state;
    }

    std::shared_ptr<ArchiveReaderState> addArchiveReader(AudioLoader& loader, const QString& label,
                                                         const QStringList& extensions, int priority = -1)
    {
        auto state        = std::make_shared<ArchiveReaderState>();
        state->label      = label;
        state->extensions = extensions;
        state->type       = label;

        loader.addArchiveReader(
            label,
            [state]() {
                ++state->creatorCalls;
                return std::make_unique<FakeArchiveReader>(state);
            },
            priority);
        return state;
    }

    QString createFile(const QString& name, const QByteArray& data = "data") const
    {
        const QString path = m_tempDir.filePath(name);
        QDir().mkpath(QFileInfo{path}.path());
        QFile file{path};
        EXPECT_TRUE(file.open(QIODevice::WriteOnly));
        if(file.isOpen()) {
            file.write(data);
            file.close();
        }
        return path;
    }

    QTemporaryDir m_tempDir;
};

TEST_F(AudioLoaderTest, RegistersLoadersAndEnumeratesPublicState)
{
    AudioLoader loader;

    const auto decoderMp3     = addDecoder(loader, u"decoder-mp3"_s, {u" MP3 "_s}, 5);
    const auto decoderFlac    = addDecoder(loader, u"decoder-flac"_s, {u"FLAC"_s}, 1);
    const auto wrapperDecoder = addDecoder(loader, u"wrapper-decoder"_s, {u"ignored"_s}, 3, true);
    const auto readerMp3      = addReader(loader, u"reader-mp3"_s, {u"mP3"_s}, 4);
    const auto readerFlac     = addReader(loader, u"reader-flac"_s, {u" flac "_s}, 0);
    const auto wrapperReader  = addReader(loader, u"wrapper-reader"_s, {u"ignored"_s}, 2, true);
    const auto archiveReader  = addArchiveReader(loader, u"zip-reader"_s, {u" ZIP "_s}, 0);

    Q_UNUSED(decoderMp3);
    Q_UNUSED(decoderFlac);
    Q_UNUSED(wrapperDecoder);
    Q_UNUSED(readerMp3);
    Q_UNUSED(readerFlac);
    Q_UNUSED(wrapperReader);
    Q_UNUSED(archiveReader);

    EXPECT_EQ((QStringList{u"decoder-mp3"_s, u"decoder-flac"_s, u"wrapper-decoder"_s}), entryNames(loader.decoders()));
    EXPECT_EQ((QStringList{u"reader-mp3"_s, u"reader-flac"_s, u"wrapper-reader"_s}), entryNames(loader.readers()));
    EXPECT_EQ((QStringList{u"zip-reader"_s}), entryNames(loader.archiveReaders()));

    EXPECT_EQ((QStringList{u"flac"_s, u"mp3"_s, u"zip"_s}), loader.supportedFileExtensions());
    EXPECT_EQ((QStringList{u"flac"_s, u"mp3"_s, u"zip"_s}), loader.supportedTrackExtensions());
    EXPECT_EQ((QStringList{u"zip"_s}), loader.supportedArchiveExtensions());

    const Track flacTrack{u"/tmp/album.flac"_s};
    const Track mp3Track{u"/tmp/album.mp3"_s};
    const Track archiveTrack{makeArchiveTrackPath(u"/tmp/archive.zip"_s, u"disc/song.mp3"_s)};

    EXPECT_EQ((QStringList{u"decoder-flac"_s}), decoderLabels(loader.decodersForFile(flacTrack.filepath())));
    EXPECT_EQ((QStringList{u"decoder-mp3"_s}), decoderLabels(loader.decodersForTrack(mp3Track)));
    EXPECT_EQ((QStringList{u"reader-flac"_s}), readerLabels(loader.readersForFile(flacTrack.filepath())));
    EXPECT_EQ((QStringList{u"reader-mp3"_s}), readerLabels(loader.readersForTrack(mp3Track)));
    EXPECT_EQ((QStringList{u"wrapper-decoder"_s}), decoderLabels(loader.decodersForFile(archiveTrack.filepath())));
    EXPECT_EQ((QStringList{u"wrapper-reader"_s}), readerLabels(loader.readersForFile(archiveTrack.filepath())));

    EXPECT_TRUE(loader.isArchive(u"/tmp/archive.zip"_s));
    EXPECT_FALSE(loader.isArchive(u"/tmp/archive.mp3"_s));

    auto selectedArchiveReader = loader.archiveReaderForFile(u"/tmp/archive.zip"_s);
    ASSERT_NE(selectedArchiveReader, nullptr);
    EXPECT_EQ(u"zip-reader"_s, archiveReaderLabel(selectedArchiveReader));
    EXPECT_EQ(nullptr, loader.archiveReaderForFile(u"/tmp/archive.rar"_s));
}

TEST_F(AudioLoaderTest, CanReorderDisableAndReloadRegularLoaders)
{
    AudioLoader loader;

    const auto decoderOne = addDecoder(loader, u"decoder-one"_s, {u"mp3"_s});
    const auto decoderTwo = addDecoder(loader, u"decoder-two"_s, {u"mp3"_s});
    const auto readerOne  = addReader(loader, u"reader-one"_s, {u"mp3"_s});
    const auto readerTwo  = addReader(loader, u"reader-two"_s, {u"mp3"_s});

    EXPECT_EQ((QStringList{u"decoder-one"_s, u"decoder-two"_s}),
              decoderLabels(loader.decodersForFile(u"/tmp/test.mp3"_s)));
    EXPECT_EQ((QStringList{u"reader-one"_s, u"reader-two"_s}), readerLabels(loader.readersForFile(u"/tmp/test.mp3"_s)));

    loader.changeDecoderIndex(u"decoder-two"_s, 0);
    loader.changeReaderIndex(u"reader-two"_s, 0);

    EXPECT_EQ((QStringList{u"decoder-two"_s, u"decoder-one"_s}), entryNames(loader.decoders()));
    EXPECT_EQ((QStringList{u"reader-two"_s, u"reader-one"_s}), entryNames(loader.readers()));
    EXPECT_EQ((QStringList{u"decoder-two"_s, u"decoder-one"_s}),
              decoderLabels(loader.decodersForFile(u"/tmp/test.mp3"_s)));
    EXPECT_EQ((QStringList{u"reader-two"_s, u"reader-one"_s}), readerLabels(loader.readersForFile(u"/tmp/test.mp3"_s)));

    loader.setDecoderEnabled(u"decoder-two"_s, false);
    loader.setReaderEnabled(u"reader-two"_s, false);

    EXPECT_EQ((std::vector<uint8_t>{0, 1}), entryEnabledStates(loader.decoders()));
    EXPECT_EQ((std::vector<uint8_t>{0, 1}), entryEnabledStates(loader.readers()));
    EXPECT_EQ((QStringList{u"decoder-one"_s}), decoderLabels(loader.decodersForFile(u"/tmp/test.mp3"_s)));
    EXPECT_EQ((QStringList{u"reader-one"_s}), readerLabels(loader.readersForFile(u"/tmp/test.mp3"_s)));

    decoderOne->extensions = {u"aac"_s};
    readerOne->extensions  = {u"aac"_s};
    loader.reloadDecoderExtensions(u"decoder-one"_s);
    loader.reloadReaderExtensions(u"reader-one"_s);

    EXPECT_EQ((QStringList{u"aac"_s, u"mp3"_s}), loader.supportedTrackExtensions());
    EXPECT_TRUE(loader.decodersForFile(u"/tmp/test.mp3"_s).empty());
    EXPECT_TRUE(loader.readersForFile(u"/tmp/test.mp3"_s).empty());
    EXPECT_EQ((QStringList{u"decoder-one"_s}), decoderLabels(loader.decodersForFile(u"/tmp/test.aac"_s)));
    EXPECT_EQ((QStringList{u"reader-one"_s}), readerLabels(loader.readersForFile(u"/tmp/test.aac"_s)));
}

TEST_F(AudioLoaderTest, PrioritisesLoadersWithPreferredExtensions)
{
    AudioLoader loader;
    addDecoder(loader, u"first-decoder"_s, {u"m4b"_s, u"mp3"_s}, 0);
    const auto preferredDecoder           = addDecoder(loader, u"preferred-decoder"_s, {u"m4b"_s, u"mp3"_s}, 99);
    preferredDecoder->preferredExtensions = {u"m4b"_s};

    addReader(loader, u"first-reader"_s, {u"m4b"_s, u"mp3"_s}, 0);
    const auto preferredReader           = addReader(loader, u"preferred-reader"_s, {u"m4b"_s, u"mp3"_s}, 99);
    preferredReader->preferredExtensions = {u"m4b"_s};

    EXPECT_EQ((QStringList{u"preferred-decoder"_s, u"first-decoder"_s}),
              decoderLabels(loader.decodersForFile(u"/tmp/test.m4b"_s)));
    EXPECT_EQ((QStringList{u"first-decoder"_s, u"preferred-decoder"_s}),
              decoderLabels(loader.decodersForFile(u"/tmp/test.mp3"_s)));
    EXPECT_EQ((QStringList{u"preferred-reader"_s, u"first-reader"_s}),
              readerLabels(loader.readersForFile(u"/tmp/test.m4b"_s)));
    EXPECT_EQ((QStringList{u"first-reader"_s, u"preferred-reader"_s}),
              readerLabels(loader.readersForFile(u"/tmp/test.mp3"_s)));
}

TEST_F(AudioLoaderTest, LoadsDecoderAndReaderForRegularTracks)
{
    AudioLoader loader;
    const QString filePath = createFile(u"song.mp3"_s, "regular-track");
    const Track track{filePath};

    const auto decoderOne     = addDecoder(loader, u"decoder-one"_s, {u"mp3"_s});
    decoderOne->initSucceeds  = false;
    decoderOne->requireDevice = true;

    const auto decoderTwo     = addDecoder(loader, u"decoder-two"_s, {u"mp3"_s});
    decoderTwo->requireDevice = true;

    const auto readerOne     = addReader(loader, u"reader-one"_s, {u"mp3"_s});
    readerOne->initSucceeds  = false;
    readerOne->requireDevice = true;

    const auto readerTwo     = addReader(loader, u"reader-two"_s, {u"mp3"_s});
    readerTwo->requireDevice = true;
    readerTwo->subsongCount  = 3;

    const auto loadedDecoder
        = loader.loadDecoderForTrack(track, AudioDecoder::UpdateTracks, AudioDecoder::RepeatTrackEnabled);
    ASSERT_NE(loadedDecoder.decoder, nullptr);
    EXPECT_EQ(u"decoder-two"_s, decoderLabel(loadedDecoder.decoder));
    ASSERT_TRUE(loadedDecoder.format.has_value());
    EXPECT_EQ(testFormat(), loadedDecoder.format.value());
    ASSERT_NE(loadedDecoder.input.device, nullptr);
    EXPECT_TRUE(loadedDecoder.input.device->isOpen());
    EXPECT_EQ(filePath, loadedDecoder.input.source.filepath);
    EXPECT_EQ(loadedDecoder.input.device.get(), loadedDecoder.input.source.device);
    EXPECT_EQ(1, decoderOne->initCalls);
    EXPECT_EQ(1, decoderTwo->initCalls);
    EXPECT_TRUE(decoderTwo->lastHadDevice);
    EXPECT_TRUE(decoderTwo->lastDeviceOpen);
    EXPECT_FALSE(decoderTwo->lastHadArchiveReader);
    EXPECT_EQ(AudioDecoder::UpdateTracks, decoderTwo->lastOptions);
    EXPECT_EQ(AudioDecoder::RepeatTrackEnabled, decoderTwo->playbackHintsAtInit);

    const auto loadedReader = loader.loadReaderForTrack(track);
    ASSERT_NE(loadedReader.reader, nullptr);
    EXPECT_EQ(u"reader-two"_s, readerLabel(loadedReader.reader));
    ASSERT_NE(loadedReader.input.device, nullptr);
    EXPECT_TRUE(loadedReader.input.device->isOpen());
    EXPECT_EQ(filePath, loadedReader.input.source.filepath);
    EXPECT_EQ(loadedReader.input.device.get(), loadedReader.input.source.device);
    EXPECT_EQ(1, readerOne->initCalls);
    EXPECT_EQ(1, readerTwo->initCalls);
    EXPECT_TRUE(readerTwo->lastHadDevice);
    EXPECT_TRUE(readerTwo->lastDeviceOpen);
    EXPECT_FALSE(readerTwo->lastHadArchiveReader);
    EXPECT_EQ(3, loadedReader.reader->subsongCount());
}

TEST_F(AudioLoaderTest, LoadsDecoderAndReaderForRemoteTracksWithoutOpeningDevices)
{
    AudioLoader loader;
    const Track track{u"https://radio.example.com/live"_s};

    const auto decoderOne             = addDecoder(loader, u"decoder-one"_s, {u"mp3"_s});
    decoderOne->initSucceeds          = false;
    decoderOne->supportsRemoteSources = false;

    const auto decoderTwo             = addDecoder(loader, u"decoder-two"_s, {u"flac"_s});
    decoderTwo->supportsRemoteSources = true;

    const auto readerOne             = addReader(loader, u"reader-one"_s, {u"ogg"_s});
    readerOne->initSucceeds          = false;
    readerOne->supportsRemoteSources = false;

    const auto readerTwo             = addReader(loader, u"reader-two"_s, {u"aac"_s});
    readerTwo->supportsRemoteSources = true;
    readerTwo->title                 = u"Remote Stream"_s;

    EXPECT_EQ((QStringList{u"decoder-two"_s}), decoderLabels(loader.decodersForTrack(track)));
    EXPECT_EQ((QStringList{u"reader-two"_s}), readerLabels(loader.readersForTrack(track)));

    const auto loadedDecoder = loader.loadDecoderForTrack(track);
    ASSERT_NE(loadedDecoder.decoder, nullptr);
    EXPECT_EQ(u"decoder-two"_s, decoderLabel(loadedDecoder.decoder));
    EXPECT_EQ(track.filepath(), loadedDecoder.input.source.filepath);
    EXPECT_EQ(nullptr, loadedDecoder.input.source.device);
    EXPECT_EQ(nullptr, loadedDecoder.input.device.get());
    EXPECT_EQ(0, decoderOne->initCalls);
    EXPECT_FALSE(decoderOne->lastHadDevice);
    EXPECT_FALSE(decoderTwo->lastHadDevice);
    EXPECT_FALSE(decoderTwo->lastDeviceOpen);

    const auto loadedReader = loader.loadReaderForTrack(track);
    ASSERT_NE(loadedReader.reader, nullptr);
    EXPECT_EQ(u"reader-two"_s, readerLabel(loadedReader.reader));
    EXPECT_EQ(track.filepath(), loadedReader.input.source.filepath);
    EXPECT_EQ(nullptr, loadedReader.input.source.device);
    EXPECT_EQ(nullptr, loadedReader.input.device.get());
    EXPECT_EQ(0, readerOne->initCalls);
    EXPECT_FALSE(readerOne->lastHadDevice);
    EXPECT_FALSE(readerTwo->lastHadDevice);
    EXPECT_FALSE(readerTwo->lastDeviceOpen);

    Track metadataTrack{track};
    ASSERT_TRUE(loader.readTrackMetadata(metadataTrack));
    EXPECT_EQ(u"Remote Stream"_s, metadataTrack.title());
    EXPECT_TRUE(metadataTrack.metadataWasRead());
    EXPECT_EQ(0, readerOne->readCalls);
    EXPECT_EQ(1, readerTwo->readCalls);
    EXPECT_FALSE(loader.canWriteMetadata(track));
    EXPECT_FALSE(loader.writeTrackMetadata(track, AudioReader::Metadata));
    EXPECT_FALSE(loader.writeTrackCover(track, {}, AudioReader::None));
}

TEST_F(AudioLoaderTest, LoadsRemoteTracksThroughSourceFactoryWhenConfigured)
{
    AudioLoader loader;
    const Track track{u"https://radio.example.com/live"_s};
    const auto provider = std::make_shared<FakeRemoteSourceProvider>();

    loader.setRemoteSourceProvider(provider);

    const auto decoder             = addDecoder(loader, u"remote-decoder"_s, {});
    decoder->supportsRemoteSources = true;
    decoder->requireDevice         = true;

    const auto reader             = addReader(loader, u"remote-reader"_s, {});
    reader->supportsRemoteSources = true;
    reader->requireDevice         = true;
    reader->title                 = u"Factory Stream"_s;

    const auto loadedDecoder = loader.loadDecoderForTrack(track);
    ASSERT_NE(loadedDecoder.decoder, nullptr);
    ASSERT_NE(loadedDecoder.input.device, nullptr);
    EXPECT_TRUE(loadedDecoder.input.device->isOpen());
    EXPECT_EQ(loadedDecoder.input.device.get(), loadedDecoder.input.source.device);
    EXPECT_EQ(loadedDecoder.input.source.remoteStreamDevice,
              dynamic_cast<Fooyin::RemoteStreamDevice*>(loadedDecoder.input.device.get()));
    EXPECT_TRUE(decoder->lastHadDevice);
    EXPECT_TRUE(decoder->lastHadRemoteStreamDevice);
    EXPECT_TRUE(decoder->lastDeviceOpen);

    const auto loadedReader = loader.loadReaderForTrack(track);
    ASSERT_NE(loadedReader.reader, nullptr);
    ASSERT_NE(loadedReader.input.device, nullptr);
    EXPECT_TRUE(loadedReader.input.device->isOpen());
    EXPECT_EQ(loadedReader.input.device.get(), loadedReader.input.source.device);
    EXPECT_EQ(loadedReader.input.source.remoteStreamDevice,
              dynamic_cast<Fooyin::RemoteStreamDevice*>(loadedReader.input.device.get()));
    EXPECT_TRUE(reader->lastHadDevice);
    EXPECT_TRUE(reader->lastHadRemoteStreamDevice);
    EXPECT_TRUE(reader->lastDeviceOpen);

    Track metadataTrack{track};
    ASSERT_TRUE(loader.readTrackMetadata(metadataTrack));
    EXPECT_EQ(u"Factory Stream"_s, metadataTrack.title());
    EXPECT_TRUE(reader->lastHadDevice);
    EXPECT_TRUE(reader->lastHadRemoteStreamDevice);
    EXPECT_TRUE(reader->lastDeviceOpen);
    EXPECT_EQ(3, provider->calls);

    reader->canReadCover = true;
    reader->coverData    = "remote-cover";
    EXPECT_TRUE(loader.readTrackCover(track, Track::Cover::Front).isEmpty());
    EXPECT_EQ(3, provider->calls);
    EXPECT_EQ(0, reader->readCoverCalls);
}

TEST_F(AudioLoaderTest, ReadsWritesMetadataAndCoversForRegularTracks)
{
    AudioLoader loader;
    const QString filePath = createFile(u"metadata.mp3"_s, "metadata-track");

    const auto firstReader     = addReader(loader, u"first-reader"_s, {u"mp3"_s});
    firstReader->requireDevice = true;
    firstReader->title         = u"failed"_s;
    firstReader->readSucceeds  = false;

    const auto secondReader        = addReader(loader, u"second-reader"_s, {u"mp3"_s});
    secondReader->requireDevice    = true;
    secondReader->canReadCover     = true;
    secondReader->canWriteMetadata = true;
    secondReader->canWriteCover    = true;
    secondReader->title            = u"loaded"_s;
    secondReader->coverData        = "cover-data";

    Track track{filePath};
    track.setTitle(u"original"_s);

    EXPECT_TRUE(loader.canWriteMetadata(track));

    ASSERT_TRUE(loader.readTrackMetadata(track));
    EXPECT_EQ(u"loaded"_s, track.title());
    EXPECT_TRUE(track.metadataWasRead());
    EXPECT_EQ(1, firstReader->readCalls);
    EXPECT_EQ(1, secondReader->readCalls);
    EXPECT_TRUE(secondReader->lastHadDevice);
    EXPECT_TRUE(secondReader->lastDeviceOpen);

    EXPECT_EQ(QByteArray{"cover-data"}, loader.readTrackCover(track, Track::Cover::Front));
    EXPECT_EQ(1, secondReader->readCoverCalls);
    EXPECT_EQ(Track::Cover::Front, secondReader->lastCover);

    EXPECT_TRUE(loader.writeTrackMetadata(track, AudioReader::Metadata | AudioReader::Rating));
    EXPECT_EQ(1, secondReader->writeTrackCalls);
    EXPECT_TRUE(secondReader->lastDeviceWritable);
    EXPECT_TRUE(secondReader->lastWriteOptions.testFlag(AudioReader::Metadata));
    EXPECT_TRUE(secondReader->lastWriteOptions.testFlag(AudioReader::Rating));

    TrackCovers covers;
    covers.emplace(Track::Cover::Front, CoverImage{u"image/png"_s, QByteArray{"updated-cover"}});
    EXPECT_TRUE(loader.writeTrackCover(track, covers, AudioReader::PreserveTimestamps));
    EXPECT_EQ(1, secondReader->writeCoverCalls);
    EXPECT_EQ(1U, secondReader->lastWrittenCoverCount);
    EXPECT_TRUE(secondReader->lastWriteOptions.testFlag(AudioReader::PreserveTimestamps));
}

TEST_F(AudioLoaderTest, LoadsDecoderAndReaderForArchiveTracks)
{
    AudioLoader loader;
    const QString archivePath = m_tempDir.filePath(u"library.zip"_s);
    const QString innerPath   = u"disc/song.mp3"_s;
    const Track track{makeArchiveTrackPath(archivePath, innerPath)};

    const auto archiveReader = addArchiveReader(loader, u"zip-reader"_s, {u"zip"_s});
    archiveReader->entries.emplace(innerPath,
                                   ArchiveEntrySpec{.path = innerPath, .data = "archive-data", .modifiedTime = 1234});

    const auto decoderOne            = addDecoder(loader, u"decoder-one"_s, {u"mp3"_s});
    decoderOne->initSucceeds         = false;
    decoderOne->requireDevice        = true;
    decoderOne->requireArchiveReader = true;

    const auto decoderTwo            = addDecoder(loader, u"decoder-two"_s, {u"mp3"_s});
    decoderTwo->requireDevice        = true;
    decoderTwo->requireArchiveReader = true;

    const auto readerOne            = addReader(loader, u"reader-one"_s, {u"mp3"_s});
    readerOne->initSucceeds         = false;
    readerOne->requireDevice        = true;
    readerOne->requireArchiveReader = true;

    const auto readerTwo            = addReader(loader, u"reader-two"_s, {u"mp3"_s});
    readerTwo->requireDevice        = true;
    readerTwo->requireArchiveReader = true;
    readerTwo->subsongCount         = 2;

    const auto loadedDecoder
        = loader.loadDecoderForArchiveTrack(track, AudioDecoder::NoSeeking, AudioDecoder::RepeatTrackEnabled);
    ASSERT_NE(loadedDecoder.decoder, nullptr);
    EXPECT_EQ(u"decoder-two"_s, decoderLabel(loadedDecoder.decoder));
    ASSERT_TRUE(loadedDecoder.format.has_value());
    EXPECT_EQ(testFormat(), loadedDecoder.format.value());
    ASSERT_NE(loadedDecoder.input.device, nullptr);
    EXPECT_TRUE(loadedDecoder.input.device->isOpen());
    EXPECT_EQ(innerPath, loadedDecoder.input.source.filepath);
    EXPECT_EQ(12U, loadedDecoder.input.source.size);
    EXPECT_EQ(1234U, loadedDecoder.input.source.modifiedTime);
    EXPECT_TRUE(decoderTwo->lastHadDevice);
    EXPECT_TRUE(decoderTwo->lastDeviceOpen);
    EXPECT_TRUE(decoderTwo->lastHadArchiveReader);
    EXPECT_EQ(12U, decoderTwo->lastSourceSize);
    EXPECT_EQ(1234U, decoderTwo->lastSourceModifiedTime);
    EXPECT_EQ(AudioDecoder::NoSeeking, decoderTwo->lastOptions);

    const auto loadedReader = loader.loadReaderForArchiveTrack(track);
    ASSERT_NE(loadedReader.reader, nullptr);
    EXPECT_EQ(u"reader-two"_s, readerLabel(loadedReader.reader));
    ASSERT_NE(loadedReader.input.device, nullptr);
    EXPECT_TRUE(loadedReader.input.device->isOpen());
    EXPECT_EQ(innerPath, loadedReader.input.source.filepath);
    EXPECT_EQ(12U, loadedReader.input.source.size);
    EXPECT_EQ(1234U, loadedReader.input.source.modifiedTime);
    EXPECT_TRUE(readerTwo->lastHadDevice);
    EXPECT_TRUE(readerTwo->lastDeviceOpen);
    EXPECT_TRUE(readerTwo->lastHadArchiveReader);
    EXPECT_EQ(2, loadedReader.reader->subsongCount());

    EXPECT_GE(archiveReader->initCalls, 2);
    EXPECT_GE(archiveReader->entryCalls, 2);
}

TEST_F(AudioLoaderTest, UsesArchiveWrapperReadersForArchiveMetadataAndRejectsArchiveWrites)
{
    AudioLoader loader;
    const QString archivePath = m_tempDir.filePath(u"collection.zip"_s);
    const QString innerPath   = u"disc/song.mp3"_s;
    const Track trackTemplate{makeArchiveTrackPath(archivePath, innerPath)};

    const auto wrapperReader        = addReader(loader, u"wrapper-reader"_s, {u"ignored"_s}, -1, true);
    wrapperReader->canReadCover     = true;
    wrapperReader->canWriteMetadata = true;
    wrapperReader->title            = u"archive-title"_s;
    wrapperReader->coverData        = "archive-cover";

    addArchiveReader(loader, u"zip-reader"_s, {u"zip"_s});

    EXPECT_TRUE(loader.canWriteMetadata(trackTemplate));

    Track metadataTrack{trackTemplate};
    ASSERT_TRUE(loader.readTrackMetadata(metadataTrack));
    EXPECT_EQ(u"archive-title"_s, metadataTrack.title());
    EXPECT_TRUE(metadataTrack.metadataWasRead());
    EXPECT_FALSE(wrapperReader->lastHadDevice);
    EXPECT_FALSE(wrapperReader->lastHadArchiveReader);

    EXPECT_EQ(QByteArray{"archive-cover"}, loader.readTrackCover(trackTemplate, Track::Cover::Front));
    EXPECT_EQ(Track::Cover::Front, wrapperReader->lastCover);
    EXPECT_FALSE(loader.writeTrackMetadata(trackTemplate, AudioReader::Metadata));

    TrackCovers covers;
    covers.emplace(Track::Cover::Front, CoverImage{u"image/png"_s, QByteArray{"cover"}});
    EXPECT_FALSE(loader.writeTrackCover(trackTemplate, covers, AudioReader::Metadata));
}

TEST_F(AudioLoaderTest, SavesAndRestoresStateAcrossInstances)
{
    AudioLoader loader;
    addDecoder(loader, u"decoder-one"_s, {u"mp3"_s});
    addDecoder(loader, u"decoder-two"_s, {u"mp3"_s});
    addReader(loader, u"reader-one"_s, {u"mp3"_s});
    addReader(loader, u"reader-two"_s, {u"mp3"_s});

    loader.setDecoderEnabled(u"decoder-one"_s, false);
    loader.changeDecoderIndex(u"decoder-two"_s, 0);
    loader.setReaderEnabled(u"reader-one"_s, false);
    loader.changeReaderIndex(u"reader-two"_s, 0);
    loader.saveState();

    {
        FySettings settings;
        settings.sync();
        EXPECT_TRUE(settings.contains(u"Engine/DecoderState"_s));
        EXPECT_TRUE(settings.contains(u"Engine/ReaderState"_s));
        EXPECT_FALSE(settings.value(u"Engine/DecoderState"_s).toByteArray().isEmpty());
        EXPECT_FALSE(settings.value(u"Engine/ReaderState"_s).toByteArray().isEmpty());
    }

    AudioLoader restoredLoader;
    addDecoder(restoredLoader, u"decoder-one"_s, {u"mp3"_s});
    addDecoder(restoredLoader, u"decoder-two"_s, {u"mp3"_s});
    addReader(restoredLoader, u"reader-one"_s, {u"mp3"_s});
    addReader(restoredLoader, u"reader-two"_s, {u"mp3"_s});

    restoredLoader.restoreState();

    EXPECT_EQ((QStringList{u"decoder-two"_s, u"decoder-one"_s}), entryNames(restoredLoader.decoders()));
    EXPECT_EQ((QStringList{u"reader-two"_s, u"reader-one"_s}), entryNames(restoredLoader.readers()));
    EXPECT_EQ((std::vector<uint8_t>{1, 0}), entryEnabledStates(restoredLoader.decoders()));
    EXPECT_EQ((std::vector<uint8_t>{1, 0}), entryEnabledStates(restoredLoader.readers()));
    EXPECT_EQ((QStringList{u"decoder-two"_s}), decoderLabels(restoredLoader.decodersForFile(u"/tmp/test.mp3"_s)));
    EXPECT_EQ((QStringList{u"reader-two"_s}), readerLabels(restoredLoader.readersForFile(u"/tmp/test.mp3"_s)));
}

TEST_F(AudioLoaderTest, ResetRestoresDefaultsAndClearsSavedState)
{
    AudioLoader loader;
    addDecoder(loader, u"decoder-one"_s, {u"mp3"_s});
    addDecoder(loader, u"decoder-two"_s, {u"mp3"_s});
    addReader(loader, u"reader-one"_s, {u"mp3"_s});
    addReader(loader, u"reader-two"_s, {u"mp3"_s});

    loader.setDecoderEnabled(u"decoder-one"_s, false);
    loader.changeDecoderIndex(u"decoder-two"_s, 0);
    loader.setReaderEnabled(u"reader-one"_s, false);
    loader.changeReaderIndex(u"reader-two"_s, 0);
    loader.saveState();

    loader.reset();

    EXPECT_EQ((QStringList{u"decoder-one"_s, u"decoder-two"_s}), entryNames(loader.decoders()));
    EXPECT_EQ((QStringList{u"reader-one"_s, u"reader-two"_s}), entryNames(loader.readers()));
    EXPECT_EQ((std::vector<uint8_t>{1, 1}), entryEnabledStates(loader.decoders()));
    EXPECT_EQ((std::vector<uint8_t>{1, 1}), entryEnabledStates(loader.readers()));

    AudioLoader restoredLoader;
    addDecoder(restoredLoader, u"decoder-one"_s, {u"mp3"_s});
    addDecoder(restoredLoader, u"decoder-two"_s, {u"mp3"_s});
    addReader(restoredLoader, u"reader-one"_s, {u"mp3"_s});
    addReader(restoredLoader, u"reader-two"_s, {u"mp3"_s});

    restoredLoader.restoreState();

    EXPECT_EQ((QStringList{u"decoder-one"_s, u"decoder-two"_s}), entryNames(restoredLoader.decoders()));
    EXPECT_EQ((QStringList{u"reader-one"_s, u"reader-two"_s}), entryNames(restoredLoader.readers()));
    EXPECT_EQ((std::vector<uint8_t>{1, 1}), entryEnabledStates(restoredLoader.decoders()));
    EXPECT_EQ((std::vector<uint8_t>{1, 1}), entryEnabledStates(restoredLoader.readers()));
}
} // namespace Fooyin::Testing
