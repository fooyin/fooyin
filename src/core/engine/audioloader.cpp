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

#include <algorithm>
#include <mutex>
#include <shared_mutex>

Q_LOGGING_CATEGORY(AUD_LDR, "fy.audioloader")

using namespace Qt::StringLiterals;

constexpr auto DecoderState = "Engine/DecoderState";
constexpr auto ReaderState  = "Engine/ReaderState";

namespace {
QStringList normaliseExtensions(QStringList extensions)
{
    for(QString& extension : extensions) {
        extension = extension.trimmed().toLower();
    }

    extensions.removeAll(QString{});
    std::ranges::sort(extensions);
    extensions.removeDuplicates();

    return extensions;
}

template <typename T>
void appendLoaderExtensions(QStringList& extensions, const std::vector<T>& loaders)
{
    for(const auto& loader : loaders) {
        extensions.append(loader.extensions);
    }
}

template <typename T>
QStringList archiveExtensionsFromReaders(const std::vector<T>& archiveReaders)
{
    QStringList extensions;
    appendLoaderExtensions(extensions, archiveReaders);
    return normaliseExtensions(std::move(extensions));
}

template <typename T>
void refreshArchiveWrapperExtensions(std::vector<T>& loaders, const QStringList& archiveExtensions)
{
    for(auto& loader : loaders) {
        if(loader.isArchiveWrapper) {
            loader.extensions = archiveExtensions;
        }
    }
}

template <typename T>
void sortLoaderEntries(std::vector<T>& entries)
{
    std::ranges::sort(entries, {}, &T::index);
    std::ranges::for_each(entries, [i = 0](auto& loader) mutable { loader.index = i++; });
}

template <typename EntryT, typename CreatorT>
CreatorT selectTrackIoCreator(const std::vector<EntryT>& loaders, const QString& ext, bool isInArchive)
{
    for(const auto& loader : loaders) {
        if(!loader.enabled) {
            continue;
        }
        if(isInArchive && loader.isArchiveWrapper) {
            return loader.creator;
        }
        if(!isInArchive && !loader.isArchiveWrapper && loader.extensions.contains(ext)) {
            return loader.creator;
        }
    }

    return {};
}
} // namespace

namespace Fooyin {
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
        if(data.isEmpty()) {
            return;
        }

        QDataStream stream{&data, QIODevice::ReadOnly};

        qsizetype size{0};
        stream >> size;
        if(stream.status() != QDataStream::Ok || size < 0) {
            return;
        }

        const auto maxEntries = static_cast<qsizetype>(loaders.size());
        size                  = std::min(size, maxEntries);

        while(size > 0) {
            --size;
            QString name;
            int index{0};
            bool enabled{true};

            stream >> name >> index >> enabled;
            if(stream.status() != QDataStream::Ok) {
                break;
            }

            auto loaderIt = std::ranges::find_if(loaders, [&name](const auto& loader) { return loader.name == name; });
            if(loaderIt != loaders.end()) {
                loaderIt->index   = index;
                loaderIt->enabled = enabled;
            }
        }

        sortLoaderEntries(loaders);
    };

    const std::unique_lock lock{p->m_mutex};
    const QStringList archiveExts = archiveExtensionsFromReaders(p->m_archiveReaders);

    refreshArchiveWrapperExtensions(p->m_decoders, archiveExts);
    refreshArchiveWrapperExtensions(p->m_readers, archiveExts);

    restoreLoaders(p->m_decoders, p->m_defaultDecoders, DecoderState);
    restoreLoaders(p->m_readers, p->m_defaultReaders, ReaderState);
}

AudioLoader::~AudioLoader() = default;

QStringList AudioLoader::supportedFileExtensions() const
{
    const std::shared_lock lock{p->m_mutex};

    QStringList extensions;
    appendLoaderExtensions(extensions, p->m_decoders);
    appendLoaderExtensions(extensions, p->m_readers);
    appendLoaderExtensions(extensions, p->m_archiveReaders);
    return normaliseExtensions(std::move(extensions));
}

QStringList AudioLoader::supportedTrackExtensions() const
{
    const std::shared_lock lock{p->m_mutex};

    QStringList extensions;
    appendLoaderExtensions(extensions, p->m_decoders);
    appendLoaderExtensions(extensions, p->m_readers);
    return normaliseExtensions(std::move(extensions));
}

QStringList AudioLoader::supportedArchiveExtensions() const
{
    const std::shared_lock lock{p->m_mutex};
    return archiveExtensionsFromReaders(p->m_archiveReaders);
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
    const QString ext      = QFileInfo{file}.suffix().toLower();
    const bool isInArchive = Track::isArchivePath(file);

    DecoderCreator creator;
    {
        const std::shared_lock lock{p->m_mutex};
        creator = selectTrackIoCreator<LoaderEntry<DecoderCreator>, DecoderCreator>(p->m_decoders, ext, isInArchive);
    }

    if(creator) {
        return creator();
    }

    return nullptr;
}

std::unique_ptr<AudioDecoder> AudioLoader::decoderForTrack(const Track& track) const
{
    return decoderForFile(track.filepath());
}

std::unique_ptr<AudioReader> AudioLoader::readerForFile(const QString& file) const
{
    const QString ext      = QFileInfo{file}.suffix().toLower();
    const bool isInArchive = Track::isArchivePath(file);

    ReaderCreator creator;
    {
        const std::shared_lock lock{p->m_mutex};
        creator = selectTrackIoCreator<LoaderEntry<ReaderCreator>, ReaderCreator>(p->m_readers, ext, isInArchive);
    }

    if(creator) {
        return creator();
    }

    return nullptr;
}

std::unique_ptr<AudioReader> AudioLoader::readerForTrack(const Track& track) const
{
    return readerForFile(track.filepath());
}

std::unique_ptr<ArchiveReader> AudioLoader::archiveReaderForFile(const QString& file) const
{
    if(!isArchive(file)) {
        return nullptr;
    }

    const QString ext = QFileInfo{file}.suffix().toLower();

    ArchiveReaderCreator creator;
    {
        const std::shared_lock lock{p->m_mutex};
        for(const auto& loader : p->m_archiveReaders) {
            if(!loader.enabled) {
                continue;
            }
            if(loader.extensions.contains(ext)) {
                creator = loader.creator;
                break;
            }
        }
    }

    if(creator) {
        return creator();
    }

    return nullptr;
}

bool AudioLoader::readTrackMetadata(Track& track) const
{
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

    auto reader = readerForTrack(track);
    if(!reader || !reader->canWriteMetaData()) {
        return false;
    }

    AudioSource source;
    source.filepath = track.filepath();
    QFile file{track.filepath()};

    if(!file.exists()) {
        qCWarning(AUD_LDR) << "File not found:" << source.filepath;
        return false;
    }

    if(!file.open(QIODeviceBase::ReadWrite)) {
        qCWarning(AUD_LDR) << "Failed to open file:" << source.filepath;
        return false;
    }
    source.device = &file;

    return reader->writeTrack(source, track, options);
}

bool AudioLoader::writeTrackCover(const Track& track, const TrackCovers& coverData,
                                  AudioReader::WriteOptions options) const
{
    if(track.isInArchive()) {
        return false;
    }

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

    return reader->writeCover(source, track, coverData, options);
}

void AudioLoader::addDecoder(const QString& name, const DecoderCreator& creator, int priority, bool isArchiveWrapper)
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
    if(!decoder) {
        qCWarning(AUD_LDR) << "Decoder" << name << "cannot be instantiated";
        return;
    }

    LoaderEntry<DecoderCreator> loader;
    loader.name             = name;
    loader.index            = priority >= 0 ? priority : static_cast<int>(p->m_decoders.size());
    loader.extensions       = isArchiveWrapper ? archiveExtensionsFromReaders(p->m_archiveReaders)
                                               : normaliseExtensions(decoder->extensions());
    loader.isArchiveWrapper = isArchiveWrapper;
    loader.creator          = creator;

    p->m_decoders.push_back(loader);
    sortLoaderEntries(p->m_decoders);

    p->m_defaultDecoders.push_back(loader);
    sortLoaderEntries(p->m_defaultDecoders);
}

void AudioLoader::addReader(const QString& name, const ReaderCreator& creator, int priority, bool isArchiveWrapper)
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
    if(!reader) {
        qCWarning(AUD_LDR) << "Reader" << name << "cannot be instantiated";
        return;
    }

    LoaderEntry<ReaderCreator> loader;
    loader.name             = name;
    loader.index            = priority >= 0 ? priority : static_cast<int>(p->m_readers.size());
    loader.extensions       = isArchiveWrapper ? archiveExtensionsFromReaders(p->m_archiveReaders)
                                               : normaliseExtensions(reader->extensions());
    loader.isArchiveWrapper = isArchiveWrapper;
    loader.creator          = creator;

    p->m_readers.push_back(loader);
    sortLoaderEntries(p->m_readers);

    p->m_defaultReaders.push_back(loader);
    sortLoaderEntries(p->m_defaultReaders);
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
    if(!reader) {
        qCWarning(AUD_LDR) << "Reader" << name << "cannot be instantiated";
        return;
    }

    LoaderEntry<ArchiveReaderCreator> loader;
    loader.name       = name;
    loader.index      = priority >= 0 ? priority : static_cast<int>(p->m_archiveReaders.size());
    loader.extensions = normaliseExtensions(reader->extensions());
    loader.creator    = creator;

    p->m_archiveReaders.push_back(loader);
    sortLoaderEntries(p->m_archiveReaders);

    const QStringList archiveExtensions = archiveExtensionsFromReaders(p->m_archiveReaders);
    refreshArchiveWrapperExtensions(p->m_decoders, archiveExtensions);
    refreshArchiveWrapperExtensions(p->m_readers, archiveExtensions);
    refreshArchiveWrapperExtensions(p->m_defaultDecoders, archiveExtensions);
    refreshArchiveWrapperExtensions(p->m_defaultReaders, archiveExtensions);
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
        const auto from = static_cast<int>(std::distance(p->m_decoders.begin(), decoder));
        Utils::move(p->m_decoders, from, index);
    }
    sortLoaderEntries(p->m_decoders);
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
        const auto from = static_cast<int>(std::distance(p->m_readers.begin(), reader));
        Utils::move(p->m_readers, from, index);
    }
    sortLoaderEntries(p->m_readers);
}

void AudioLoader::reloadDecoderExtensions(const QString& name)
{
    DecoderCreator creator;
    {
        const std::shared_lock lock{p->m_mutex};
        auto decoder = std::ranges::find_if(p->m_decoders, [&name](const auto& entry) { return entry.name == name; });
        if(decoder != p->m_decoders.end()) {
            creator = decoder->creator;
        }
    }

    if(!creator) {
        return;
    }

    auto instance = creator();
    if(!instance) {
        return;
    }

    const auto extensions = normaliseExtensions(instance->extensions());

    const std::unique_lock lock{p->m_mutex};
    auto decoder = std::ranges::find_if(p->m_decoders, [&name](const auto& entry) { return entry.name == name; });
    if(decoder != p->m_decoders.end()) {
        decoder->extensions = extensions;
    }
}

void AudioLoader::reloadReaderExtensions(const QString& name)
{
    ReaderCreator creator;
    {
        const std::shared_lock lock{p->m_mutex};
        auto reader = std::ranges::find_if(p->m_readers, [&name](const auto& entry) { return entry.name == name; });
        if(reader != p->m_readers.end()) {
            creator = reader->creator;
        }
    }

    if(!creator) {
        return;
    }

    auto instance = creator();
    if(!instance) {
        return;
    }

    const auto extensions = normaliseExtensions(instance->extensions());

    const std::unique_lock lock{p->m_mutex};
    auto reader = std::ranges::find_if(p->m_readers, [&name](const auto& entry) { return entry.name == name; });
    if(reader != p->m_readers.end()) {
        reader->extensions = extensions;
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
