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

#include <QIcon>
#include <QLabel>

namespace Fooyin {
class FYGUI_EXPORT HintLabel : public QLabel
{
    Q_OBJECT

public:
    enum class Type : uint8_t
    {
        Information = 0,
        Warning,
    };

    explicit HintLabel(QWidget* parent = nullptr);
    explicit HintLabel(const QString& text, QWidget* parent = nullptr);
    HintLabel(const QString& text, Type type, QWidget* parent = nullptr);

    void setType(Type type);

protected:
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void updateMargins();

    QIcon m_icon;
};
} // namespace Fooyin
