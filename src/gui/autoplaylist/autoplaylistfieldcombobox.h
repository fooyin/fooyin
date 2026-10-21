/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "fygui_export.h"

#include "autoplaylistquery.h"

#include <QComboBox>

namespace Fooyin {
class FYGUI_EXPORT AutoPlaylistFieldComboBox : public QComboBox
{
    Q_OBJECT

public:
    explicit AutoPlaylistFieldComboBox(QWidget* parent);

    [[nodiscard]] QString field() const;
    [[nodiscard]] AutoPlaylistFieldSource source() const;

    void setField(const QString& name, AutoPlaylistFieldSource source);

Q_SIGNALS:
    void fieldChanged();

private:
    QString m_previousField;
    AutoPlaylistFieldSource m_previousSource;
};
} // namespace Fooyin
