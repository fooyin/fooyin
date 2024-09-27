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

#include "libarchiveinput.h"

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMimeDatabase>

#include <archive_entry.h>

Q_LOGGING_CATEGORY(LIBARCH, "fy.libarchive")

namespace {
QStringList fileExtensions()
{
    static const QStringList extensions = {QStringLiteral("zip"), QStringLiteral("rar"), QStringLiteral("tar"),
                                           QStringLiteral("gz"),  QStringLiteral("7z"),  QStringLiteral("vgm7z")};
    return extensions;
}

bool isImageFile(const QString& filePath)
{
    const QMimeDatabase mimeDatabase;
    const QMimeType mimeType = mimeDatabase.mimeTypeForFile(filePath);

    return mimeType.name().startsWith(u"image/");
}

bool setupForReading(archive* archive, const QString& filename)
{
    archive_read_support_filter_all(archive);
    archive_read_support_format_all(archive);

    if(archive_read_open_filename(archive, QFile::encodeName(filename).constData(), 10240) != ARCHIVE_OK) {
        qCWarning(LIBARCH) << "Unable to open archive:" << archive_error_string(archive);
        qCWarning(LIBARCH) << "Archive corrupted or insufficient permissions";
        return false;
    }

    return true;
}
} // namespace

namespace Fooyin::LibArchive {
LibArchiveIODevice::LibArchiveIODevice(ArchivePtr archive, archive_entry* entry, QObject* parent)
    : QIODevice{parent}
    , m_archive{std::move(archive)}
    , m_entry{entry}
{
    open(QIODevice::ReadOnly);
    m_buffer.open(QBuffer::ReadWrite);
}

LibArchiveIODevice::~LibArchiveIODevice()
{
    m_archive.reset();
}

bool LibArchiveIODevice::seek(qint64 pos)
{
    if(!isOpen()) {
        return false;
    }

    QIODevice::seek(pos);

    if(pos <= m_buffer.size()) {
        return m_buffer.seek(pos);
    }

    qint64 bufferLen = pos - m_buffer.size();
    std::vector<char> tmpBuffer(1024);

    while(bufferLen > 0) {
        qint64 lenToRead = std::min(static_cast<qint64>(tmpBuffer.size()), bufferLen);

        lenToRead = archive_read_data(m_archive.get(), tmpBuffer.data(), static_cast<size_t>(lenToRead));
        if(lenToRead > 0) {
            m_buffer.buffer().append(tmpBuffer.data(), static_cast<qsizetype>(lenToRead));
            bufferLen -= lenToRead;
            continue;
        }
        if(lenToRead < 0) {
            qCWarning(LIBARCH) << "Seeking failed:" << archive_error_string(m_archive.get());
            setErrorString(QString::fromLocal8Bit(archive_error_string(m_archive.get())));
            close();
        }
        return false;
    }

    return m_buffer.seek(pos);
}

qint64 LibArchiveIODevice::size() const
{
    return archive_entry_size(m_entry);
}

archive* LibArchiveIODevice::releaseArchive()
{
    return m_archive.release();
}

qint64 LibArchiveIODevice::readData(char* data, qint64 maxlen)
{
    if(!isOpen()) {
        return -1;
    }

    if(m_buffer.pos() + maxlen > m_buffer.size()) {
        const qint64 lenToRead = m_buffer.pos() + maxlen - m_buffer.size();
        std::vector<char> tmpBuffer(lenToRead);

        const auto read = archive_read_data(m_archive.get(), tmpBuffer.data(), lenToRead);
        if(read > 0) {
            m_buffer.buffer().append(tmpBuffer.data(), read);
        }
        else if(read < 0) {
            qCWarning(LIBARCH) << "Reading failed:" << archive_error_string(m_archive.get());
            setErrorString(QString::fromLocal8Bit(archive_error_string(m_archive.get())));
            return -1;
        }
    }

    return m_buffer.read(data, maxlen);
}

qint64 LibArchiveIODevice::writeData(const char* /*data*/, qint64 /*len*/)
{
    return -1;
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

std::unique_ptr<QIODevice> LibArchiveReader::entry(const QString& file)
{
    ArchivePtr archive{archive_read_new()};

    if(!setupForReading(archive.get(), m_file)) {
        return nullptr;
    }

    archive_entry* entry{nullptr};

    while(archive_read_next_header(archive.get(), &entry) == ARCHIVE_OK) {
        if(archive_read_has_encrypted_entries(archive.get()) == 1) {
            qCInfo(LIBARCH) << "Unable to read encrypted file" << m_file;
            return nullptr;
        }

        if(archive_entry_filetype(entry) == AE_IFREG) {
            const QString entryPath = QDir::fromNativeSeparators(QFile::decodeName(archive_entry_pathname(entry)));
            if(entryPath == file) {
                return std::make_unique<LibArchiveIODevice>(std::move(archive), entry, nullptr);
            }
        }
    }

    qCDebug(LIBARCH) << "Unable to find" << file << "in" << m_file;
    return nullptr;
}

bool LibArchiveReader::readTracks(ReadEntryCallback readEntry)
{
    ArchivePtr archive{archive_read_new()};

    if(!setupForReading(archive.get(), m_file)) {
        return false;
    }

    archive_entry* entry{nullptr};

    while(archive_read_next_header(archive.get(), &entry) == ARCHIVE_OK) {
        if(archive_read_has_encrypted_entries(archive.get()) == 1) {
            qCInfo(LIBARCH) << "Unable to read encrypted file" << m_file;
            return false;
        }

        if(archive_entry_filetype(entry) == AE_IFREG) {
            const QString entryPath = QDir::fromNativeSeparators(QFile::decodeName(archive_entry_pathname(entry)));
            auto entryDev           = std::make_unique<LibArchiveIODevice>(std::move(archive), entry, nullptr);

            readEntry(entryPath, entryDev.get());
            archive.reset(entryDev->releaseArchive());
        }
    }

    return true;
}

QByteArray LibArchiveReader::readCover(const Track& track, Track::Cover /*cover*/)
{
    ArchivePtr archive{archive_read_new()};

    if(!setupForReading(archive.get(), m_file)) {
        return {};
    }

    QByteArray coverData;

    archive_entry* entry{nullptr};

    while(archive_read_next_header(archive.get(), &entry) == ARCHIVE_OK) {
        if(archive_read_has_encrypted_entries(archive.get()) == 1) {
            qCInfo(LIBARCH) << "Unable to read encrypted file" << m_file;
            return {};
        }

        if(archive_entry_filetype(entry) == AE_IFREG) {
            const QString entryPath = QDir::fromNativeSeparators(QFile::decodeName(archive_entry_pathname(entry)));

            if(isImageFile(entryPath)) {
                const QFileInfo info{entryPath};
                if(info.path() == track.relativeArchivePath()) {
                    auto entryDev = std::make_unique<LibArchiveIODevice>(std::move(archive), entry, nullptr);
                    if(entryDev) {
                        // Use first valid image
                        coverData = entryDev->readAll();
                        break;
                    }
                }
            }
        }
    }

    return coverData;
}
} // namespace Fooyin::LibArchive

#include "moc_libarchiveinput.cpp"
