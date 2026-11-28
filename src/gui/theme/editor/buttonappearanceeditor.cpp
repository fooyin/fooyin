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

#include "buttonappearanceeditor.h"

#include "theme/buttonappearance.h"

using namespace Qt::StringLiterals;

namespace Fooyin {
void ButtonAppearanceEditor::addButtonSurfaceProperties(AppearancePropertyPage* page, const QString& group,
                                                        const ButtonSurface& surface, bool menu)
{
    auto* background = addBool(page, tr("Draw background"), group, u"backgroundVisible"_s, surface.backgroundVisible);
    background->toolTip = tr("Paint the background and its tint for this state");

    addBool(page, tr("Draw border"), group, u"borderVisible"_s, surface.borderVisible);
    addNumber(page, tr("Border width"), group, u"borderWidth"_s, surface.borderWidth, 0, 32, 2);

    const auto paletteGroup = stateColourGroup(group);

    addBrush(page, tr("Background"), group, u"backgroundColour"_s, surface.backgroundColour, paletteGroup);
    addColour(page, menu ? tr("Arrow colour") : tr("Text colour"), group, u"foregroundColour"_s,
              surface.foregroundColour, paletteGroup);

    if(!menu) {
        auto* tintIcon    = addBool(page, tr("Tint icon"), group, u"tintIcon"_s, surface.tintIcon);
        tintIcon->toolTip = tr("Recolour the icon using the selected colour");
        addColour(page, tr("Icon colour"), group, u"iconColour"_s, surface.iconColour, paletteGroup);
    }

    addColour(page, tr("Border colour"), group, u"borderColour"_s, surface.borderColour, paletteGroup);
    addColour(page, tr("Tint colour"), group, u"tintColour"_s, surface.tintColour, paletteGroup);

    auto* opacity    = addNumber(page, tr("Tint opacity"), group, u"tintOpacity"_s, surface.tintOpacity, 0, 100, 0);
    opacity->suffix  = tr(" %");
    opacity->toolTip = tr("Amount of tint blended over the background");
}
} // namespace Fooyin
