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

#include <gui/fywidget.h>

namespace Fooyin {
class ActionManager;
class SettingsManager;
class ToolButton;

namespace Scrobbler {
class ScrobblerToggle : public FyWidget
{
    Q_OBJECT

public:
    explicit ScrobblerToggle(ActionManager* actionManager, SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

private:
    void updateButtonStyle() const;
    void scrobblingToggled(bool enabled);

    ActionManager* m_actionManager;
    SettingsManager* m_settings;
    ToolButton* m_scrobbleButton;
    QColor m_iconColour;
};
} // namespace Scrobbler
} // namespace Fooyin
