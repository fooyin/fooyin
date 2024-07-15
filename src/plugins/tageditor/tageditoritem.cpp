/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "tageditoritem.h"

#include <QCollator>

constexpr auto CharLimit = 2000;

namespace {
bool withinCharLimit(const QStringList& strings)
{
    const int currentLength = std::accumulate(strings.cbegin(), strings.cend(), 0,
                                              [](int sum, const QString& str) { return sum + str.length(); });
    return currentLength <= CharLimit;
}

void sortList(QStringList& list)
{
    QCollator collator;
    collator.setNumericMode(true);

    std::ranges::sort(list, collator);
}
} // namespace

namespace Fooyin::TagEditor {
TagEditorItem::TagEditorItem()
    : TagEditorItem{{}, nullptr, true}
{ }

TagEditorItem::TagEditorItem(QString title, TagEditorItem* parent, bool isDefault)
    : TreeStatusItem{parent}
    , m_isDefault{isDefault}
    , m_title{std::move(title)}
    , m_titleChanged{false}
    , m_valueChanged{false}
    , m_trackCount{0}
    , m_multipleValues{false}
    , m_splitTrackValues{false}
{ }

QString TagEditorItem::title() const
{
    return m_title;
}

QString TagEditorItem::changedTitle() const
{
    return m_changedTitle;
}

bool TagEditorItem::titleChanged() const
{
    return m_titleChanged;
}

QString TagEditorItem::displayValue() const
{
    if(m_value.isEmpty()) {
        QStringList nonEmptyValues{m_values};
        nonEmptyValues.removeAll(QString{});
        sortList(nonEmptyValues);
        m_value = nonEmptyValues.join(u"; ");
    }

    if(m_trackCount > 1 && m_multipleValues && m_values.size() > 1) {
        return QStringLiteral("<<multiple values>> ") + value();
    }

    return m_value;
}

QString TagEditorItem::value() const
{
    if(m_value.isEmpty()) {
        QStringList nonEmptyValues{m_values};
        nonEmptyValues.removeAll(QString{});
        sortList(nonEmptyValues);
        m_value = nonEmptyValues.join(u"; ");
    }

    return m_value;
}

QString TagEditorItem::changedDisplayValue() const
{
    if(m_trackCount > 1 && m_multipleValues && m_changedValues.size() > 1) {
        return QStringLiteral("<<multiple values>> ") + changedValue();
    }

    return changedValue();
}

QString TagEditorItem::changedValue() const
{
    if(m_changedValue.isEmpty()) {
        QStringList nonEmptyValues{m_changedValues};
        nonEmptyValues.removeAll(QString{});
        sortList(nonEmptyValues);
        m_changedValue = nonEmptyValues.join(u"; ");
    }

    return m_changedValue;
}

bool TagEditorItem::valueChanged() const
{
    return m_valueChanged;
}

bool TagEditorItem::isDefault() const
{
    return m_isDefault;
}

int TagEditorItem::trackCount() const
{
    return m_trackCount;
}

bool TagEditorItem::splitTrackValues() const
{
    return m_splitTrackValues;
}

bool TagEditorItem::multipleValues() const
{
    return m_multipleValues;
}

void TagEditorItem::addTrack()
{
    m_trackCount++;
}

void TagEditorItem::addTrackValue(const QString& value)
{
    if(!m_values.contains(value)) {
        if(m_trackCount == 0 || withinCharLimit(m_values)) {
            m_values.append(value);
            sortList(m_values);
        }

        m_multipleValues = m_trackCount >= 1;
    }

    m_trackCount++;
}

void TagEditorItem::addTrackValue(const QStringList& values)
{
    if(values.empty()) {
        return;
    }

    for(const auto& trackValue : values) {
        if(m_values.contains(trackValue)) {
            continue;
        }

        if(m_trackCount == 0 || withinCharLimit(m_values)) {
            m_values.append(trackValue);
            sortList(m_values);
        }

        m_multipleValues = m_trackCount >= 1;
    }

    m_trackCount++;
}

bool TagEditorItem::setValue(int value)
{
    return setValue(QString::number(value));
}

bool TagEditorItem::setValue(const QString& value)
{
    return setValue(QStringList{value});
}

bool TagEditorItem::setValue(const QStringList& values)
{
    if(m_values == values) {
        if(status() == None) {
            return false;
        }
        if(status() == Changed) {
            m_valueChanged = false;
            m_changedValues.clear();
            m_changedValue.clear();
            setStatus(None);
            return false;
        }
    }

    m_changedValues = values;
    m_changedValue.clear();
    m_valueChanged = true;

    if(status() != TagEditorItem::Added) {
        setStatus(TagEditorItem::Changed);
    }

    return true;
}

bool TagEditorItem::setTitle(const QString& title)
{
    if(m_title == title) {
        if(status() == None) {
            return false;
        }
        if(status() == Changed) {
            m_titleChanged = false;
            m_changedTitle.clear();
            setStatus(None);
            return false;
        }
    }

    m_changedTitle = title;
    m_titleChanged = true;

    if(status() != TagEditorItem::Added) {
        setStatus(TagEditorItem::Changed);
    }

    return true;
}

void TagEditorItem::setMultipleValues(bool multiple)
{
    m_multipleValues = multiple;
}

void TagEditorItem::setSplitTrackValues(bool enabled)
{
    m_splitTrackValues = enabled;
}

void TagEditorItem::sortCustomTags()
{
    if(m_children.empty()) {
        return;
    }

    auto sortedChildren{m_children};

    QCollator collator;
    collator.setNumericMode(true);

    auto defaultEnd
        = std::ranges::find_if(sortedChildren, [](const TagEditorItem* item) { return item && !item->isDefault(); });

    std::ranges::sort(defaultEnd, sortedChildren.end(), [collator](const TagEditorItem* lhs, const TagEditorItem* rhs) {
        return collator.compare(lhs->title(), rhs->title()) < 0;
    });

    m_children = sortedChildren;
}

void TagEditorItem::applyChanges()
{
    if(!m_isDefault || !m_changedTitle.isEmpty()) {
        m_title = m_changedTitle;
    }
    m_values = m_changedValues;
    m_value  = m_changedValue;
    reset();
}

void TagEditorItem::reset()
{
    m_titleChanged     = false;
    m_valueChanged     = false;
    m_splitTrackValues = false;
    m_changedTitle.clear();
    m_changedValue.clear();
    m_changedValues.clear();
    setStatus(None);
}
} // namespace Fooyin::TagEditor
