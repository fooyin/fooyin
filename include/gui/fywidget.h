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

#include "fygui_export.h"

#include <utils/id.h>

#include <QWidget>

namespace Fooyin {
class ActionContainer;

class FYGUI_EXPORT FyWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FyWidget(QWidget* parent);

    [[nodiscard]] Id id() const;
    [[nodiscard]] virtual QString name() const = 0;
    [[nodiscard]] virtual QString layoutName() const;

    [[nodiscard]] FyWidget* findParent() const;
    [[nodiscard]] QRect widgetGeometry() const;

    void saveLayout(QJsonArray& layout);
    void loadLayout(const QJsonObject& layout);

    virtual void layoutEditingMenu(ActionContainer* menu);
    virtual void saveLayoutData(QJsonObject& layout);
    virtual void loadLayoutData(const QJsonObject& layout);
    virtual void finalise();

private:
    Id m_id;
};
} // namespace Fooyin
