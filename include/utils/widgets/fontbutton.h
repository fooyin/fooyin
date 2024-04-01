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

#pragma once

#include "fyutils_export.h"

#include <QFont>
#include <QPushButton>

namespace Fooyin {
class FYUTILS_EXPORT FontButton : public QPushButton
{
    Q_OBJECT

public:
    explicit FontButton(QWidget* parent = nullptr);
    explicit FontButton(const QString& text, QWidget* parent = nullptr);
    explicit FontButton(const QIcon& icon, const QString& text, QWidget* parent = nullptr);

    [[nodiscard]] QFont font() const;
    [[nodiscard]] bool fontChanged() const;
    void setFont(const QFont& font);
    void setFont(const QString& font);

private:
    void pickFont();

    QFont m_font;
    bool m_changed;
};
} // namespace Fooyin
