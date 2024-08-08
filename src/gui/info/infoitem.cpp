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

#include "infoitem.h"

#include <QCollator>

#include <algorithm>

namespace {
QString formatPercentage(const std::map<QString, int>& values)
{
    if(values.size() == 1) {
        const auto val = values.cbegin();
        if(val->first == u"-1") {
            return {};
        }
        return val->first;
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

QString joinValues(const QStringList& values)
{
    QStringList list;
    for(const QString& value : values) {
        list.append(value);
    }

    return list.join(u"; ");
}
} // namespace

namespace Fooyin {
InfoItem::InfoItem()
    : InfoItem{Header, {}, nullptr, ValueType::Concat, {}}
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
                m_value = joinValues(m_values);
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

    if(m_values.contains(value)) {
        return;
    }

    if(m_values.size() > 40) {
        return;
    }

    m_values.append(value);
}

void InfoItem::addTrackValue(const QStringList& values)
{
    for(const auto& strValue : values) {
        addTrackValue(strValue);
    }
}
} // namespace Fooyin
