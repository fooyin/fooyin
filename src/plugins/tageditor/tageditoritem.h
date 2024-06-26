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

#pragma once

#include <utils/treestatusitem.h>

#include <QList>

namespace Fooyin::TagEditor {
class TagEditorItem : public TreeStatusItem<TagEditorItem>
{
public:
    enum Role
    {
        IsDefault = Qt::UserRole,
    };

    TagEditorItem();
    TagEditorItem(QString title, TagEditorItem* parent, bool isDefault);

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString changedTitle() const;
    [[nodiscard]] QString value() const;
    [[nodiscard]] QString changedValue() const;
    [[nodiscard]] bool isDefault() const;
    [[nodiscard]] int trackCount() const;

    void addTrack();
    void addTrackValue(const QString& value);
    void addTrackValue(const QStringList& values);
    bool setValue(const QStringList& values);
    bool setTitle(const QString& title);

    void sortCustomTags();
    void applyChanges();
    void reset();

private:
    bool m_isDefault;
    QString m_title;
    QString m_changedTitle;
    QStringList m_values;
    QStringList m_changedValues;
    mutable QString m_value;
    mutable QString m_changedValue;
    int m_trackCount;
};
} // namespace Fooyin::TagEditor
