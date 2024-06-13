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

#include "infopopulator.h"

#include <core/track.h>

#include <QCollator>
#include <QFileInfo>
#include <QFont>
#include <QThread>

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
        if(key != u"-1") {
            formattedList.append(QStringLiteral("%1 (%2%)").arg(key, QString::number(value, 'f', 1)));
        }
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
            m_numValue = std::max(m_numValue, static_cast<uint64_t>(value));
            break;
        case(ValueType::Percentage):
            m_percentValues[QString::number(value)]++;
            break;
    }
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

    QThread m_populatorThread;
    InfoPopulator m_populator;
    std::unordered_map<QString, InfoItem> m_nodes;

    Options m_options{Default};
    QFont m_headerFont;

    explicit Private(InfoModel* self_)
        : self{self_}
    {
        m_populator.moveToThread(&m_populatorThread);

        m_headerFont.setPointSize(m_headerFont.pointSize() + HeaderFontDelta);
    }

    void populate(InfoData data)
    {
        self->beginResetModel();
        self->resetRoot();
        m_nodes.clear();

        m_nodes = std::move(data.nodes);

        for(const auto& [parentKey, children] : data.parents) {
            InfoItem* parent{nullptr};

            if(parentKey == u"Root") {
                parent = self->rootItem();
            }
            else if(m_nodes.contains(parentKey)) {
                parent = &m_nodes.at(parentKey);
            }

            if(parent) {
                for(const auto& child : children) {
                    if(m_nodes.contains(child)) {
                        parent->appendChild(&m_nodes.at(child));
                    }
                }
            }
        }

        self->endResetModel();
    }
};

InfoModel::InfoModel(QObject* parent)
    : TreeModel{parent}
    , p{std::make_unique<Private>(this)}
{
    QObject::connect(&p->m_populator, &InfoPopulator::populated, this,
                     [this](InfoData data) { p->populate(std::move(data)); });

    QObject::connect(&p->m_populator, &Worker::finished, this, [this]() {
        p->m_populator.stopThread();
        p->m_populatorThread.quit();
    });
}

InfoModel::~InfoModel() = default;

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
            return p->m_headerFont;
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

InfoModel::Options InfoModel::options() const
{
    return p->m_options;
}

void InfoModel::setOption(Option option, bool enabled)
{
    if(enabled) {
        p->m_options |= option;
    }
    else {
        p->m_options &= ~option;
    }
}

void InfoModel::setOptions(Options options)
{
    p->m_options = options;
}

void InfoModel::resetModel(const TrackList& tracks, const Track& playingTrack)
{
    if(p->m_populatorThread.isRunning()) {
        p->m_populator.stopThread();
    }
    else {
        p->m_populatorThread.start();
    }

    TrackList infoTracks{tracks};

    if(infoTracks.empty() && playingTrack.isValid()) {
        infoTracks.push_back(playingTrack);
    }

    QMetaObject::invokeMethod(&p->m_populator, [this, infoTracks] { p->m_populator.run(p->m_options, infoTracks); });
}
} // namespace Fooyin

#include "moc_infomodel.cpp"
