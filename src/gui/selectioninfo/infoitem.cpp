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
#include <utility>

using namespace Qt::StringLiterals;

namespace {
double getTotalFloat(const QStringList& values)
{
    double total{0.0};

    for(const QString& str : values) {
        bool ok{false};
        const double value = str.toDouble(&ok);
        if(ok) {
            total += value;
        }
    }

    return total;
};

uint64_t getTotal(const QStringList& values)
{
    uint64_t total{0};

    for(const QString& str : values) {
        bool ok{false};
        const uint64_t value = str.toULongLong(&ok);
        if(ok) {
            total += value;
        }
    }

    return total;
};

double getMaxFloat(const QStringList& values)
{
    double max{0.0};

    for(const QString& str : values) {
        bool ok{false};
        const double value = str.toDouble(&ok);
        if(ok) {
            max = std::max(max, value);
        }
    }

    return max;
};

uint64_t getMax(const QStringList& values)
{
    uint64_t max{0};

    for(const QString& str : values) {
        bool ok{false};
        const uint64_t value = str.toULongLong(&ok);
        if(ok) {
            max = std::max(max, value);
        }
    }

    return max;
};

QString formatPercentage(const QStringList& values)
{
    std::map<QString, int> valueCounts;
    for(const auto& value : values) {
        ++valueCounts[value];
    }

    if(valueCounts.size() == 1) {
        const auto val = valueCounts.cbegin();
        return val->first;
    }

    int totalCount{0};
    for(const auto& [_, count] : valueCounts) {
        totalCount += count;
    }

    std::map<QString, double> ratios;
    for(const auto& [key, count] : valueCounts) {
        ratios[key] = (static_cast<double>(count) / totalCount) * 100;
    }

    QStringList formattedList;
    for(const auto& [key, ratio] : ratios) {
        if(!key.isEmpty()) {
            formattedList.append(u"%1 (%2%)"_s.arg(key, QString::number(ratio, 'f', 1)));
        }
    }
    return formattedList.join("; "_L1);
}

QString joinValues(const QStringList& values)
{
    QStringList list;
    for(const QString& value : values) {
        if(!value.isEmpty()) {
            list.append(value);
        }
    }
    return list.join("; "_L1);
}

QString calculateAverage(const QStringList& values, const Fooyin::InfoItem::FormatFunc& format, bool isFloat)
{
    const double average
        = (isFloat ? getTotalFloat(values) : static_cast<double>(getTotal(values))) / static_cast<int>(values.size());
    return std::visit(
        [average](const auto& func) -> QString {
            if(func) {
                return func(average);
            }
            return QString::number(average);
        },
        format);
}

QString calculateTotal(const QStringList& values, const Fooyin::InfoItem::FormatFunc& format, bool isFloat)
{
    return std::visit(
        [&values, isFloat](const auto& func) -> QString {
            if(func) {
                if(isFloat) {
                    return func(getTotalFloat(values));
                }
                return func(getTotal(values));
            }
            if(isFloat) {
                return QString::number(getTotalFloat(values));
            }
            return QString::number(getTotal(values));
        },
        format);
}

QString calculateMax(const QStringList& values, const Fooyin::InfoItem::FormatFunc& format, bool isFloat)
{
    return std::visit(
        [&values, isFloat](const auto& func) -> QString {
            if(func) {
                if(isFloat) {
                    return func(getMaxFloat(values));
                }
                return func(getMax(values));
            }
            if(isFloat) {
                return QString::number(getMaxFloat(values));
            }
            return QString::number(getMax(values));
        },
        format);
}
} // namespace

namespace Fooyin {
InfoItem::InfoItem()
    : InfoItem{Header, {}, nullptr, ValueType::Concat, {}}
{ }

InfoItem::InfoItem(ItemType type, QString name, InfoItem* parent, ValueType valueType)
    : InfoItem{type, std::move(name), parent, valueType, {}}
{ }

InfoItem::InfoItem(ItemType type, QString name, InfoItem* parent, ValueType valueType, const FormatFunc& formatFunc)
    : TreeItem{parent}
    , m_type{type}
    , m_valueType{valueType}
    , m_isFloat{false}
    , m_name{std::move(name)}
    , m_formatNum{formatFunc}
{ }

InfoItem::InfoItem(const InfoItem& other) = default;

bool InfoItem::operator<(const InfoItem& other) const
{
    const bool leftIsCustom  = m_name.startsWith(u'<');
    const bool rightIsCustom = other.m_name.startsWith(u'<');

    if(leftIsCustom && !rightIsCustom) {
        return false;
    }
    if(!leftIsCustom && rightIsCustom) {
        return true;
    }

    const auto cmp = QString::localeAwareCompare(name(), other.name());
    if(cmp == 0) {
        return false;
    }

    if(m_type == Entry && leftIsCustom && rightIsCustom) {
        return cmp < 0;
    }

    return false;
}

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
                m_value = formatPercentage(m_values);
            }
            return m_value;
        }
        case(ValueType::Average): {
            if(m_value.isEmpty() && !m_values.empty()) {
                m_value = calculateAverage(m_values, m_formatNum, m_isFloat);
            }
            return m_value;
        }
        case(ValueType::Total): {
            if(m_value.isEmpty() && !m_values.empty()) {
                m_value = calculateTotal(m_values, m_formatNum, m_isFloat);
            }
            return m_value;
        }
        case(ValueType::Max): {
            if(m_value.isEmpty() && !m_values.empty()) {
                m_value = calculateMax(m_values, m_formatNum, m_isFloat);
            }
            return m_value;
        }
    }
    return m_value;
}

void InfoItem::setIsFloat(bool isFloat)
{
    m_isFloat = isFloat;
}

void InfoItem::addTrackValue(const QString& value)
{
    if(m_valueType != ValueType::Concat) {
        m_values.append(value);
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
