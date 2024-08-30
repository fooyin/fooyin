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

#include <gui/theme/themeregistry.h>

namespace Fooyin {
ThemeRegistry::ThemeRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{QStringLiteral("Theme/SavedThemes"), settings, parent}
{
    loadItems();
}

std::map<QString, QString> ThemeRegistry::fontEntries() const
{
    return m_fontEntries;
}

void ThemeRegistry::registerFontEntry(const QString& title, const QString& className)
{
    if(!m_fontEntries.contains(className)) {
        m_fontEntries.emplace(className, title);
    }
}

void ThemeRegistry::loadDefaults()
{
    FyTheme theme;
    theme.name = tr("Dark mode");

    theme.colours = {{PaletteKey{QPalette::WindowText}, QColor(0xffffff)},
                     {PaletteKey{QPalette::WindowText, QPalette::Disabled}, QColor(0x7f7f7f)},
                     {PaletteKey{QPalette::Button}, QColor(0x353535)},
                     {PaletteKey{QPalette::Light}, QColor(0x5a5a5a)},
                     {PaletteKey{QPalette::Midlight}, QColor(0x4b4b4b)},
                     {PaletteKey{QPalette::Dark}, QColor(0x232323)},
                     {PaletteKey{QPalette::Mid}, QColor(0x2d2d2d)},
                     {PaletteKey{QPalette::Text}, QColor(0xffffff)},
                     {PaletteKey{QPalette::Text, QPalette::Disabled}, QColor(0x7f7f7f)},
                     {PaletteKey{QPalette::BrightText}, QColor(0xff0000)},
                     {PaletteKey{QPalette::ButtonText}, QColor(0xffffff)},
                     {PaletteKey{QPalette::ButtonText, QPalette::Disabled}, QColor(0x7f7f7f)},
                     {PaletteKey{QPalette::Base}, QColor(0x2a2a2a)},
                     {PaletteKey{QPalette::Window}, QColor(0x353535)},
                     {PaletteKey{QPalette::Shadow}, QColor(0x141414)},
                     {PaletteKey{QPalette::Highlight}, QColor(0x1a5591)},
                     {PaletteKey{QPalette::Highlight, QPalette::Disabled}, QColor(0x505050)},
                     {PaletteKey{QPalette::HighlightedText}, QColor(0xffffff)},
                     {PaletteKey{QPalette::HighlightedText, QPalette::Disabled}, QColor(0x7f7f7f)},
                     {PaletteKey{QPalette::Link}, QColor(0x2a82da)},
                     {PaletteKey{QPalette::LinkVisited}, QColor(0x822ada)},
                     {PaletteKey{QPalette::AlternateBase}, QColor(0x424242)},
                     {PaletteKey{QPalette::ToolTipBase}, QColor(0xffffff)},
                     {PaletteKey{QPalette::ToolTipText}, QColor(0x353535)},
                     {PaletteKey{QPalette::PlaceholderText}, QColor(0x7f7f7f)}};

    addDefaultItem(theme);
}
} // namespace Fooyin
