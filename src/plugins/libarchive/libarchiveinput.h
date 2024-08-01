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

#pragma once

#include <core/engine/audioinput.h>
#include <core/engine/audioloader.h>

#include <QBuffer>
#include <QFile>

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
    LibArchiveIODevice(ArchivePtr archive, archive_entry* entry, QObject* parent = nullptr);
    ~LibArchiveIODevice() override;

    bool seek(qint64 pos) override;
    [[nodiscard]] qint64 size() const override;

protected:
    qint64 readData(char* data, qint64 maxlen) override;
    qint64 writeData(const char* data, qint64 len) override;

private:
    ArchivePtr m_archive;
    archive_entry* m_entry;
    QBuffer m_buffer;
};

class FYCORE_EXPORT LibArchiveReader : public ArchiveReader
{
public:
    [[nodiscard]] QStringList extensions() const override;

    bool init(const QString& file) override;
    [[nodiscard]] QString type() const override;
    [[nodiscard]] QStringList entryList() const override;
    [[nodiscard]] std::unique_ptr<QIODevice> entry(const QString& file) const override;

    [[nodiscard]] QByteArray readCover(const Track& track, Track::Cover cover) override;

private:
    QString m_file;
    std::unique_ptr<LibArchiveIODevice> m_device;
    QString m_type;
    QStringList m_entries;
};
} // namespace Fooyin::Archive
