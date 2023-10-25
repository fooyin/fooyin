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

#include "tageditoritem.h"

#include <utils/helpers.h>

using namespace Qt::Literals::StringLiterals;

namespace Fy::TagEditor {
TagEditorItem::TagEditorItem(QString title, TagEditorItem* parent)
    : TreeStatusItem{parent}
    , m_name{std::move(title)}
{ }

void TagEditorItem::reset()
{
    m_values.clear();
    m_value.clear();
    for(auto* const child : m_children) {
        child->reset();
    }
}

void TagEditorItem::addTrackValue(const QString& value)
{
    if(m_values.contains(value) || value.isEmpty()) {
        return;
    }
    m_values.append(value);
    m_values.sort();
    valuesToString();
}

void TagEditorItem::addTrackValue(const QStringList& values)
{
    for(const auto& trackValue : values) {
        addTrackValue(trackValue);
    }
}

void TagEditorItem::setValue(const QStringList& values)
{
    m_values = values;
    valuesToString();
}

void TagEditorItem::valuesToString()
{
    QString values;
    values = m_values.join("; "_L1);
    if(m_values.size() > 1) {
        values.prepend("<<multiple items>> ");
    }
    m_value = values;
}

QString TagEditorItem::name() const
{
    return m_name;
}

QString TagEditorItem::value() const
{
    return m_value;
}
} // namespace Fy::TagEditor
