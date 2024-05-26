/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "infomodel.h"

#include <core/player/playercontroller.h>
#include <core/track.h>
#include <utils/enum.h>
#include <utils/utils.h>

#include <QCollator>
#include <QFileInfo>
#include <QFont>

#include <algorithm>

constexpr auto HeaderFontDelta = 2;

namespace {
QString formatPercentage(const std::map<QString, int>& values)
{
    if(values.size() == 1) {
        return values.cbegin()->first;
    }

    int count{0};
    for(const auto& [_, value] : values) {
        count += value;
    }

    std::map<QString, double> ratios;
    for(const auto& [key, value] : values) {
        ratios[key] = (static_cast<double>(value) / count) * 100;
    }

    QStringList formattedList;
    for(const auto& [key, value] : ratios) {
        formattedList.append(QStringLiteral("%1 (%2%)").arg(key, QString::number(value, 'f', 1)));
    }

    return formattedList.join(u"; ");
}

QString sortJoinSet(const std::set<QString>& values)
{
    QStringList list;
    for(const QString& value : values) {
        list.append(value);
    }

    QCollator collator;
    collator.setNumericMode(true);

    std::ranges::sort(list, collator);

    return list.join(u"; ");
}
} // namespace

namespace Fooyin {
InfoItem::InfoItem()
    : InfoItem{Header, QStringLiteral(""), nullptr, ValueType::Concat, {}}
{ }

InfoItem::InfoItem(ItemType type, QString name, InfoItem* parent, ValueType valueType)
    : InfoItem{type, std::move(name), parent, valueType, {}}
{ }

InfoItem::InfoItem(ItemType type, QString name, InfoItem* parent, ValueType valueType, FormatFunc numFunc)
    : TreeItem{parent}
    , m_type{type}
    , m_valueType{valueType}
    , m_name{std::move(name)}
    , m_numValue{0}
    , m_formatNum{std::move(numFunc)}
{ }

InfoItem::ItemType InfoItem::type() const
{
    return m_type;
}

QString InfoItem::name() const
{
    return m_name;
}

QVariant InfoItem::value() const
{
    switch(m_valueType) {
        case(ValueType::Concat): {
            if(m_value.isEmpty()) {
                m_value = sortJoinSet(m_values);
            }
            return m_value;
        }
        case(ValueType::Percentage): {
            if(m_value.isEmpty()) {
                m_value = formatPercentage(m_percentValues);
            }
            return m_value;
        }
        case(ValueType::Average):
            if(m_numValue == 0 && !m_numValues.empty()) {
                m_numValue = std::reduce(m_numValues.cbegin(), m_numValues.cend()) / m_numValues.size();
            }
            // Fallthrough
        case(ValueType::Total):
        case(ValueType::Max):
            if(m_formatNum) {
                return m_formatNum(m_numValue);
            }
            return QVariant::fromValue(m_numValue);
    }
    return m_value;
}

void InfoItem::addTrackValue(uint64_t value)
{
    switch(m_valueType) {
        case(ValueType::Concat):
            addTrackValue(QString::number(value));
            break;
        case(ValueType::Average):
            m_numValues.push_back(value);
            break;
        case(ValueType::Total):
            m_numValue += value;
            break;
        case(ValueType::Max):
            m_numValue = std::max(m_numValue, value);
            break;
        case(ValueType::Percentage):
            m_percentValues[QString::number(value)]++;
            break;
    }
}

void InfoItem::addTrackValue(int value)
{
    value = std::max(value, 0);
    addTrackValue(static_cast<uint64_t>(value));
}

void InfoItem::addTrackValue(const QString& value)
{
    if(value.isEmpty()) {
        return;
    }

    if(m_valueType == ValueType::Percentage) {
        m_percentValues[value]++;
        return;
    }

    if(m_values.size() > 40) {
        return;
    }

    m_values.emplace(value);
}

void InfoItem::addTrackValue(const QStringList& values)
{
    for(const auto& strValue : values) {
        addTrackValue(strValue);
    }
}

struct InfoModel::Private
{
    InfoModel* self;

    std::unordered_map<QString, InfoItem> nodes;

    QFont headerFont;

    explicit Private(InfoModel* self_)
        : self{self_}
    {
        headerFont.setPointSize(headerFont.pointSize() + HeaderFontDelta);
    }

    void reset()
    {
        self->resetRoot();
        nodes.clear();
    }

    InfoItem* getOrAddNode(const QString& key, const QString& name, ItemParent parent, InfoItem::ItemType type,
                           InfoItem::ValueType valueType = InfoItem::Concat, InfoItem::FormatFunc numFunc = {})
    {
        if(key.isEmpty() || name.isEmpty()) {
            return nullptr;
        }

        if(nodes.contains(key)) {
            return &nodes.at(key);
        }

        InfoItem* parentItem{nullptr};

        if(parent == ItemParent::Root) {
            parentItem = self->rootItem();
        }
        else {
            const QString parentKey = Utils::Enum::toString(parent);
            if(nodes.contains(parentKey)) {
                parentItem = &nodes.at(parentKey);
            }
        }

        if(!parentItem) {
            return nullptr;
        }

        InfoItem item{type, name, parentItem, valueType, std::move(numFunc)};
        InfoItem* node = &nodes.emplace(key, std::move(item)).first->second;
        parentItem->appendChild(node);

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

    void addTrackNodes(int total, const Track& track)
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

        checkAddEntryNode(QStringLiteral("Duration"), tr("Duration"), ItemParent::General, track.duration(),
                          InfoItem::Total, Utils::msToString);
        checkAddEntryNode(QStringLiteral("Channels"), tr("Channels"), ItemParent::General, track.channels(),
                          InfoItem::Percentage);
        if(const int bitDepth = track.bitDepth(); bitDepth > 0) {
            checkAddEntryNode(QStringLiteral("BitDepth"), tr("Bit Depth"), ItemParent::General, bitDepth,
                              InfoItem::Percentage);
        }
        checkAddEntryNode(QStringLiteral("Bitrate"), total > 1 ? tr("Avg. Bitrate") : tr("Bitrate"),
                          ItemParent::General, track.bitrate(), InfoItem::Average, [](uint64_t bitrate) -> QString {
                              return QString::number(bitrate) + QStringLiteral("kbps");
                          });

        checkAddEntryNode(QStringLiteral("SampleRate"), tr("Sample Rate"), ItemParent::General,
                          QString::number(track.sampleRate()) + QStringLiteral(" Hz"), InfoItem::Percentage);
        checkAddEntryNode(QStringLiteral("Codec"), tr("Codec"), ItemParent::General, track.typeString(),
                          InfoItem::Percentage);
    }
};

InfoModel::InfoModel(QObject* parent)
    : TreeModel{parent}
    , p{std::make_unique<Private>(this)}
{ }

InfoModel::~InfoModel() = default;

void InfoModel::resetModel(const TrackList& tracks, const Track& playingTrack)
{
    TrackList infoTracks{tracks};

    if(infoTracks.empty() && playingTrack.isValid()) {
        infoTracks.push_back(playingTrack);
    }

    beginResetModel();
    p->reset();

    const int total = static_cast<int>(infoTracks.size());

    if(total > 0) {
        p->checkAddEntryNode(QStringLiteral("Tracks"), tr("Tracks"), ItemParent::General, total, InfoItem::Total);

        for(const Track& track : infoTracks) {
            p->addTrackNodes(total, track);
        }
    }

    endResetModel();
}

QVariant InfoModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case(0):
            return QStringLiteral("Name");
        case(1):
            return QStringLiteral("Value");
        default:
            break;
    }

    return {};
}

int InfoModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 2;
}

QVariant InfoModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const auto* item              = static_cast<InfoItem*>(index.internalPointer());
    const InfoItem::ItemType type = item->type();

    if(role == InfoItem::Type) {
        return QVariant::fromValue<InfoItem::ItemType>(type);
    }

    if(role == Qt::FontRole) {
        if(type == InfoItem::Header) {
            return p->headerFont;
        }
        return {};
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    switch(index.column()) {
        case(0):
            return item->name();
        case(1):
            return item->value();
        default:
            break;
    }

    return {};
}
} // namespace Fooyin

#include "moc_infomodel.cpp"
