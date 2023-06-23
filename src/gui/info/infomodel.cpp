/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/models/track.h>
#include <core/player/playermanager.h>

#include <utils/enumhelper.h>
#include <utils/utils.h>

#include <QFileInfo>
#include <utility>

namespace Fy::Gui::Widgets::Info {
InfoItem::InfoItem()
    : InfoItem{ItemType::Header, "", nullptr, ValueType::Concat, {}}
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
                m_value = m_values.join("; ");
            }
            return m_value;
        }
        case(ValueType::Average): {
            if(m_numValue == 0) {
                m_numValue = std::reduce(m_numValues.cbegin(), m_numValues.cend(), 0) / m_numValues.size();
            }
            return QVariant::fromValue(m_numValue);
        }
        case(ValueType::Total):
            if(m_numValue == 0) {
                m_numValue = std::reduce(m_numValues.cbegin(), m_numValues.cend(), 0);
            }
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
        case(ValueType::Concat): {
            addTrackValue(QString::number(value));
            break;
        }
        case(ValueType::Average): {
            m_numValues.push_back(value);
            m_numValue = std::reduce(m_numValues.cbegin(), m_numValues.cend(), 0) / m_numValues.size();
            break;
        }
        case(ValueType::Total): {
            m_numValue += value;
            break;
        }
    }
}

void InfoItem::addTrackValue(int value)
{
    addTrackValue(static_cast<uint64_t>(value));
}

void InfoItem::addTrackValue(const QString& value)
{
    if(m_values.size() > 100 || m_values.contains(value) || value.isEmpty()) {
        return;
    }
    m_values.append(value);
    m_values.sort();
}

void InfoItem::addTrackValue(const QStringList& values)
{
    for(const auto& value : values) {
        addTrackValue(value);
    }
}

struct InfoModel::Private
{
    InfoModel* model;
    Core::Player::PlayerManager* playerManager;
    std::unordered_map<QString, InfoItem> nodes;

    Private(InfoModel* model, Core::Player::PlayerManager* playerManager)
        : model{model}
        , playerManager{playerManager}
    { }

    void reset()
    {
        model->resetRoot();
        nodes.clear();
    }

    InfoItem* getOrAddNode(const QString& name, ItemParent parent, InfoItem::ItemType type,
                           InfoItem::ValueType valueType = InfoItem::ValueType::Concat,
                           InfoItem::FormatFunc numFunc  = {})
    {
        if(name.isEmpty()) {
            return nullptr;
        }

        if(nodes.contains(name)) {
            return &nodes.at(name);
        }

        InfoItem* parentItem{nullptr};

        if(parent == ItemParent::Root) {
            parentItem = model->rootItem();
        }
        else {
            const QString parentKey = Utils::EnumHelper::toString(parent);
            if(nodes.contains(parentKey)) {
                parentItem = &nodes.at(parentKey);
            }
        }

        if(!parentItem) {
            return nullptr;
        }

        InfoItem* node = &nodes.emplace(name, InfoItem{type, name, parentItem, valueType, numFunc}).first->second;
        parentItem->appendChild(node);

        return node;
    }

    template <typename Arg>
    void checkAddEntryNode(const QString& name, InfoModel::ItemParent parent, Arg&& arg,
                           InfoItem::ValueType valueType = InfoItem::ValueType::Concat,
                           InfoItem::FormatFunc numFunc  = {})
    {
        if constexpr(std::is_same_v<Arg, QString> || std::is_same_v<Arg, QStringList>) {
            if(arg.isEmpty()) {
                return;
            }
        }
        auto* node = getOrAddNode(name, parent, InfoItem::ItemType::Entry, valueType, numFunc);
        node->addTrackValue(std::forward<Arg>(arg));
    }
};

InfoModel::InfoModel(Core::Player::PlayerManager* playerManager, QObject* parent)
    : TreeModel{parent}
    , p{std::make_unique<Private>(this, playerManager)}
{ }

InfoModel::~InfoModel() = default;

void InfoModel::resetModel(const Core::TrackList& tracks)
{
    Core::TrackList infoTracks{tracks};

    if(infoTracks.empty()) {
        Core::Track playingTrack = p->playerManager->currentTrack();
        if(playingTrack.isValid()) {
            infoTracks.push_back(std::move(playingTrack));
        }
        else {
            return;
        }
    }

    beginResetModel();
    p->reset();

    p->getOrAddNode("Metadata", ItemParent::Root, InfoItem::ItemType::Header);
    p->getOrAddNode("Location", ItemParent::Root, InfoItem::ItemType::Header);
    p->getOrAddNode("General", ItemParent::Root, InfoItem::ItemType::Header);

    const int total = static_cast<int>(tracks.size());
    for(const Core::Track& track : infoTracks) {
        p->checkAddEntryNode("Artist", ItemParent::Metadata, track.artists());
        p->checkAddEntryNode("Title", ItemParent::Metadata, track.title());
        p->checkAddEntryNode("Album", ItemParent::Metadata, track.album());
        p->checkAddEntryNode("Date", ItemParent::Metadata, track.date());
        p->checkAddEntryNode("Genre", ItemParent::Metadata, track.genres());
        p->checkAddEntryNode("Album Artist", ItemParent::Metadata, track.albumArtist());
        p->checkAddEntryNode("Track Number", ItemParent::Metadata, track.trackNumber());
        p->checkAddEntryNode(total > 1 ? "File Names" : "File Name", ItemParent::Location,
                             QFileInfo{track.filepath()}.fileName());
        p->checkAddEntryNode(total > 1 ? "File Paths" : "File Path", ItemParent::Location, track.filepath());
        p->checkAddEntryNode("Duration", ItemParent::General, track.duration(), InfoItem::ValueType::Total,
                             Utils::msToString);
        p->checkAddEntryNode(total > 1 ? "Avg. Bitrate" : "Bitrate", ItemParent::General, track.bitrate(),
                             InfoItem::ValueType::Average);
        p->checkAddEntryNode("Sample Rate", ItemParent::General, track.sampleRate());
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
            return "Name";
        case(1):
            return "Value";
    }
    return {};
}

int InfoModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 2;
}

QVariant InfoModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item                    = static_cast<InfoItem*>(index.internalPointer());
    const InfoItem::ItemType type = item->type();

    if(role == InfoItem::Role::Type) {
        return QVariant::fromValue<InfoItem::ItemType>(type);
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    switch(index.column()) {
        case(0):
            return item->name();
        case(1):
            return item->value();
    }

    return {};
}
} // namespace Fy::Gui::Widgets::Info
