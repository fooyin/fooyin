/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "libarchiveinput.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMimeDatabase>

#ifdef Q_OS_WIN
#define NOMINMAX
#endif
#include <archive_entry.h>

#include <array>

Q_LOGGING_CATEGORY(LIBARCH, "fy.libarchive")

using namespace Qt::StringLiterals;

constexpr qint64 ArchiveReadChunkSize    = 64LL * 1024;
constexpr qint64 KnownEntryMemoryLimit   = 128LL * 1024 * 1024;
constexpr qint64 UnknownEntryMemoryLimit = 32LL * 1024 * 1024;

namespace {
QStringList fileExtensions()
{
    static const QStringList extensions = {u"zip"_s, u"rar"_s, u"tar"_s, u"gz"_s, u"7z"_s, u"vgm7z"_s};
    return extensions;
}

bool isImageFile(const QString& filePath)
{
    const QMimeDatabase mimeDatabase;
    const QMimeType mimeType = mimeDatabase.mimeTypeForFile(filePath);

    return mimeType.name().startsWith("image/"_L1);
}

bool setupForReading(archive* archive, const QString& filename)
{
    archive_read_support_filter_all(archive);
    archive_read_support_format_all(archive);

    if(archive_read_open_filename(archive, QFile::encodeName(filename).constData(), 10240) != ARCHIVE_OK) {
        qCWarning(LIBARCH) << "Unable to open" << filename << ':' << archive_error_string(archive);
        return false;
    }

    return true;
}

bool archiveIterationFinished(const int result, archive* archive, const QString& filename)
{
    if(result == ARCHIVE_EOF) {
        return true;
    }

    qCWarning(LIBARCH) << "Reading" << filename << "failed:" << archive_error_string(archive);
    return false;
}

uint64_t entryModifiedTimeMs(archive_entry* entry)
{
    if(!entry || archive_entry_mtime_is_set(entry) == 0) {
        return 0;
    }

    const auto seconds = archive_entry_mtime(entry);
    if(seconds < 0) {
        return 0;
    }

    return static_cast<uint64_t>(seconds) * 1000ULL;
}

} // namespace

namespace Fooyin::LibArchive {
LibArchiveIODevice::LibArchiveIODevice(ArchivePtr archive, archive_entry* entry, QString archiveFile, QString entryPath,
                                       ArchiveReader::StopRequestedCallback stopRequested, QObject* parent)
    : QIODevice{parent}
    , m_archive{std::move(archive)}
    , m_entry{entry}
    , m_archiveFile{std::move(archiveFile)}
    , m_entryPath{std::move(entryPath)}
    , m_stopRequested{std::move(stopRequested)}
    , m_memoryLimit{UnknownEntryMemoryLimit}
    , m_failed{false}
{
    QIODevice::open(ReadOnly);
    m_buffer.open(ReadWrite);

    if(archive_entry_size_is_set(m_entry) != 0) {
        m_memoryLimit = KnownEntryMemoryLimit;
        if(LibArchiveIODevice::size() > KnownEntryMemoryLimit) {
            switchToTemporaryFile();
        }
    }
}

LibArchiveIODevice::~LibArchiveIODevice()
{
    m_archive.reset();
}

bool LibArchiveIODevice::seek(qint64 pos)
{
    const qint64 entrySize    = size();
    const bool entrySizeKnown = archive_entry_size_is_set(m_entry) != 0;
    if(!isOpen() || m_failed || pos < 0 || (entrySizeKnown && entrySize >= 0 && pos > entrySize)) {
        return false;
    }

    auto* buffer = bufferDevice();

    if(pos <= buffer->size()) {
        return buffer->seek(pos) && QIODevice::seek(pos);
    }

    qint64 bufferLen = pos - buffer->size();
    std::array<char, ArchiveReadChunkSize> tmpBuffer{};

    while(bufferLen > 0) {
        if(stopRequested()) {
            return false;
        }

        qint64 lenToRead = std::min(static_cast<qint64>(tmpBuffer.size()), bufferLen);

        lenToRead = archive_read_data(m_archive.get(), tmpBuffer.data(), lenToRead);
        if(lenToRead > 0) {
            if(!appendToBuffer(tmpBuffer.data(), lenToRead)) {
                return false;
            }
            bufferLen -= lenToRead;
            continue;
        }
        if(lenToRead < 0) {
            setArchiveError("Seeking");
        }
        else {
            m_failed = true;
            setErrorString(tr("Unexpected end of archive entry"));
            qCWarning(LIBARCH) << "Seeking in" << m_archiveFile << "entry" << m_entryPath << "failed:" << errorString();
        }
        return false;
    }

    buffer = bufferDevice();
    return buffer->seek(pos) && QIODevice::seek(pos);
}

qint64 LibArchiveIODevice::size() const
{
    return archive_entry_size(m_entry);
}

bool LibArchiveIODevice::failed() const
{
    return m_failed;
}

archive* LibArchiveIODevice::releaseArchive()
{
    return m_archive.release();
}

qint64 LibArchiveIODevice::readData(char* data, qint64 maxlen)
{
    if(!isOpen() || m_failed) {
        return -1;
    }
    if(maxlen <= 0) {
        return 0;
    }

    auto* buffer           = bufferDevice();
    const qint64 available = buffer->size() - buffer->pos();

    if(maxlen > available) {
        if(stopRequested()) {
            return -1;
        }

        const qint64 lenToRead = std::min(maxlen - available, ArchiveReadChunkSize);
        std::array<char, ArchiveReadChunkSize> tmpBuffer{};

        const auto read = archive_read_data(m_archive.get(), tmpBuffer.data(), lenToRead);
        if(read > 0) {
            if(!appendToBuffer(tmpBuffer.data(), read)) {
                return -1;
            }
        }
        else if(read < 0) {
            setArchiveError("Reading");
            return -1;
        }
    }

    return bufferDevice()->read(data, maxlen);
}

qint64 LibArchiveIODevice::writeData(const char* /*data*/, qint64 /*len*/)
{
    return -1;
}

bool LibArchiveIODevice::stopRequested()
{
    if(!m_stopRequested || !m_stopRequested()) {
        return false;
    }

    m_failed = true;
    setErrorString(tr("Operation cancelled"));
    return true;
}

bool LibArchiveIODevice::appendToBuffer(const char* data, const qint64 len)
{
    if(!m_tempFile && m_buffer.size() + len > m_memoryLimit) {
        if(!switchToTemporaryFile()) {
            return false;
        }
    }

    auto* buffer          = bufferDevice();
    const qint64 position = buffer->pos();
    if(!buffer->seek(buffer->size()) || buffer->write(data, len) != len || !buffer->seek(position)) {
        m_failed = true;
        setErrorString(buffer->errorString());
        qCWarning(LIBARCH) << "Buffering" << m_archiveFile << "entry" << m_entryPath << "failed:" << errorString();
        return false;
    }

    return true;
}

bool LibArchiveIODevice::switchToTemporaryFile()
{
    auto tempFile = std::make_unique<QTemporaryFile>();
    if(!tempFile->open() || tempFile->write(m_buffer.data()) != m_buffer.size() || !tempFile->seek(m_buffer.pos())) {
        m_failed = true;
        setErrorString(tempFile->errorString());
        qCWarning(LIBARCH) << "Buffering" << m_archiveFile << "entry" << m_entryPath << "failed:" << errorString();
        return false;
    }

    m_buffer.close();
    m_buffer.setData({});
    m_tempFile = std::move(tempFile);
    return true;
}

QIODevice* LibArchiveIODevice::bufferDevice()
{
    return m_tempFile ? qobject_cast<QIODevice*>(m_tempFile.get()) : &m_buffer;
}

void LibArchiveIODevice::setArchiveError(const char* operation)
{
    const char* archiveError = archive_error_string(m_archive.get());
    const QString error      = archiveError ? QString::fromLocal8Bit(archiveError) : tr("Unknown archive error");

    m_failed = true;
    setErrorString(error);
    qCWarning(LIBARCH) << operation << "in" << m_archiveFile << "entry" << m_entryPath << "failed:" << error;
}

QStringList LibArchiveReader::extensions() const
{
    return fileExtensions();
}

QString LibArchiveReader::type() const
{
    return m_type;
}

bool LibArchiveReader::init(const QString& file)
{
    m_file = file;
    m_type = QFileInfo{file}.suffix();
    return true;
}

ArchiveEntryData LibArchiveReader::entry(const QString& file)
{
    ArchivePtr archive{archive_read_new()};

    if(!setupForReading(archive.get(), m_file)) {
        return {};
    }

    archive_entry* entry{nullptr};
    int result{ARCHIVE_OK};

    while((result = archive_read_next_header(archive.get(), &entry)) == ARCHIVE_OK) {
        if(archive_read_has_encrypted_entries(archive.get()) == 1) {
            qCInfo(LIBARCH) << "Unable to read encrypted file" << m_file;
            return {};
        }

        if(archive_entry_filetype(entry) == AE_IFREG) {
            const QString entryPath = QDir::fromNativeSeparators(QFile::decodeName(archive_entry_pathname(entry)));
            if(entryPath == file) {
                const la_int64_t entrySize = archive_entry_size(entry);
                return {.info   = {.path          = entryPath,
                                   .modifiedTime  = entryModifiedTimeMs(entry),
                                   .size          = entrySize > 0 ? static_cast<uint64_t>(entrySize) : 0,
                                   .isRegularFile = true},
                        .device = std::make_unique<LibArchiveIODevice>(std::move(archive), entry, m_file, entryPath)};
            }
        }
    }

    if(archiveIterationFinished(result, archive.get(), m_file)) {
        qCDebug(LIBARCH) << "Unable to find" << file << "in" << m_file;
    }
    return {};
}

bool LibArchiveReader::copyEntryToDevice(const QString& file, QIODevice* device,
                                         const StopRequestedCallback& stopRequested)
{
    if(!device || !device->isWritable()) {
        return false;
    }

    const ArchivePtr archive{archive_read_new()};

    if(!setupForReading(archive.get(), m_file)) {
        return false;
    }

    archive_entry* entry{nullptr};
    int result{ARCHIVE_OK};

    const auto isStopRequested = [&stopRequested]() {
        return stopRequested && stopRequested();
    };

    while(!isStopRequested() && (result = archive_read_next_header(archive.get(), &entry)) == ARCHIVE_OK) {
        if(isStopRequested()) {
            return false;
        }
        if(archive_read_has_encrypted_entries(archive.get()) == 1) {
            qCInfo(LIBARCH) << "Unable to read encrypted file" << m_file;
            return false;
        }

        if(archive_entry_filetype(entry) != AE_IFREG) {
            continue;
        }

        const QString entryPath = QDir::fromNativeSeparators(QFile::decodeName(archive_entry_pathname(entry)));
        if(entryPath != file) {
            continue;
        }

        std::array<char, 64UL * 1024> buffer{};
        while(!isStopRequested()) {
            const la_ssize_t read = archive_read_data(archive.get(), buffer.data(), buffer.size());
            if(read == 0) {
                return true;
            }
            if(read < 0) {
                qCWarning(LIBARCH) << "Reading" << m_file << "entry" << entryPath
                                   << "failed:" << archive_error_string(archive.get());
                return false;
            }
            if(device->write(buffer.data(), read) != read) {
                qCWarning(LIBARCH) << "Writing" << m_file << "entry" << entryPath << "failed:" << device->errorString();
                return false;
            }
        }

        return false;
    }

    if(isStopRequested()) {
        return false;
    }
    if(archiveIterationFinished(result, archive.get(), m_file)) {
        qCDebug(LIBARCH) << "Unable to find" << file << "in" << m_file;
    }
    return false;
}

bool LibArchiveReader::readEntries(const ReadEntryInfoCallback& readEntry, const StopRequestedCallback& stopRequested)
{
    const ArchivePtr archive{archive_read_new()};

    if(!setupForReading(archive.get(), m_file)) {
        return false;
    }

    archive_entry* entry{nullptr};
    int result{ARCHIVE_OK};

    const auto isStopRequested = [&stopRequested]() {
        return stopRequested && stopRequested();
    };

    while(!isStopRequested() && (result = archive_read_next_header(archive.get(), &entry)) == ARCHIVE_OK) {
        if(isStopRequested()) {
            return false;
        }
        if(archive_read_has_encrypted_entries(archive.get()) == 1) {
            qCInfo(LIBARCH) << "Unable to read encrypted file" << m_file;
            return false;
        }

        const la_int64_t entrySize = archive_entry_size(entry);
        const ArchiveEntryInfo entryInfo{
            .path          = QDir::fromNativeSeparators(QFile::decodeName(archive_entry_pathname(entry))),
            .modifiedTime  = entryModifiedTimeMs(entry),
            .size          = entrySize > 0 ? static_cast<uint64_t>(entrySize) : 0,
            .isRegularFile = archive_entry_filetype(entry) == AE_IFREG,
        };

        if(readEntry && !readEntry(entryInfo)) {
            return true;
        }
    }

    if(isStopRequested()) {
        return false;
    }
    return archiveIterationFinished(result, archive.get(), m_file);
}

bool LibArchiveReader::readTracks(ReadEntryCallback readEntry, const StopRequestedCallback& stopRequested)
{
    ArchivePtr archive{archive_read_new()};

    if(!setupForReading(archive.get(), m_file)) {
        return false;
    }

    archive_entry* entry{nullptr};
    int result{ARCHIVE_OK};

    const auto isStopRequested = [&stopRequested]() {
        return stopRequested && stopRequested();
    };

    while(!isStopRequested() && (result = archive_read_next_header(archive.get(), &entry)) == ARCHIVE_OK) {
        if(isStopRequested()) {
            return false;
        }
        if(archive_read_has_encrypted_entries(archive.get()) == 1) {
            qCInfo(LIBARCH) << "Unable to read encrypted file" << m_file;
            return false;
        }

        if(archive_entry_filetype(entry) == AE_IFREG) {
            const la_int64_t entrySize = archive_entry_size(entry);
            const QString entryPath    = QDir::fromNativeSeparators(QFile::decodeName(archive_entry_pathname(entry)));
            ArchiveEntryData entryData{.info   = {.path          = entryPath,
                                                  .modifiedTime  = entryModifiedTimeMs(entry),
                                                  .size          = entrySize > 0 ? static_cast<uint64_t>(entrySize) : 0,
                                                  .isRegularFile = true},
                                       .device = std::make_unique<LibArchiveIODevice>(std::move(archive), entry, m_file,
                                                                                      entryPath, stopRequested)};
            auto* archiveDevice = static_cast<LibArchiveIODevice*>(entryData.device.get());
            readEntry(std::move(entryData));

            if(archiveDevice->failed() || isStopRequested()) {
                return false;
            }
            archive.reset(archiveDevice->releaseArchive());
        }
    }

    if(isStopRequested()) {
        return false;
    }
    return archiveIterationFinished(result, archive.get(), m_file);
}

QByteArray LibArchiveReader::readCover(const Track& track, Track::Cover cover)
{
    if(cover != Track::Cover::Front) {
        // Only read front cover for now
        return {};
    }

    ArchivePtr archive{archive_read_new()};

    if(!setupForReading(archive.get(), m_file)) {
        return {};
    }

    QByteArray coverData;

    archive_entry* entry{nullptr};
    int result{ARCHIVE_OK};
    bool entryFound{false};

    while((result = archive_read_next_header(archive.get(), &entry)) == ARCHIVE_OK) {
        if(archive_read_has_encrypted_entries(archive.get()) == 1) {
            qCInfo(LIBARCH) << "Unable to read encrypted file" << m_file;
            return {};
        }

        if(archive_entry_filetype(entry) == AE_IFREG) {
            const QString entryPath = QDir::fromNativeSeparators(QFile::decodeName(archive_entry_pathname(entry)));

            if(isImageFile(entryPath)) {
                const QFileInfo info{entryPath};
                if(info.path() == track.relativeArchivePath()) {
                    auto entryDev = std::make_unique<LibArchiveIODevice>(std::move(archive), entry, m_file, entryPath);
                    if(entryDev) {
                        // Use first valid image
                        coverData  = entryDev->readAll();
                        entryFound = true;
                        break;
                    }
                }
            }
        }
    }

    if(!entryFound) {
        archiveIterationFinished(result, archive.get(), m_file);
    }

    return coverData;
}
} // namespace Fooyin::LibArchive

#include "moc_libarchiveinput.cpp"
