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

#include <core/library/librarymanager.h>
#include <core/track.h>
#include <utils/enum.h>
#include <utils/stringutils.h>
#include <utils/utils.h>

#include <QFileInfo>

#include <set>

using namespace Qt::StringLiterals;

namespace Fooyin {
using ItemParent = InfoModel::ItemParent;

class InfoPopulatorPrivate
{
public:
    explicit InfoPopulatorPrivate(InfoPopulator* self, LibraryManager* libraryManager)
        : m_self{self}
        , m_libraryManager{libraryManager}
    { }

    void reset();

    InfoItem* getOrAddNode(const QString& key, const QString& name, ItemParent parent, InfoItem::ItemType type,
                           InfoItem::ValueType valueType = InfoItem::Concat, const InfoItem::FormatFunc& numFunc = {});
    void checkAddParentNode(InfoModel::ItemParent parent);
    void checkAddEntryNode(const QString& key, const QString& name, InfoModel::ItemParent parent);

    template <typename Value>
    void checkAddEntryNode(const QString& key, const QString& name, InfoModel::ItemParent parent, Value&& value,
                           InfoItem::ValueType valueType  = InfoItem::ValueType::Concat,
                           InfoItem::FormatFunc&& numFunc = {}, bool isFloat = false)
    {
        checkAddParentNode(parent);
        auto* node = getOrAddNode(key, name, parent, InfoItem::Entry, valueType, numFunc);
        node->setIsFloat(isFloat);

        if constexpr(std::is_same_v<Value, int> || std::is_same_v<Value, uint64_t>) {
            if(value <= 0) {
                node->addTrackValue(QString{});
            }
            else {
                node->addTrackValue(QString::number(value));
            }
        }
        else {
            node->addTrackValue(std::forward<Value>(value));
        }
    }

    void addTrackMetadata(const Track& track, bool extended);
    void addTrackLocation(int total, const Track& track);
    void addTrackGeneral(int total, const Track& track);
    void addTrackReplayGain(int total);
    void addTrackOther(const Track& track);

    void addTrackNodes(InfoItem::Options options, const TrackList& tracks);

    InfoPopulator* m_self;
    LibraryManager* m_libraryManager;

    InfoData m_data;
    std::set<float> m_trackGain;
    std::set<float> m_trackPeak;
    std::set<float> m_albumGain;
    std::set<float> m_albumPeak;
};

void InfoPopulatorPrivate::reset()
{
    m_data.clear();
    m_trackGain.clear();
    m_trackPeak.clear();
    m_albumGain.clear();
    m_albumPeak.clear();
}

InfoItem* InfoPopulatorPrivate::getOrAddNode(const QString& key, const QString& name, ItemParent parent,
                                             InfoItem::ItemType type, InfoItem::ValueType valueType,
                                             const InfoItem::FormatFunc& numFunc)
{
    if(key.isEmpty() || name.isEmpty()) {
        return nullptr;
    }

    if(m_data.nodes.contains(key)) {
        return &m_data.nodes.at(key);
    }

    const InfoItem item{type, name, nullptr, valueType, numFunc};
    InfoItem* node = &m_data.nodes.emplace(key, std::move(item)).first->second;
    m_data.parents[Utils::Enum::toString(parent)].emplace_back(key);

    return node;
}

void InfoPopulatorPrivate::checkAddParentNode(InfoModel::ItemParent parent)
{
    if(parent == InfoModel::ItemParent::Metadata) {
        getOrAddNode(u"Metadata"_s, InfoPopulator::tr("Metadata"), ItemParent::Root, InfoItem::Header);
    }
    else if(parent == InfoModel::ItemParent::Location) {
        getOrAddNode(u"Location"_s, InfoPopulator::tr("Location"), ItemParent::Root, InfoItem::Header);
    }
    else if(parent == InfoModel::ItemParent::General) {
        getOrAddNode(u"General"_s, InfoPopulator::tr("General"), ItemParent::Root, InfoItem::Header);
    }
    else if(parent == InfoModel::ItemParent::ReplayGain) {
        getOrAddNode(u"ReplayGain"_s, InfoPopulator::tr("ReplayGain"), ItemParent::Root, InfoItem::Header);
    }
    else if(parent == InfoModel::ItemParent::Other) {
        getOrAddNode(u"Other"_s, InfoPopulator::tr("Other"), ItemParent::Root, InfoItem::Header);
    }
}

void InfoPopulatorPrivate::checkAddEntryNode(const QString& key, const QString& name, InfoModel::ItemParent parent)
{
    checkAddParentNode(parent);
    getOrAddNode(key, name, parent, InfoItem::Entry);
}

void InfoPopulatorPrivate::addTrackMetadata(const Track& track, bool extended)
{
    checkAddEntryNode(u"Artist"_s, InfoPopulator::tr("Artist"), ItemParent::Metadata, track.artists());
    checkAddEntryNode(u"Title"_s, InfoPopulator::tr("Title"), ItemParent::Metadata, track.title());
    checkAddEntryNode(u"Album"_s, InfoPopulator::tr("Album"), ItemParent::Metadata, track.album());
    checkAddEntryNode(u"Date"_s, InfoPopulator::tr("Date"), ItemParent::Metadata, track.date());
    checkAddEntryNode(u"Genre"_s, InfoPopulator::tr("Genre"), ItemParent::Metadata, track.genres());
    checkAddEntryNode(u"AlbumArtist"_s, InfoPopulator::tr("Album Artist"), ItemParent::Metadata, track.albumArtists());

    if(!track.trackNumber().isEmpty()) {
        checkAddEntryNode(u"TrackNumber"_s, InfoPopulator::tr("Track Number"), ItemParent::Metadata,
                          track.trackNumber());
    }

    if(extended) {
        const auto extras = track.extraTags();
        for(const auto& [tag, values] : Utils::asRange(extras)) {
            const auto extraTag = u"<%1>"_s.arg(tag);
            checkAddEntryNode(extraTag, extraTag, ItemParent::Metadata, values);
        }
    }
}

void InfoPopulatorPrivate::addTrackLocation(int total, const Track& track)
{
    checkAddEntryNode(u"FileName"_s, total > 1 ? InfoPopulator::tr("File Names") : InfoPopulator::tr("File Name"),
                      ItemParent::Location, track.filename());
    checkAddEntryNode(u"FolderName"_s, total > 1 ? InfoPopulator::tr("Folder Names") : InfoPopulator::tr("Folder Name"),
                      ItemParent::Location, track.path());

    if(total == 1) {
        checkAddEntryNode(u"FilePath"_s, InfoPopulator::tr("File Path"), ItemParent::Location, track.prettyFilepath());
        if(track.subsong() >= 0) {
            checkAddEntryNode(u"SubsongIndex"_s, InfoPopulator::tr("Subsong Index"), ItemParent::Location,
                              track.subsong());
        }
    }

    checkAddEntryNode(u"FileSize"_s, total > 1 ? InfoPopulator::tr("Total Size") : InfoPopulator::tr("File Size"),
                      ItemParent::Location, track.fileSize(), InfoItem::Total,
                      InfoItem::FormatUIntFunc{[](uint64_t size) -> QString {
                          return Utils::formatFileSize(size, true);
                      }});
    checkAddEntryNode(u"LastModified"_s, InfoPopulator::tr("Last Modified"), ItemParent::Location, track.modifiedTime(),
                      InfoItem::Max, InfoItem::FormatUIntFunc{Utils::formatTimeMs});

    if(track.isInLibrary()) {
        if(const auto library = m_libraryManager->libraryInfo(track.libraryId())) {
            checkAddEntryNode(u"Library"_s, InfoPopulator::tr("Library"), ItemParent::Location, library->name);
        }
    }

    if(total == 1) {
        checkAddEntryNode(u"Added"_s, InfoPopulator::tr("Added"), ItemParent::Location, track.addedTime(),
                          InfoItem::Max, InfoItem::FormatUIntFunc{Utils::formatTimeMs});
    }
}

void InfoPopulatorPrivate::addTrackGeneral(int total, const Track& track)
{
    if(total > 1) {
        checkAddEntryNode(u"Tracks"_s, InfoPopulator::tr("Tracks"), ItemParent::General, u"1"_s, InfoItem::Total);
    }

    checkAddEntryNode(u"Duration"_s, InfoPopulator::tr("Duration"), ItemParent::General, track.duration(),
                      InfoItem::Total, InfoItem::FormatUIntFunc{[](uint64_t ms) {
                          return Utils::msToString(ms);
                      }});
    checkAddEntryNode(u"Channels"_s, InfoPopulator::tr("Channels"), ItemParent::General, track.channels(),
                      InfoItem::Percentage);
    checkAddEntryNode(u"BitDepth"_s, InfoPopulator::tr("Bit Depth"), ItemParent::General, track.bitDepth(),
                      InfoItem::Percentage);
    if(track.bitrate() > 0) {
        checkAddEntryNode(u"Bitrate"_s, total > 1 ? InfoPopulator::tr("Avg. Bitrate") : InfoPopulator::tr("Bitrate"),
                          ItemParent::General, track.bitrate(), InfoItem::Average,
                          InfoItem::FormatUIntFunc{[](uint64_t bitrate) -> QString {
                              return u"%1 kbps"_s.arg(bitrate);
                          }});
    }
    checkAddEntryNode(u"SampleRate"_s, InfoPopulator::tr("Sample Rate"), ItemParent::General,
                      u"%1 Hz"_s.arg(track.sampleRate()), InfoItem::Percentage);
    checkAddEntryNode(u"Codec"_s, InfoPopulator::tr("Codec"), ItemParent::General,
                      !track.codec().isEmpty() ? track.codec() : track.extension().toUpper(), InfoItem::Percentage);
    checkAddEntryNode(u"CodecProfile"_s, InfoPopulator::tr("Codec Profile"), ItemParent::General, track.codecProfile(),
                      InfoItem::Percentage);
    checkAddEntryNode(u"Tool"_s, InfoPopulator::tr("Tool"), ItemParent::General, track.tool(), InfoItem::Percentage);
    checkAddEntryNode(u"TagTypes"_s, InfoPopulator::tr("Tag Types"), ItemParent::General, track.tagType(u" | "_s),
                      InfoItem::Percentage);
}

void InfoPopulatorPrivate::addTrackReplayGain(int total)
{
    const auto formatGain = [](const double gain) -> QString {
        return u"%1 dB"_s.arg(QString::number(gain, 'f', 2));
    };
    const auto formatPeak = [](const double gain) -> QString {
        return QString::number(gain, 'f', 6);
    };

    if(m_trackGain.size() == 1) {
        checkAddEntryNode(u"TrackGain"_s, InfoPopulator::tr("Track Gain"), ItemParent::ReplayGain,
                          QString::number(*m_trackGain.cbegin()), InfoItem::Total,
                          InfoItem::FormatFloatFunc{formatGain}, true);
    }
    if(m_trackPeak.size() == 1) {
        checkAddEntryNode(u"TrackPeak"_s, InfoPopulator::tr("Track Peak"), ItemParent::ReplayGain,
                          QString::number(*m_trackPeak.cbegin()), InfoItem::Total,
                          InfoItem::FormatFloatFunc{formatPeak}, true);
    }
    if(m_albumGain.size() == 1) {
        checkAddEntryNode(u"AlbumGain"_s, InfoPopulator::tr("Album Gain"), ItemParent::ReplayGain,
                          QString::number(*m_albumGain.cbegin()), InfoItem::Total,
                          InfoItem::FormatFloatFunc{formatGain}, true);
    }
    if(m_albumPeak.size() == 1) {
        checkAddEntryNode(u"AlbumPeak"_s, InfoPopulator::tr("Album Peak"), ItemParent::ReplayGain,
                          QString::number(*m_albumPeak.cbegin()), InfoItem::Total,
                          InfoItem::FormatFloatFunc{formatPeak}, true);
    }
    if(total > 1 && !m_trackPeak.empty()) {
        checkAddEntryNode(u"TotalPeak"_s, InfoPopulator::tr("Total Peak"), ItemParent::ReplayGain,
                          QString::number(*std::ranges::max_element(m_trackPeak)), InfoItem::Max,
                          InfoItem::FormatFloatFunc{formatPeak}, true);
    }
}

void InfoPopulatorPrivate::addTrackOther(const Track& track)
{
    const auto props = track.extraProperties();
    for(const auto& [prop, value] : Utils::asRange(props)) {
        const auto extraProp = u"<%1>"_s.arg(prop);
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

        if(track.hasTrackGain()) {
            m_trackGain.emplace(track.rgTrackGain());
        }
        if(track.hasTrackPeak()) {
            m_trackPeak.emplace(track.rgTrackPeak());
        }
        if(track.hasAlbumGain()) {
            m_albumGain.emplace(track.rgAlbumGain());
        }
        if(track.hasAlbumPeak()) {
            m_albumPeak.emplace(track.rgAlbumPeak());
        }
    }

    if(options & InfoItem::ReplayGain) {
        addTrackReplayGain(total);
    }
}

InfoPopulator::InfoPopulator(LibraryManager* libraryManager, QObject* parent)
    : Worker{parent}
    , p{std::make_unique<InfoPopulatorPrivate>(this, libraryManager)}
{ }

InfoPopulator::~InfoPopulator() = default;

void InfoPopulator::run(InfoItem::Options options, const TrackList& tracks)
{
    setState(Running);

    p->reset();

    p->addTrackNodes(options, tracks);

    if(mayRun()) {
        emit populated(p->m_data);
        emit finished();
    }

    p->reset();

    setState(Idle);
}
} // namespace Fooyin
