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

#pragma once

#include <core/engine/audioinput.h>
#include <core/engine/audioloader.h>

#include <QBuffer>
#include <QTemporaryFile>

#include <archive.h>

struct archive;
struct archive_entry;

namespace Fooyin::LibArchive {
struct ArchiveDeleter
{
    void operator()(archive* archive) const noexcept
    {
        if(archive) {
            archive_read_close(archive);
            archive_read_free(archive);
        }
    }
};
using ArchivePtr = std::unique_ptr<archive, ArchiveDeleter>;

class LibArchiveIODevice : public QIODevice
{
    Q_OBJECT

public:
    LibArchiveIODevice(ArchivePtr archive, archive_entry* entry, QString archiveFile = {}, QString entryPath = {},
                       ArchiveReader::StopRequestedCallback stopRequested = {}, QObject* parent = nullptr);
    ~LibArchiveIODevice() override;

    bool seek(qint64 pos) override;
    [[nodiscard]] qint64 size() const override;

    [[nodiscard]] bool failed() const;

    archive* releaseArchive();

protected:
    qint64 readData(char* data, qint64 maxlen) override;
    qint64 writeData(const char* data, qint64 len) override;

private:
    bool stopRequested();
    bool appendToBuffer(const char* data, qint64 len);
    bool switchToTemporaryFile();
    QIODevice* bufferDevice();
    void setArchiveError(const char* operation);

    ArchivePtr m_archive;
    archive_entry* m_entry;
    QString m_archiveFile;
    QString m_entryPath;
    ArchiveReader::StopRequestedCallback m_stopRequested;
    QBuffer m_buffer;
    std::unique_ptr<QTemporaryFile> m_tempFile;
    qint64 m_memoryLimit;
    bool m_failed;
};

class LibArchiveReader : public ArchiveReader
{
public:
    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] QString type() const override;

    bool init(const QString& file) override;
    ArchiveEntryData entry(const QString& file) override;
    bool copyEntryToDevice(const QString& file, QIODevice* device, const StopRequestedCallback& stopRequested) override;
    bool readEntries(const ReadEntryInfoCallback& readEntry, const StopRequestedCallback& stopRequested) override;
    bool readTracks(ReadEntryCallback readEntry, const StopRequestedCallback& stopRequested) override;
    QByteArray readCover(const Track& track, Track::Cover cover) override;

private:
    QString m_file;
    QString m_type;
};
} // namespace Fooyin::LibArchive
