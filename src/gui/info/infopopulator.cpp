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

class InfoPopulatorPrivate
{
public:
    explicit InfoPopulatorPrivate(InfoPopulator* self)
        : m_self{self}
    { }

    InfoItem* getOrAddNode(const QString& key, const QString& name, ItemParent parent, InfoItem::ItemType type,
                           InfoItem::ValueType valueType = InfoItem::Concat, InfoItem::FormatFunc numFunc = {});
    void checkAddParentNode(InfoModel::ItemParent parent);
    void checkAddEntryNode(const QString& key, const QString& name, InfoModel::ItemParent parent);

    template <typename Value>
    void checkAddEntryNode(const QString& key, const QString& name, InfoModel::ItemParent parent, Value&& value,
                           InfoItem::ValueType valueType  = InfoItem::ValueType::Concat,
                           InfoItem::FormatFunc&& numFunc = {}, bool includeEmpty = false)
    {
        if(!includeEmpty && (!std::is_same_v<Value, QString> || std::is_same_v<Value, QStringList>)) {
            if(value.isEmpty()) {
                return;
            }
        }

        checkAddParentNode(parent);
        auto* node = getOrAddNode(key, name, parent, InfoItem::Entry, valueType, std::move(numFunc));
        node->addTrackValue(std::forward<Value>(value));
    }

    void addTrackMetadata(const Track& track, bool extended);
    void addTrackLocation(int total, const Track& track);
    void addTrackGeneral(int total, const Track& track);
    void addTrackOther(const Track& track);

    void addTrackNodes(InfoItem::Options options, const TrackList& tracks);

    InfoPopulator* m_self;
    InfoData m_data;
};

InfoItem* InfoPopulatorPrivate::getOrAddNode(const QString& key, const QString& name, ItemParent parent,
                                             InfoItem::ItemType type, InfoItem::ValueType valueType,
                                             InfoItem::FormatFunc numFunc)
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

void InfoPopulatorPrivate::checkAddParentNode(InfoModel::ItemParent parent)
{
    if(parent == InfoModel::ItemParent::Metadata) {
        getOrAddNode(QStringLiteral("Metadata"), InfoPopulator::tr("Metadata"), ItemParent::Root, InfoItem::Header);
    }
    else if(parent == InfoModel::ItemParent::Location) {
        getOrAddNode(QStringLiteral("Location"), InfoPopulator::tr("Location"), ItemParent::Root, InfoItem::Header);
    }
    else if(parent == InfoModel::ItemParent::General) {
        getOrAddNode(QStringLiteral("General"), InfoPopulator::tr("General"), ItemParent::Root, InfoItem::Header);
    }
    else if(parent == InfoModel::ItemParent::Other) {
        getOrAddNode(QStringLiteral("Other"), InfoPopulator::tr("Other"), ItemParent::Root, InfoItem::Header);
    }
}

void InfoPopulatorPrivate::checkAddEntryNode(const QString& key, const QString& name, InfoModel::ItemParent parent)
{
    checkAddParentNode(parent);
    getOrAddNode(key, name, parent, InfoItem::Entry);
}

void InfoPopulatorPrivate::addTrackMetadata(const Track& track, bool extended)
{
    checkAddEntryNode(QStringLiteral("Artist"), InfoPopulator::tr("Artist"), ItemParent::Metadata, track.artists());
    checkAddEntryNode(QStringLiteral("Title"), InfoPopulator::tr("Title"), ItemParent::Metadata, track.title());
    checkAddEntryNode(QStringLiteral("Album"), InfoPopulator::tr("Album"), ItemParent::Metadata, track.album());
    checkAddEntryNode(QStringLiteral("Date"), InfoPopulator::tr("Date"), ItemParent::Metadata, track.date());
    checkAddEntryNode(QStringLiteral("Genre"), InfoPopulator::tr("Genre"), ItemParent::Metadata, track.genres());
    checkAddEntryNode(QStringLiteral("AlbumArtist"), InfoPopulator::tr("Album Artist"), ItemParent::Metadata,
                      track.albumArtists());

    if(!track.trackNumber().isEmpty()) {
        checkAddEntryNode(QStringLiteral("TrackNumber"), InfoPopulator::tr("Track Number"), ItemParent::Metadata,
                          track.trackNumber());
    }

    if(extended) {
        const auto extras = track.extraTags();
        for(const auto& [tag, values] : Utils::asRange(extras)) {
            const auto extraTag = QStringLiteral("<%1>").arg(tag);
            checkAddEntryNode(extraTag, extraTag, ItemParent::Metadata, values);
        }
    }
}

void InfoPopulatorPrivate::addTrackLocation(int total, const Track& track)
{
    checkAddEntryNode(QStringLiteral("FileName"),
                      total > 1 ? InfoPopulator::tr("File Names") : InfoPopulator::tr("File Name"),
                      ItemParent::Location, track.filename());
    checkAddEntryNode(QStringLiteral("FolderName"),
                      total > 1 ? InfoPopulator::tr("Folder Names") : InfoPopulator::tr("Folder Name"),
                      ItemParent::Location, track.path());

    if(total == 1) {
        checkAddEntryNode(QStringLiteral("FilePath"), InfoPopulator::tr("File Path"), ItemParent::Location,
                          track.prettyFilepath());
        if(track.subsong() >= 0) {
            checkAddEntryNode(QStringLiteral("SubsongIndex"), InfoPopulator::tr("Subsong Index"), ItemParent::Location,
                              QString::number(track.subsong()));
        }
    }

    checkAddEntryNode(QStringLiteral("FileSize"),
                      total > 1 ? InfoPopulator::tr("Total Size") : InfoPopulator::tr("File Size"),
                      ItemParent::Location, QString::number(track.fileSize()), InfoItem::Total,
                      InfoItem::FormatUIntFunc{[](uint64_t size) -> QString {
                          return Utils::formatFileSize(size, true);
                      }});
    checkAddEntryNode(QStringLiteral("LastModified"), InfoPopulator::tr("Last Modified"), ItemParent::Location,
                      QString::number(track.modifiedTime()), InfoItem::Max,
                      InfoItem::FormatUIntFunc{Utils::formatTimeMs});

    if(total == 1) {
        checkAddEntryNode(QStringLiteral("Added"), InfoPopulator::tr("Added"), ItemParent::Location,
                          QString::number(track.addedTime()), InfoItem::Max,
                          InfoItem::FormatUIntFunc{Utils::formatTimeMs});
    }
}

void InfoPopulatorPrivate::addTrackGeneral(int total, const Track& track)
{
    if(total > 1) {
        checkAddEntryNode(QStringLiteral("Tracks"), InfoPopulator::tr("Tracks"), ItemParent::General,
                          QStringLiteral("1"), InfoItem::Total);
    }

    checkAddEntryNode(QStringLiteral("Duration"), InfoPopulator::tr("Duration"), ItemParent::General,
                      QString::number(track.duration()), InfoItem::Total,
                      InfoItem::FormatUIntFunc{Utils::msToStringExtended});
    checkAddEntryNode(QStringLiteral("Channels"), InfoPopulator::tr("Channels"), ItemParent::General,
                      QString::number(track.channels()), InfoItem::Percentage);
    checkAddEntryNode(QStringLiteral("BitDepth"), InfoPopulator::tr("Bit Depth"), ItemParent::General,
                      QString::number(track.bitDepth()), InfoItem::Percentage);
    if(track.bitrate() > 0) {
        checkAddEntryNode(QStringLiteral("Bitrate"),
                          total > 1 ? InfoPopulator::tr("Avg. Bitrate") : InfoPopulator::tr("Bitrate"),
                          ItemParent::General, QString::number(track.bitrate()), InfoItem::Average,
                          InfoItem::FormatUIntFunc{[](uint64_t bitrate) -> QString {
                              return QStringLiteral("%1 kbps").arg(bitrate);
                          }});
    }
    checkAddEntryNode(QStringLiteral("SampleRate"), InfoPopulator::tr("Sample Rate"), ItemParent::General,
                      QStringLiteral("%1 Hz").arg(track.sampleRate()), InfoItem::Percentage);
    checkAddEntryNode(QStringLiteral("Codec"), InfoPopulator::tr("Codec"), ItemParent::General, track.codec(),
                      InfoItem::Percentage);
}

void InfoPopulatorPrivate::addTrackOther(const Track& track)
{
    const auto props = track.extraProperties();
    for(const auto& [prop, value] : Utils::asRange(props)) {
        const auto extraProp = QStringLiteral("<%1>").arg(prop);
        checkAddEntryNode(extraProp, extraProp, ItemParent::Other, value, InfoItem::Percentage);
    }
}

void InfoPopulatorPrivate::addTrackNodes(InfoItem::Options options, const TrackList& tracks)
{
    const int total = static_cast<int>(tracks.size());

    for(const Track& track : tracks) {
        if(!m_self->mayRun()) {
            return;
        }

        if(options & InfoItem::Metadata) {
            addTrackMetadata(track, options & InfoItem::ExtendedMetadata);
        }
        if(options & InfoItem::Location) {
            addTrackLocation(total, track);
        }
        if(options & InfoItem::General) {
            addTrackGeneral(total, track);
        }
        if(options & InfoItem::Other) {
            addTrackOther(track);
        }
    }
}

InfoPopulator::InfoPopulator(QObject* parent)
    : Worker{parent}
    , p{std::make_unique<InfoPopulatorPrivate>(this)}
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
