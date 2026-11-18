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

#include "themepreset.h"

#include <gui/theme/themeregistry.h>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
QString presetKey(ThemePart part)
{
    switch(part) {
        case ThemePart::Colours:
            return u"Theme/ColourPresets"_s;
        case ThemePart::Fonts:
            return u"Theme/FontPresets"_s;
        case ThemePart::Appearance:
            return u"Theme/AppearancePresets"_s;
    }
    return {};
}
} // namespace

FyTheme extractThemePart(const FyTheme& theme, ThemePart part)
{
    FyTheme preset;
    preset.id        = theme.id;
    preset.name      = theme.name;
    preset.index     = theme.index;
    preset.isDefault = theme.isDefault;
    applyThemePart(preset, theme, part);
    return preset;
}

void applyThemePart(FyTheme& theme, const FyTheme& preset, ThemePart part)
{
    switch(part) {
        case ThemePart::Colours:
            theme.colours = preset.colours;
            break;
        case ThemePart::Fonts:
            theme.fonts = preset.fonts;
            break;
        case ThemePart::Appearance:
            theme.appearance = preset.appearance;
            break;
    }
}

bool themePartMatches(const FyTheme& first, const FyTheme& second, ThemePart part)
{
    switch(part) {
        case ThemePart::Colours:
            return first.colours == second.colours;
        case ThemePart::Fonts:
            return first.fonts == second.fonts;
        case ThemePart::Appearance:
            return first.appearance == second.appearance;
    }
    return false;
}

ThemePresetRegistry::ThemePresetRegistry(ThemePart part, ThemeRegistry* themes, SettingsManager* settings,
                                         QObject* parent)
    : ItemRegistry{presetKey(part), settings, parent}
    , m_part{part}
    , m_themes{themes}
{
    loadItems();
}

void ThemePresetRegistry::loadDefaults()
{
    if(m_part != ThemePart::Colours) {
        return;
    }

    const auto items = m_themes->items();
    for(const auto& theme : items) {
        if(theme.isDefault && !theme.colours.isEmpty()) {
            addDefaultItem(extractThemePart(theme, m_part));
        }
    }
}
} // namespace Fooyin
