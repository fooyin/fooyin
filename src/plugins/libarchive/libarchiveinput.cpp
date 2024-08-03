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
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMimeDatabase>

#include <archive_entry.h>

Q_LOGGING_CATEGORY(LIBARCH, "LibArchive")

namespace {
QStringList fileExtensions()
{
    static const QStringList extensions = {QStringLiteral("zip"), QStringLiteral("rar"), QStringLiteral("tar"),
                                           QStringLiteral("gz"), QStringLiteral("7z")};
    return extensions;
}

bool isImageFile(const QString& filePath)
{
    const QMimeDatabase mimeDatabase;
    const QMimeType mimeType = mimeDatabase.mimeTypeForFile(filePath);

    return mimeType.name().startsWith(u"image/");
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

bool LibArchiveReader::init(const QString& file)
{
    m_file = file;

    const ArchivePtr archive{archive_read_new()};

    archive_read_support_filter_all(archive.get());
    archive_read_support_format_all(archive.get());

    if(archive_read_open_filename(archive.get(), file.toUtf8().constData(), 10240) != ARCHIVE_OK) {
        qCWarning(LIBARCH) << "Unable to open" << file << ":" << archive_error_string(archive.get());
        return false;
    }

    archive_entry* entry{nullptr};

    while(archive_read_next_header(archive.get(), &entry) == ARCHIVE_OK) {
        if(archive_read_has_encrypted_entries(archive.get()) == 1) {
            return false;
        }

        if(archive_entry_filetype(entry) == AE_IFREG) {
            m_entries.emplace_back(QString::fromUtf8(archive_entry_pathname(entry)));
        }
    }

    m_type = QFileInfo{file}.suffix();

    return true;
}

QString LibArchiveReader::type() const
{
    return m_type;
}

QStringList LibArchiveReader::entryList() const
{
    return m_entries;
}

std::unique_ptr<QIODevice> LibArchiveReader::entry(const QString& file) const
{
    ArchivePtr archive{archive_read_new()};

    archive_read_support_filter_all(archive.get());
    archive_read_support_format_all(archive.get());

    if(archive_read_open_filename(archive.get(), m_file.toUtf8().constData(), 10240) != ARCHIVE_OK) {
        qCWarning(LIBARCH) << "Unable to open" << m_file << ":" << archive_error_string(archive.get());
        return nullptr;
    }

    archive_entry* entry{nullptr};

    while(archive_read_next_header(archive.get(), &entry) == ARCHIVE_OK) {
        if(archive_read_has_encrypted_entries(archive.get()) == 1) {
            return nullptr;
        }

        if(archive_entry_filetype(entry) == AE_IFREG) {
            const QString filepath = QString::fromUtf8(archive_entry_pathname(entry));
            if(filepath == file) {
                return std::make_unique<LibArchiveIODevice>(std::move(archive), entry, nullptr);
            }
        }
    }
    return nullptr;
}

QByteArray LibArchiveReader::readCover(const Track& /*track*/, Track::Cover /*cover*/)
{
    QByteArray coverData;

    for(const QString& file : std::as_const(m_entries)) {
        if(isImageFile(file)) {
            auto device = entry(file);
            if(device) {
                // Use first valid image
                coverData = device->readAll();
                break;
            }
        }
    }

    return coverData;
}
} // namespace Fooyin::LibArchive

#include "moc_libarchiveinput.cpp"
