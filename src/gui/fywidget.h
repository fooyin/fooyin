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

#include <utils/id.h>

#include <QJsonArray>
#include <QWidget>

namespace Fy {

namespace Utils {
class ActionContainer;
}

namespace Gui::Widgets {
class FyWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FyWidget(QWidget* parent);

    [[nodiscard]] Utils::Id id() const;
    [[nodiscard]] virtual QString name() const = 0;
    [[nodiscard]] virtual QString layoutName() const
    {
        return name();
    };

    [[nodiscard]] FyWidget* findParent();

    virtual void layoutEditingMenu(Utils::ActionContainer* menu);
    virtual void saveLayout(QJsonArray& array);
    virtual void loadLayout(const QJsonObject& object);

private:
    Utils::Id m_id;
};
} // namespace Gui::Widgets
} // namespace Fy
