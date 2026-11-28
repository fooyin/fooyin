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

#include <gui/theme/fytheme.h>
#include <utils/itemregistry.h>

namespace Fooyin {
class ThemeRegistry;

enum class ThemePart : uint8_t
{
    Colours = 0,
    Fonts,
    Appearance,
};

FyTheme extractThemePart(const FyTheme& theme, ThemePart part);
void applyThemePart(FyTheme& theme, const FyTheme& preset, ThemePart part);
bool themePartMatches(const FyTheme& first, const FyTheme& second, ThemePart part);

class ThemePresetRegistry : public ItemRegistry<FyTheme>
{
public:
    ThemePresetRegistry(ThemePart part, ThemeRegistry* themes, SettingsManager* settings, QObject* parent = nullptr);

protected:
    void loadDefaults() override;

private:
    ThemePart m_part;
    ThemeRegistry* m_themes;
};
} // namespace Fooyin
