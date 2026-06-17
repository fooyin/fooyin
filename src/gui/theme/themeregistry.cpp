/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
struct ThemeColours
{
    QRgb windowText;
    QRgb disabledText;
    QRgb button;
    QRgb light;
    QRgb midlight;
    QRgb dark;
    QRgb mid;
    QRgb base;
    QRgb window;
    QRgb shadow;
    QRgb highlight;
    QRgb disabledHighlight;
    QRgb highlightedText;
    QRgb link;
    QRgb linkVisited;
    QRgb alternateBase;
    QRgb toolTipBase;
    QRgb toolTipText;
    QRgb placeholderText;
    QRgb brightText{0xff0000};
};

FyTheme makeTheme(const QString& name, int id, int index, const ThemeColours& colours)
{
    FyTheme theme;
    theme.id    = id;
    theme.index = index;
    theme.name  = name;

    theme.colours = {{PaletteKey{QPalette::WindowText}, QColor(colours.windowText)},
                     {PaletteKey{QPalette::WindowText, QPalette::Disabled}, QColor(colours.disabledText)},
                     {PaletteKey{QPalette::Button}, QColor(colours.button)},
                     {PaletteKey{QPalette::Light}, QColor(colours.light)},
                     {PaletteKey{QPalette::Midlight}, QColor(colours.midlight)},
                     {PaletteKey{QPalette::Dark}, QColor(colours.dark)},
                     {PaletteKey{QPalette::Mid}, QColor(colours.mid)},
                     {PaletteKey{QPalette::Text}, QColor(colours.windowText)},
                     {PaletteKey{QPalette::Text, QPalette::Disabled}, QColor(colours.disabledText)},
                     {PaletteKey{QPalette::BrightText}, QColor(colours.brightText)},
                     {PaletteKey{QPalette::ButtonText}, QColor(colours.windowText)},
                     {PaletteKey{QPalette::ButtonText, QPalette::Disabled}, QColor(colours.disabledText)},
                     {PaletteKey{QPalette::Base}, QColor(colours.base)},
                     {PaletteKey{QPalette::Window}, QColor(colours.window)},
                     {PaletteKey{QPalette::Shadow}, QColor(colours.shadow)},
                     {PaletteKey{QPalette::Highlight}, QColor(colours.highlight)},
                     {PaletteKey{QPalette::Highlight, QPalette::Disabled}, QColor(colours.disabledHighlight)},
                     {PaletteKey{QPalette::HighlightedText}, QColor(colours.highlightedText)},
                     {PaletteKey{QPalette::HighlightedText, QPalette::Disabled}, QColor(colours.disabledText)},
                     {PaletteKey{QPalette::Link}, QColor(colours.link)},
                     {PaletteKey{QPalette::LinkVisited}, QColor(colours.linkVisited)},
                     {PaletteKey{QPalette::AlternateBase}, QColor(colours.alternateBase)},
                     {PaletteKey{QPalette::ToolTipBase}, QColor(colours.toolTipBase)},
                     {PaletteKey{QPalette::ToolTipText}, QColor(colours.toolTipText)},
                     {PaletteKey{QPalette::PlaceholderText}, QColor(colours.placeholderText)}};

    return theme;
}
} // namespace

ThemeRegistry::ThemeRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{u"Theme/SavedThemes"_s, settings, parent}
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
    addDefaultItem(makeTheme(u"Dark mode"_s, 0, 0,
                             {.windowText        = 0xffffff,
                              .disabledText      = 0x7f7f7f,
                              .button            = 0x353535,
                              .light             = 0x5a5a5a,
                              .midlight          = 0x4b4b4b,
                              .dark              = 0x232323,
                              .mid               = 0x2d2d2d,
                              .base              = 0x2a2a2a,
                              .window            = 0x353535,
                              .shadow            = 0x141414,
                              .highlight         = 0x1a5591,
                              .disabledHighlight = 0x505050,
                              .highlightedText   = 0xffffff,
                              .link              = 0x2a82da,
                              .linkVisited       = 0x822ada,
                              .alternateBase     = 0x424242,
                              .toolTipBase       = 0xffffff,
                              .toolTipText       = 0x353535,
                              .placeholderText   = 0x7f7f7f}));

    addDefaultItem(makeTheme(u"Black"_s, 5, 1,
                             {.windowText        = 0xffffff,
                              .disabledText      = 0x707070,
                              .button            = 0x000000,
                              .light             = 0x303030,
                              .midlight          = 0x202020,
                              .dark              = 0x000000,
                              .mid               = 0x101010,
                              .base              = 0x000000,
                              .window            = 0x000000,
                              .shadow            = 0x000000,
                              .highlight         = 0x0050a4,
                              .disabledHighlight = 0x303030,
                              .highlightedText   = 0xffffff,
                              .link              = 0x3297ff,
                              .linkVisited       = 0x9b6dff,
                              .alternateBase     = 0x090909,
                              .toolTipBase       = 0xffffff,
                              .toolTipText       = 0x000000,
                              .placeholderText   = 0x707070}));

    addDefaultItem(makeTheme(u"Twilight"_s, 1, 2,
                             {.windowText        = 0xe7edf4,
                              .disabledText      = 0x7c8793,
                              .button            = 0x29313d,
                              .light             = 0x536170,
                              .midlight          = 0x3f4a57,
                              .dark              = 0x141a22,
                              .mid               = 0x202834,
                              .base              = 0x1b222c,
                              .window            = 0x242c37,
                              .shadow            = 0x0b0f14,
                              .highlight         = 0x3d7a9f,
                              .disabledHighlight = 0x48505a,
                              .highlightedText   = 0xffffff,
                              .link              = 0x67b7dc,
                              .linkVisited       = 0xb28adf,
                              .alternateBase     = 0x303946,
                              .toolTipBase       = 0xf2f5f8,
                              .toolTipText       = 0x242c37,
                              .placeholderText   = 0x87929e}));

    addDefaultItem(makeTheme(u"Midnight Green"_s, 2, 3,
                             {.windowText        = 0xe5eee9,
                              .disabledText      = 0x7d8c84,
                              .button            = 0x26332d,
                              .light             = 0x53665d,
                              .midlight          = 0x405149,
                              .dark              = 0x131d18,
                              .mid               = 0x1e2924,
                              .base              = 0x19231e,
                              .window            = 0x223029,
                              .shadow            = 0x0a100d,
                              .highlight         = 0x2f7f63,
                              .disabledHighlight = 0x46524b,
                              .highlightedText   = 0xffffff,
                              .link              = 0x56c08f,
                              .linkVisited       = 0x98a6d8,
                              .alternateBase     = 0x2c3a33,
                              .toolTipBase       = 0xf1f6f3,
                              .toolTipText       = 0x223029,
                              .placeholderText   = 0x84938b}));

    addDefaultItem(makeTheme(u"Graphite Light"_s, 3, 4,
                             {.windowText        = 0x20242a,
                              .disabledText      = 0x8b929a,
                              .button            = 0xe2e5e9,
                              .light             = 0xffffff,
                              .midlight          = 0xf1f3f5,
                              .dark              = 0xaeb5bd,
                              .mid               = 0xc8cdd3,
                              .base              = 0xf8f9fb,
                              .window            = 0xe8ebef,
                              .shadow            = 0x7a828c,
                              .highlight         = 0x2f6f9f,
                              .disabledHighlight = 0xc8cdd3,
                              .highlightedText   = 0xffffff,
                              .link              = 0x1d6f9f,
                              .linkVisited       = 0x735aa6,
                              .alternateBase     = 0xeef1f4,
                              .toolTipBase       = 0x20242a,
                              .toolTipText       = 0xf8f9fb,
                              .placeholderText   = 0x7b838d}));

    addDefaultItem(makeTheme(u"Solar Light"_s, 4, 5,
                             {.windowText        = 0x2c2a24,
                              .disabledText      = 0x948d7d,
                              .button            = 0xe6dfcf,
                              .light             = 0xfffbef,
                              .midlight          = 0xf2ead8,
                              .dark              = 0xb9ae98,
                              .mid               = 0xd0c6b2,
                              .base              = 0xfffcf4,
                              .window            = 0xeee7d7,
                              .shadow            = 0x827867,
                              .highlight         = 0xb5692f,
                              .disabledHighlight = 0xd2c9b8,
                              .highlightedText   = 0xffffff,
                              .link              = 0x1d6f83,
                              .linkVisited       = 0x7b5da6,
                              .alternateBase     = 0xf5efdf,
                              .toolTipBase       = 0x2c2a24,
                              .toolTipText       = 0xfffcf4,
                              .placeholderText   = 0x8b846f}));

    addDefaultItem(makeTheme(u"Nord"_s, 6, 6,
                             {.windowText        = 0xd8dee9,
                              .disabledText      = 0x7f8999,
                              .button            = 0x3b4252,
                              .light             = 0x5e81ac,
                              .midlight          = 0x4c566a,
                              .dark              = 0x2e3440,
                              .mid               = 0x343b49,
                              .base              = 0x2e3440,
                              .window            = 0x3b4252,
                              .shadow            = 0x1f2430,
                              .highlight         = 0x5e81ac,
                              .disabledHighlight = 0x4c566a,
                              .highlightedText   = 0xeceff4,
                              .link              = 0x88c0d0,
                              .linkVisited       = 0xb48ead,
                              .alternateBase     = 0x434c5e,
                              .toolTipBase       = 0xeceff4,
                              .toolTipText       = 0x2e3440,
                              .placeholderText   = 0x8b96a9}));

    addDefaultItem(makeTheme(u"Dracula"_s, 7, 7,
                             {.windowText        = 0xf8f8f2,
                              .disabledText      = 0x8b8b8b,
                              .button            = 0x343746,
                              .light             = 0x6272a4,
                              .midlight          = 0x44475a,
                              .dark              = 0x1e2029,
                              .mid               = 0x282a36,
                              .base              = 0x282a36,
                              .window            = 0x343746,
                              .shadow            = 0x191a21,
                              .highlight         = 0xbd93f9,
                              .disabledHighlight = 0x4b4d5f,
                              .highlightedText   = 0x282a36,
                              .link              = 0x8be9fd,
                              .linkVisited       = 0xff79c6,
                              .alternateBase     = 0x3a3d4f,
                              .toolTipBase       = 0xf8f8f2,
                              .toolTipText       = 0x282a36,
                              .placeholderText   = 0x9a9a9a}));

    addDefaultItem(makeTheme(u"Gruvbox Dark"_s, 8, 8,
                             {.windowText        = 0xebdbb2,
                              .disabledText      = 0x928374,
                              .button            = 0x3c3836,
                              .light             = 0x665c54,
                              .midlight          = 0x504945,
                              .dark              = 0x1d2021,
                              .mid               = 0x282828,
                              .base              = 0x282828,
                              .window            = 0x32302f,
                              .shadow            = 0x1d2021,
                              .highlight         = 0xb57614,
                              .disabledHighlight = 0x504945,
                              .highlightedText   = 0xfbf1c7,
                              .link              = 0x83a598,
                              .linkVisited       = 0xd3869b,
                              .alternateBase     = 0x3c3836,
                              .toolTipBase       = 0xfbf1c7,
                              .toolTipText       = 0x282828,
                              .placeholderText   = 0x928374}));

    addDefaultItem(makeTheme(u"Solarized Dark"_s, 9, 9,
                             {.windowText        = 0x93a1a1,
                              .disabledText      = 0x586e75,
                              .button            = 0x073642,
                              .light             = 0x586e75,
                              .midlight          = 0x28515b,
                              .dark              = 0x002b36,
                              .mid               = 0x073642,
                              .base              = 0x002b36,
                              .window            = 0x073642,
                              .shadow            = 0x001f27,
                              .highlight         = 0x268bd2,
                              .disabledHighlight = 0x586e75,
                              .highlightedText   = 0xfdf6e3,
                              .link              = 0x2aa198,
                              .linkVisited       = 0x6c71c4,
                              .alternateBase     = 0x0c3f4c,
                              .toolTipBase       = 0xfdf6e3,
                              .toolTipText       = 0x002b36,
                              .placeholderText   = 0x657b83}));

    addDefaultItem(makeTheme(u"Catppuccin Mocha"_s, 10, 10,
                             {.windowText        = 0xcdd6f4,
                              .disabledText      = 0x7f849c,
                              .button            = 0x313244,
                              .light             = 0x585b70,
                              .midlight          = 0x45475a,
                              .dark              = 0x11111b,
                              .mid               = 0x1e1e2e,
                              .base              = 0x1e1e2e,
                              .window            = 0x181825,
                              .shadow            = 0x0b0b12,
                              .highlight         = 0x89b4fa,
                              .disabledHighlight = 0x45475a,
                              .highlightedText   = 0x1e1e2e,
                              .link              = 0x89dceb,
                              .linkVisited       = 0xcba6f7,
                              .alternateBase     = 0x313244,
                              .toolTipBase       = 0xcdd6f4,
                              .toolTipText       = 0x1e1e2e,
                              .placeholderText   = 0x7f849c}));
}
} // namespace Fooyin
