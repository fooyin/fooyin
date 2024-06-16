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

#include "infopopulator.h"

#include "infomodel.h"

#include <core/track.h>
#include <utils/enum.h>
#include <utils/utils.h>

#include <QFileInfo>

namespace Fooyin {
using ItemParent = InfoModel::ItemParent;

struct InfoPopulator::Private
{
    InfoPopulator* m_self;

    InfoData m_data;

    explicit Private(InfoPopulator* self)
        : m_self{self}
    { }

    InfoItem* getOrAddNode(const QString& key, const QString& name, ItemParent parent, InfoItem::ItemType type,
                           InfoItem::ValueType valueType = InfoItem::Concat, InfoItem::FormatFunc numFunc = {})
    {
        if(key.isEmpty() || name.isEmpty()) {
            return nullptr;
        }

        if(m_data.nodes.contains(key)) {
            return &m_data.nodes.at(key);
        }

        InfoItem item{type, name, nullptr, valueType, std::move(numFunc)};
        InfoItem* node = &m_data.nodes.emplace(key, std::move(item)).first->second;
        m_data.parents[Utils::Enum::toString(parent)].emplace_back(key);

        return node;
    }

    void checkAddParentNode(InfoModel::ItemParent parent)
    {
        if(parent == InfoModel::ItemParent::Metadata) {
            getOrAddNode(QStringLiteral("Metadata"), tr("Metadata"), ItemParent::Root, InfoItem::Header);
        }
        else if(parent == InfoModel::ItemParent::Location) {
            getOrAddNode(QStringLiteral("Location"), tr("Location"), ItemParent::Root, InfoItem::Header);
        }
        else if(parent == InfoModel::ItemParent::General) {
            getOrAddNode(QStringLiteral("General"), tr("General"), ItemParent::Root, InfoItem::Header);
        }
    }

    void checkAddEntryNode(const QString& key, const QString& name, InfoModel::ItemParent parent)
    {
        checkAddParentNode(parent);
        getOrAddNode(key, name, parent, InfoItem::Entry);
    }

    template <typename Value>
    void checkAddEntryNode(const QString& key, const QString& name, InfoModel::ItemParent parent, Value&& value,
                           InfoItem::ValueType valueType = InfoItem::ValueType::Concat,
                           InfoItem::FormatFunc numFunc  = {})
    {
        if constexpr(std::is_same_v<Value, QString> || std::is_same_v<Value, QStringList>) {
            if(value.isEmpty()) {
                return;
            }
        }

        checkAddParentNode(parent);
        auto* node = getOrAddNode(key, name, parent, InfoItem::Entry, valueType, std::move(numFunc));
        node->addTrackValue(std::forward<Value>(value));
    }

    void addTrackMetadata(const Track& track)
    {
        if(const auto artists = track.artists(); !artists.empty()) {
            checkAddEntryNode(QStringLiteral("Artist"), tr("Artist"), ItemParent::Metadata, artists);
        }

        checkAddEntryNode(QStringLiteral("Title"), tr("Title"), ItemParent::Metadata, track.title());
        checkAddEntryNode(QStringLiteral("Album"), tr("Album"), ItemParent::Metadata, track.album());
        checkAddEntryNode(QStringLiteral("Date"), tr("Date"), ItemParent::Metadata, track.date());
        checkAddEntryNode(QStringLiteral("Genre"), tr("Genre"), ItemParent::Metadata, track.genres());
        checkAddEntryNode(QStringLiteral("AlbumArtist"), tr("Album Artist"), ItemParent::Metadata, track.albumArtist());

        if(const int trackNumber = track.trackNumber(); trackNumber >= 0) {
            checkAddEntryNode(QStringLiteral("TrackNumber"), tr("Track Number"), ItemParent::Metadata, trackNumber);
        }
    }

    void addTrackLocation(int total, const Track& track)
    {
        const QFileInfo file{track.filepath()};

        checkAddEntryNode(QStringLiteral("FileName"), total > 1 ? tr("File Names") : tr("File Name"),
                          ItemParent::Location, file.fileName());
        checkAddEntryNode(QStringLiteral("FolderName"), total > 1 ? tr("Folder Names") : tr("Folder Name"),
                          ItemParent::Location, file.absolutePath());

        if(total == 1) {
            checkAddEntryNode(QStringLiteral("FilePath"), tr("File Path"), ItemParent::Location, track.filepath());
        }

        checkAddEntryNode(QStringLiteral("FileSize"), total > 1 ? tr("Total Size") : tr("File Size"),
                          ItemParent::Location, track.fileSize(), InfoItem::Total,
                          [](uint64_t size) -> QString { return Utils::formatFileSize(size, true); });
        checkAddEntryNode(QStringLiteral("LastModified"), tr("Last Modified"), ItemParent::Location,
                          track.modifiedTime(), InfoItem::Max, Utils::formatTimeMs);

        if(total == 1) {
            checkAddEntryNode(QStringLiteral("Added"), tr("Added"), ItemParent::Location, track.addedTime(),
                              InfoItem::Max, Utils::formatTimeMs);
        }
    }

    void addTrackGeneral(int total, const Track& track)
    {
        if(total > 1) {
            checkAddEntryNode(QStringLiteral("Tracks"), tr("Tracks"), ItemParent::General, 1, InfoItem::Total);
        }

        checkAddEntryNode(QStringLiteral("Duration"), tr("Duration"), ItemParent::General, track.duration(),
                          InfoItem::Total, Utils::msToStringExtended);
        checkAddEntryNode(QStringLiteral("Channels"), tr("Channels"), ItemParent::General, track.channels(),
                          InfoItem::Percentage);
        checkAddEntryNode(QStringLiteral("BitDepth"), tr("Bit Depth"), ItemParent::General, track.bitDepth(),
                          InfoItem::Percentage);
        checkAddEntryNode(QStringLiteral("Bitrate"), total > 1 ? tr("Avg. Bitrate") : tr("Bitrate"),
                          ItemParent::General, track.bitrate(), InfoItem::Average, [](uint64_t bitrate) -> QString {
                              return QString::number(bitrate) + QStringLiteral(" kbps");
                          });
        checkAddEntryNode(QStringLiteral("SampleRate"), tr("Sample Rate"), ItemParent::General,
                          QStringLiteral("%1 Hz").arg(track.sampleRate()), InfoItem::Percentage);
        checkAddEntryNode(QStringLiteral("Codec"), tr("Codec"), ItemParent::General, track.typeString(),
                          InfoItem::Percentage);
    }

    void addTrackNodes(InfoItem::Options options, const TrackList& tracks)
    {
        const int total = static_cast<int>(tracks.size());

        for(const Track& track : tracks) {
            if(!m_self->mayRun()) {
                return;
            }

            if(options & InfoItem::Metadata) {
                addTrackMetadata(track);
            }
            if(options & InfoItem::Location) {
                addTrackLocation(total, track);
            }
            if(options & InfoItem::General) {
                addTrackGeneral(total, track);
            }
        }
    }
};

InfoPopulator::InfoPopulator(QObject* parent)
    : Worker{parent}
    , p{std::make_unique<Private>(this)}
{ }

InfoPopulator::~InfoPopulator() = default;

void InfoPopulator::run(InfoItem::Options options, const TrackList& tracks)
{
    setState(Running);

    p->m_data.clear();

    p->addTrackNodes(options, tracks);

    if(mayRun()) {
        emit populated(p->m_data);
        emit finished();
    }

    setState(Idle);
}
} // namespace Fooyin
