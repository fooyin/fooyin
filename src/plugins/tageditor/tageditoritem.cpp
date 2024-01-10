/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include <utils/helpers.h>

using namespace Qt::Literals::StringLiterals;

namespace Fooyin::TagEditor {
TagEditorItem::TagEditorItem()
    : TagEditorItem{u""_s, nullptr, true}
{ }

TagEditorItem::TagEditorItem(QString title, TagEditorItem* parent, bool isDefault)
    : TreeStatusItem{parent}
    , m_isDefault{isDefault}
    , m_name{std::move(title)}
    , m_trackCount{0}
{ }

QString TagEditorItem::name() const
{
    return m_name;
}

QString TagEditorItem::value() const
{
    if(m_value.isEmpty()) {
        QString values;

        QStringList nonEmptyValues{m_values};
        nonEmptyValues.removeAll(u""_s);

        values = nonEmptyValues.join("; "_L1);
        if(m_trackCount > 1 && m_values.size() > 1) {
            values.prepend("<<multiple items>> ");
        }
        m_value = values;
    }
    return m_value;
}

bool TagEditorItem::isDefault() const
{
    return m_isDefault;
}

void TagEditorItem::addTrackValue(const QString& value)
{
    if(m_values.contains(value)) {
        return;
    }
    m_values.append(value);
    m_values.sort();

    m_trackCount++;
}

void TagEditorItem::addTrackValue(const QStringList& values)
{
    for(const auto& trackValue : values) {
        if(m_values.contains(trackValue)) {
            return;
        }
        m_values.append(trackValue);
        m_values.sort();
    }
    m_trackCount++;
}

void TagEditorItem::setValue(const QStringList& values)
{
    m_values = values;
    m_value  = {};
}

void TagEditorItem::setTitle(const QString& title)
{
    m_name = title;
}
} // namespace Fooyin::TagEditor
