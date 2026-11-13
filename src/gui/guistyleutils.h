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

#include <gui/guisettings.h>

#include <initializer_list>
#include <optional>

namespace Fooyin::Gui {
struct StyleChanges
{
    bool palette{false};
    bool fonts{false};
};

class StyleChangeTracker
{
public:
    explicit StyleChangeTracker(const ResolvedAppStyle& style = {})
        : m_style{style.revision != 0 ? std::optional{style} : std::nullopt}
    { }

    StyleChanges update(const ResolvedAppStyle& style, std::initializer_list<QString> fontClasses = {})
    {
        if(style.revision == 0) {
            return {};
        }

        const bool initial = !m_style;
        const bool fontsChanged
            = initial
           || (fontClasses.size() == 0
                   ? style.defaultFont != m_style->defaultFont || style.classFonts != m_style->classFonts
                   : std::ranges::any_of(fontClasses, [&](const QString& className) {
                         return style.font(className) != m_style->font(className);
                     }));
        const StyleChanges changes{.palette = initial || style.palette != m_style->palette, .fonts = fontsChanged};
        m_style = style;
        return changes;
    }

private:
    std::optional<ResolvedAppStyle> m_style;
};
} // namespace Fooyin::Gui
