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

#pragma once

#include <utils/treeitem.h>

#include <QList>

namespace Fy::TagEditor {
class TagEditorItem : public Utils::TreeItem<TagEditorItem>
{
public:
    explicit TagEditorItem(QString title = {});

    void reset();

    void addTrackValue(const QString& value);
    void addTrackValue(const QStringList& values);
    void setValue(const QStringList& values);

    void valuesToString();

    [[nodiscard]] QString name() const;
    [[nodiscard]] QString value() const;

private:
    QString m_name;
    QStringList m_values;
    QString m_value;
};
} // namespace Fy::TagEditor
