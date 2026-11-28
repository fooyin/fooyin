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

#include "checkappearance.h"
#include "dockwidgetappearance.h"
#include "frameappearance.h"
#include "groupboxappearance.h"
#include "headerappearance.h"
#include "inputappearance.h"
#include "itemviewappearance.h"
#include "menuappearance.h"
#include "progressbarappearance.h"
#include "pushbuttonappearance.h"
#include "scrollbarappearance.h"
#include "sliderappearance.h"
#include "splitterappearance.h"
#include "tabappearance.h"
#include "toolbarappearance.h"
#include "toolbuttonappearance.h"
#include "tooltipappearance.h"

namespace Fooyin {
struct ThemeAppearance
{
    struct Layout
    {
        int horizontalSpacing{-1};
        int verticalSpacing{-1};
        int leftMargin{-1};
        int topMargin{-1};
        int rightMargin{-1};
        int bottomMargin{-1};

        bool operator==(const Layout& other) const = default;
    };

    SliderAppearance slider{};
    ScrollBarAppearance scrollBar{};
    PushButtonAppearance pushButton{};
    ToolButtonAppearance toolButton{};
    ToolBarAppearance toolBar{};
    DockWidgetAppearance dockWidget{};
    FrameAppearance frame{};
    CheckAppearance checkBox{};
    CheckAppearance radioButton{defaultRadioButtonAppearance()};
    InputAppearance lineEdit{};
    InputAppearance comboBox{defaultInputAppearance(InputType::ComboBox)};
    InputAppearance spinBox{};
    ProgressBarAppearance progressBar{};
    GroupBoxAppearance groupBox{};
    TabAppearance tab{};
    MenuAppearance menu{};
    HeaderAppearance header{};
    ItemViewAppearance itemView{};
    ToolTipAppearance toolTip{};
    SplitterAppearance splitter{};
    Layout layout{};

    int transitionDuration{120};

    bool operator==(const ThemeAppearance& other) const = default;
};

std::expected<ThemeAppearance, QString> resolveThemeAppearance(const QJsonObject& appearance);
} // namespace Fooyin
