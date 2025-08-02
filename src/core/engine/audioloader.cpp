/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <core/engine/audioloader.h>

#include <core/coresettings.h>
#include <core/track.h>
#include <utils/helpers.h>

#include <QFileInfo>
#include <QLoggingCategory>
#include <QThreadStorage>

#include <mutex>
#include <shared_mutex>

Q_LOGGING_CATEGORY(AUD_LDR, "fy.audioloader")

using namespace Qt::StringLiterals;

constexpr auto DecoderState = "Engine/DecoderState";
constexpr auto ReaderState  = "Engine/ReaderState";

namespace {
template <typename T>
void sortLoaderEntries(std::vector<T>& entries)
{
    std::ranges::sort(entries, {}, &T::index);
    std::ranges::for_each(entries, [i = 0](auto& loader) mutable { loader.index = i++; });
}
} // namespace

namespace Fooyin {
using DecoderInstances       = std::unordered_map<QString, std::unique_ptr<AudioDecoder>>;
using ReaderInstances        = std::unordered_map<QString, std::unique_ptr<AudioReader>>;
using ArchiveReaderInstances = std::unordered_map<QString, std::unique_ptr<ArchiveReader>>;

class AudioLoaderPrivate
{
public:
    std::vector<AudioLoader::LoaderEntry<DecoderCreator>> m_defaultDecoders;
    std::vector<AudioLoader::LoaderEntry<ReaderCreator>> m_defaultReaders;

    std::vector<AudioLoader::LoaderEntry<DecoderCreator>> m_decoders;
    std::vector<AudioLoader::LoaderEntry<ReaderCreator>> m_readers;
    std::vector<AudioLoader::LoaderEntry<ArchiveReaderCreator>> m_archiveReaders;

    std::shared_mutex m_mutex;
};

AudioLoader::AudioLoader()
    : p{std::make_unique<AudioLoaderPrivate>()}
{ }

void AudioLoader::saveState()
{
    FySettings settings;

    auto saveLoaders = [&settings](auto& loaders, const char* state) {
        QByteArray data;
        QDataStream stream{&data, QIODevice::WriteOnly};

        stream << static_cast<qsizetype>(loaders.size());
        for(const auto& loader : loaders) {
            stream << loader.name << loader.index << loader.enabled;
        }

        settings.setValue(state, data);
    };

    const std::shared_lock lock{p->m_mutex};
    saveLoaders(p->m_decoders, DecoderState);
    saveLoaders(p->m_readers, ReaderState);
}

void AudioLoader::restoreState()
{
    const FySettings settings;

    auto restoreLoaders = [&settings](auto& loaders, auto& defaultLoaders, const char* state) {
        defaultLoaders = loaders;
        sortLoaderEntries(defaultLoaders);

        QByteArray data = settings.value(state, {}).toByteArray();
        QDataStream stream{&data, QIODevice::ReadOnly};

        qsizetype size{0};
        stream >> size;

        while(size > 0) {
            --size;
            QString name;
            int index{0};
            bool enabled{true};

            stream >> name >> index >> enabled;

            auto loaderIt = std::ranges::find_if(loaders, [&name](const auto& loader) { return loader.name == name; });
            if(loaderIt != loaders.end()) {
                loaderIt->index   = index;
                loaderIt->enabled = enabled;
            }
        }

        sortLoaderEntries(loaders);
    };

    const QStringList archiveExts = supportedArchiveExtensions();
    const std::unique_lock lock{p->m_mutex};

    auto archiveDec
        = std::ranges::find_if(p->m_decoders, [](const auto& loader) { return loader.name == "Archive"_L1; });
    if(archiveDec != p->m_decoders.end()) {
        archiveDec->extensions = archiveExts;
    }
    auto archiveRead
        = std::ranges::find_if(p->m_readers, [](const auto& loader) { return loader.name == "Archive"_L1; });
    if(archiveRead != p->m_readers.end()) {
        archiveRead->extensions = archiveExts;
    }

    restoreLoaders(p->m_decoders, p->m_defaultDecoders, DecoderState);
    restoreLoaders(p->m_readers, p->m_defaultReaders, ReaderState);
}

AudioLoader::~AudioLoader() = default;

QStringList AudioLoader::supportedFileExtensions() const
{
    const std::shared_lock lock{p->m_mutex};

    QStringList extensions;

    auto addExtensions = [&extensions](const auto& loaders) {
        for(const auto& loader : loaders) {
            extensions.append(loader.extensions);
        }
    };

    addExtensions(p->m_decoders);
    addExtensions(p->m_readers);
    addExtensions(p->m_archiveReaders);

    return extensions;
}

QStringList AudioLoader::supportedTrackExtensions() const
{
    const std::shared_lock lock{p->m_mutex};

    QStringList extensions;

    auto addExtensions = [&extensions](const auto& loaders) {
        for(const auto& loader : loaders) {
            extensions.append(loader.extensions);
        }
    };

    addExtensions(p->m_decoders);
    addExtensions(p->m_readers);

    return extensions;
}

QStringList AudioLoader::supportedArchiveExtensions() const
{
    const std::shared_lock lock{p->m_mutex};

    QStringList extensions;

    for(const auto& loader : p->m_archiveReaders) {
        extensions.append(loader.extensions);
    }

    return extensions;
}

bool AudioLoader::canWriteMetadata(const Track& track) const
{
    if(auto reader = readerForTrack(track)) {
        if(track.isInArchive()) {
            reader->init({track.filepath(), nullptr, nullptr});
        }
        return reader->canWriteMetaData();
    }
    return false;
}

bool AudioLoader::isArchive(const QString& file) const
{
    const QString ext = QFileInfo{file}.suffix().toLower();

    const std::shared_lock lock{p->m_mutex};
    return std::ranges::any_of(p->m_archiveReaders,
                               [&ext](const auto& loader) { return loader.extensions.contains(ext); });
}

std::unique_ptr<AudioDecoder> AudioLoader::decoderForFile(const QString& file) const
{
    const std::shared_lock lock{p->m_mutex};

    const QString ext      = QFileInfo{file}.suffix().toLower();
    const bool isInArchive = Track::isArchivePath(file);

    for(const auto& loader : p->m_decoders) {
        if(!loader.enabled) {
            continue;
        }
        if((isInArchive && loader.name == "Archive"_L1) || (!isInArchive && loader.extensions.contains(ext))) {
            return loader.creator();
        }
    }

    return nullptr;
}

std::unique_ptr<AudioDecoder> AudioLoader::decoderForTrack(const Track& track) const
{
    return decoderForFile(track.filepath());
}

std::unique_ptr<AudioReader> AudioLoader::readerForFile(const QString& file) const
{
    const std::shared_lock lock{p->m_mutex};

    const QString ext      = QFileInfo{file}.suffix().toLower();
    const bool isInArchive = Track::isArchivePath(file);

    for(const auto& loader : p->m_readers) {
        if(!loader.enabled) {
            continue;
        }
        if((isInArchive && loader.name == "Archive"_L1) || (!isInArchive && loader.extensions.contains(ext))) {
            return loader.creator();
        }
    }

    return nullptr;
}

std::unique_ptr<AudioReader> AudioLoader::readerForTrack(const Track& track) const
{
    return readerForFile(track.filepath());
}

std::unique_ptr<ArchiveReader> AudioLoader::archiveReaderForFile(const QString& file) const
{
    const std::shared_lock lock{p->m_mutex};

    if(!isArchive(file)) {
        return nullptr;
    }

    const QString ext = QFileInfo{file}.suffix().toLower();

    for(const auto& loader : p->m_archiveReaders) {
        if(!loader.enabled) {
            continue;
        }
        if(loader.extensions.contains(ext)) {
            return loader.creator();
        }
    }

    return nullptr;
}

bool AudioLoader::readTrackMetadata(Track& track) const
{
    const std::shared_lock lock{p->m_mutex};

    auto reader = readerForTrack(track);
    if(!reader) {
        qCInfo(AUD_LDR) << "Tag reader not available for file:" << track.filepath();
        return {};
    }

    AudioSource source;
    source.filepath = track.filepath();
    QFile file{track.filepath()};
    if(!track.isInArchive()) {
        if(!file.open(QIODevice::ReadOnly)) {
            qCWarning(AUD_LDR) << "Failed to open file:" << source.filepath;
            return false;
        }
        source.device = &file;
    }

    if(reader->init(source)) {
        return reader->readTrack(source, track);
    }

    return false;
}

QByteArray AudioLoader::readTrackCover(const Track& track, Track::Cover cover) const
{
    const std::shared_lock lock{p->m_mutex};

    auto reader = readerForTrack(track);
    if(!reader) {
        return {};
    }

    if(!track.isInArchive() && !reader->canReadCover()) {
        return {};
    }

    AudioSource source;
    source.filepath = track.filepath();
    QFile file{track.filepath()};
    if(!track.isInArchive()) {
        if(!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        source.device = &file;
    }

    if(reader->init(source) && reader->canReadCover()) {
        return reader->readCover(source, track, cover);
    }

    return {};
}

bool AudioLoader::writeTrackMetadata(const Track& track, AudioReader::WriteOptions options) const
{
    if(track.isInArchive()) {
        return false;
    }

    const std::shared_lock lock{p->m_mutex};

    auto reader = readerForTrack(track);
    if(!reader || !reader->canWriteMetaData()) {
        return false;
    }

    AudioSource source;
    source.filepath = track.filepath();
    QFile file{track.filepath()};
    if(!file.open(QIODeviceBase::ReadWrite)) {
        qCWarning(AUD_LDR) << "Failed to open file:" << source.filepath;
        return false;
    }
    source.device = &file;

    return reader->writeTrack(source, track, options);
}

bool AudioLoader::writeTrackCover(const Track& track, const TrackCovers& coverData) const
{
    if(track.isInArchive()) {
        return false;
    }

    const std::shared_lock lock{p->m_mutex};

    auto reader = readerForTrack(track);
    if(!reader || !reader->canWriteCover()) {
        return false;
    }

    AudioSource source;
    source.filepath = track.filepath();
    QFile file{track.filepath()};
    if(!file.open(QIODeviceBase::ReadWrite)) {
        qCWarning(AUD_LDR) << "Failed to open file:" << source.filepath;
        return false;
    }
    source.device = &file;

    return reader->writeCover(source, track, coverData);
}

void AudioLoader::addDecoder(const QString& name, const DecoderCreator& creator, int priority)
{
    if(!creator) {
        qCWarning(AUD_LDR) << "Decoder" << name << "cannot be created";
        return;
    }

    const std::unique_lock lock{p->m_mutex};

    if(std::ranges::any_of(p->m_decoders, [&name](const auto& loader) { return loader.name == name; })) {
        qCWarning(AUD_LDR) << "Decoder" << name << "already registered";
        return;
    }

    auto decoder = creator();

    LoaderEntry<DecoderCreator> loader;
    loader.name       = name;
    loader.index      = priority >= 0 ? priority : static_cast<int>(p->m_decoders.size());
    loader.extensions = decoder->extensions();
    loader.creator    = creator;

    p->m_decoders.push_back(loader);
}

void AudioLoader::addReader(const QString& name, const ReaderCreator& creator, int priority)
{
    if(!creator) {
        qCWarning(AUD_LDR) << "Reader" << name << "cannot be created";
        return;
    }

    const std::unique_lock lock{p->m_mutex};

    if(std::ranges::any_of(p->m_readers, [&name](const auto& loader) { return loader.name == name; })) {
        qCWarning(AUD_LDR) << "Reader" << name << "already registered";
        return;
    }

    auto reader = creator();

    LoaderEntry<ReaderCreator> loader;
    loader.name       = name;
    loader.index      = priority >= 0 ? priority : static_cast<int>(p->m_readers.size());
    loader.extensions = reader->extensions();
    loader.creator    = creator;

    p->m_readers.push_back(loader);
}

void AudioLoader::addArchiveReader(const QString& name, const ArchiveReaderCreator& creator, int priority)
{
    if(!creator) {
        qCWarning(AUD_LDR) << "Reader" << name << "cannot be created";
        return;
    }

    const std::unique_lock lock{p->m_mutex};

    if(std::ranges::any_of(p->m_archiveReaders, [&name](const auto& loader) { return loader.name == name; })) {
        qCWarning(AUD_LDR) << "Reader" << name << "already registered";
        return;
    }

    auto reader = creator();

    LoaderEntry<ArchiveReaderCreator> loader;
    loader.name       = name;
    loader.index      = priority >= 0 ? priority : static_cast<int>(p->m_archiveReaders.size());
    loader.extensions = reader->extensions();
    loader.creator    = creator;

    p->m_archiveReaders.push_back(loader);
}

std::vector<AudioLoader::LoaderEntry<DecoderCreator>> AudioLoader::decoders() const
{
    const std::shared_lock lock{p->m_mutex};
    return p->m_decoders;
}

std::vector<AudioLoader::LoaderEntry<ReaderCreator>> AudioLoader::readers() const
{
    const std::shared_lock lock{p->m_mutex};
    return p->m_readers;
}

std::vector<AudioLoader::LoaderEntry<ArchiveReaderCreator>> AudioLoader::archiveReaders() const
{
    const std::shared_lock lock{p->m_mutex};
    return p->m_archiveReaders;
}

void AudioLoader::setDecoderEnabled(const QString& name, bool enabled)
{
    const std::unique_lock lock{p->m_mutex};
    auto decoder = std::ranges::find_if(p->m_decoders, [&name](const auto& entry) { return entry.name == name; });
    if(decoder != p->m_decoders.end()) {
        decoder->enabled = enabled;
    }
}

void AudioLoader::changeDecoderIndex(const QString& name, int index)
{
    const std::unique_lock lock{p->m_mutex};
    auto decoder = std::ranges::find_if(p->m_decoders, [&name](const auto& entry) { return entry.name == name; });
    if(decoder != p->m_decoders.end()) {
        Utils::move(p->m_decoders, decoder->index, index);
    }
    std::ranges::for_each(p->m_decoders, [i = 0](auto& loader) mutable { loader.index = i++; });
}

void AudioLoader::setReaderEnabled(const QString& name, bool enabled)
{
    const std::unique_lock lock{p->m_mutex};
    auto reader = std::ranges::find_if(p->m_readers, [&name](const auto& entry) { return entry.name == name; });
    if(reader != p->m_readers.end()) {
        reader->enabled = enabled;
    }
}

void AudioLoader::changeReaderIndex(const QString& name, int index)
{
    const std::unique_lock lock{p->m_mutex};
    auto reader = std::ranges::find_if(p->m_readers, [&name](const auto& entry) { return entry.name == name; });
    if(reader != p->m_readers.end()) {
        Utils::move(p->m_readers, reader->index, index);
    }
    std::ranges::for_each(p->m_readers, [i = 0](auto& loader) mutable { loader.index = i++; });
}

void AudioLoader::reloadDecoderExtensions(const QString& name)
{
    const std::unique_lock lock{p->m_mutex};
    auto decoder = std::ranges::find_if(p->m_decoders, [&name](const auto& entry) { return entry.name == name; });
    if(decoder != p->m_decoders.end()) {
        if(auto instance = decoder->creator()) {
            decoder->extensions = instance->extensions();
        }
    }
}

void AudioLoader::reloadReaderExtensions(const QString& name)
{
    const std::unique_lock lock{p->m_mutex};
    auto reader = std::ranges::find_if(p->m_readers, [&name](const auto& entry) { return entry.name == name; });
    if(reader != p->m_readers.end()) {
        if(auto instance = reader->creator()) {
            reader->extensions = instance->extensions();
        }
    }
}

void AudioLoader::reset()
{
    FySettings settings;
    settings.remove(QLatin1String{DecoderState});
    settings.remove(QLatin1String{ReaderState});

    const std::unique_lock lock{p->m_mutex};
    p->m_decoders = p->m_defaultDecoders;
    p->m_readers  = p->m_defaultReaders;
}
} // namespace Fooyin
