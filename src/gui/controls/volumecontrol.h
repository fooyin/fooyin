/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include <gui/fywidget.h>

class QWheelEvent;

namespace Fooyin {
class ActionManager;
class SettingsManager;
class VolumeControlPrivate;

class VolumeControl : public FyWidget
{
    Q_OBJECT

public:
    enum Option : uint8_t
    {
        IconMode   = 1 << 0,
        SliderMode = 1 << 1,
        Tooltip    = 1 << 2,
        Default    = (IconMode | SliderMode),
    };
    Q_DECLARE_FLAGS(Options, Option)

    explicit VolumeControl(ActionManager* actionManager, SettingsManager* settings, QWidget* parent = nullptr);
    ~VolumeControl() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    std::unique_ptr<VolumeControlPrivate> p;
};
} // namespace Fooyin

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::VolumeControl::Options)
