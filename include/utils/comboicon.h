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

#pragma once

#include "fyutils_export.h"

#include <QWidget>

namespace Fooyin {
class FYUTILS_EXPORT ComboIcon : public QWidget
{
    Q_OBJECT

public:
    enum Attribute
    {
        HasActiveIcon   = 1 << 0,
        HasDisabledIcon = 1 << 1,
        AutoShift       = 1 << 2,
        Active          = 1 << 3,
        Enabled         = 1 << 4,
    };
    Q_DECLARE_FLAGS(Attributes, Attribute)

    ComboIcon(const QString& path, Attributes attributes, QWidget* parent = nullptr);
    explicit ComboIcon(const QString& path, QWidget* parent = nullptr);
    ~ComboIcon() override;

    void addIcon(const QString& path, const QPixmap& icon);
    void addIcon(const QString& path);

    void setIcon(const QString& path, bool active = false);
    void setIconEnabled(bool enable = true);

    void updateIcons();

signals:
    void clicked(const QString& path = {});
    void entered();
    void mouseLeft();

protected:
    void changeEvent(QEvent* event) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::ComboIcon::Attributes)
