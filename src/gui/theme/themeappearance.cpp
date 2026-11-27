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

#include "themeappearance.h"

#include "appearanceutils.h"

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
std::expected<ThemeAppearance::Layout, QString> readLayout(const QJsonValue& value)
{
    ThemeAppearance::Layout resolved;

    const QString path = u"appearance.layout"_s;
    const auto object  = Appearance::readObject(value, path,
                                                {"horizontalSpacing"_L1, "verticalSpacing"_L1, "leftMargin"_L1,
                                                 "topMargin"_L1, "rightMargin"_L1, "bottomMargin"_L1});
    if(!object) {
        return std::unexpected{object.error()};
    }

    for(const auto& [name, target] : std::array{
            std::pair{"horizontalSpacing"_L1, &resolved.horizontalSpacing},
            std::pair{"verticalSpacing"_L1, &resolved.verticalSpacing},
            std::pair{"leftMargin"_L1, &resolved.leftMargin}, std::pair{"topMargin"_L1, &resolved.topMargin},
            std::pair{"rightMargin"_L1, &resolved.rightMargin}, std::pair{"bottomMargin"_L1, &resolved.bottomMargin}}) {
        const auto field = Appearance::readNumber(*object, name, path, *target, -1, 128);
        if(!field) {
            return std::unexpected{field.error()};
        }
        *target = *field;
    }

    return resolved;
}
} // namespace

std::expected<ThemeAppearance, QString> resolveThemeAppearance(const QJsonObject& appearance)
{
    const auto controls = Appearance::readObject(
        appearance, u"appearance"_s,
        {"slider"_L1,     "scrollBar"_L1,   "pushButton"_L1,  "toolButton"_L1, "toolBar"_L1,
         "dockWidget"_L1, "checkBox"_L1,    "radioButton"_L1, "lineEdit"_L1,   "comboBox"_L1,
         "spinBox"_L1,    "progressBar"_L1, "groupBox"_L1,    "tab"_L1,        "menu"_L1,
         "header"_L1,     "itemView"_L1,    "toolTip"_L1,     "splitter"_L1,   "transitionDuration"_L1,
         "layout"_L1});
    if(!controls) {
        return std::unexpected{controls.error()};
    }

    const auto transitionDuration = Appearance::readNumber(*controls, "transitionDuration"_L1, u"appearance"_s,
                                                           ThemeAppearance{}.transitionDuration, 0, 400);
    if(!transitionDuration) {
        return std::unexpected{transitionDuration.error()};
    }

    const auto layout = readLayout(controls->value("layout"_L1));
    if(!layout) {
        return std::unexpected{layout.error()};
    }

    const auto slider = resolveSliderAppearance(*controls);
    if(!slider) {
        return std::unexpected{slider.error()};
    }

    const auto scrollBar = resolveScrollBarAppearance(*controls);
    if(!scrollBar) {
        return std::unexpected{scrollBar.error()};
    }

    const auto pushButton = resolvePushButtonAppearance(*controls);
    if(!pushButton) {
        return std::unexpected{pushButton.error()};
    }

    const auto toolButton = resolveToolButtonAppearance(*controls);
    if(!toolButton) {
        return std::unexpected{toolButton.error()};
    }

    const auto checkBox = resolveCheckBoxAppearance(*controls);
    if(!checkBox) {
        return std::unexpected{checkBox.error()};
    }

    const auto toolBar = resolveToolBarAppearance(*controls);
    if(!toolBar) {
        return std::unexpected{toolBar.error()};
    }

    const auto dockWidget = resolveDockWidgetAppearance(*controls);
    if(!dockWidget) {
        return std::unexpected{dockWidget.error()};
    }

    const auto radioButton = resolveRadioButtonAppearance(*controls);
    if(!radioButton) {
        return std::unexpected{radioButton.error()};
    }

    const auto lineEdit = resolveInputAppearance(*controls, InputType::LineEdit);
    if(!lineEdit) {
        return std::unexpected{lineEdit.error()};
    }

    const auto comboBox = resolveInputAppearance(*controls, InputType::ComboBox);
    if(!comboBox) {
        return std::unexpected{comboBox.error()};
    }

    const auto spinBox = resolveInputAppearance(*controls, InputType::SpinBox);
    if(!spinBox) {
        return std::unexpected{spinBox.error()};
    }

    const auto progressBar = resolveProgressBarAppearance(*controls);
    if(!progressBar) {
        return std::unexpected{progressBar.error()};
    }

    const auto groupBox = resolveGroupBoxAppearance(*controls);
    if(!groupBox) {
        return std::unexpected{groupBox.error()};
    }

    const auto tab = resolveTabAppearance(*controls);
    if(!tab) {
        return std::unexpected{tab.error()};
    }

    const auto menu = resolveMenuAppearance(*controls);
    if(!menu) {
        return std::unexpected{menu.error()};
    }

    const auto header = resolveHeaderAppearance(*controls);
    if(!header) {
        return std::unexpected{header.error()};
    }

    const auto itemView = resolveItemViewAppearance(*controls);
    if(!itemView) {
        return std::unexpected{itemView.error()};
    }

    const auto toolTip = resolveToolTipAppearance(*controls);
    if(!toolTip) {
        return std::unexpected{toolTip.error()};
    }

    const auto splitter = resolveSplitterAppearance(*controls);
    if(!splitter) {
        return std::unexpected{splitter.error()};
    }

    return ThemeAppearance{.slider             = *slider,
                           .scrollBar          = *scrollBar,
                           .pushButton         = *pushButton,
                           .toolButton         = *toolButton,
                           .toolBar            = *toolBar,
                           .dockWidget         = *dockWidget,
                           .checkBox           = *checkBox,
                           .radioButton        = *radioButton,
                           .lineEdit           = *lineEdit,
                           .comboBox           = *comboBox,
                           .spinBox            = *spinBox,
                           .progressBar        = *progressBar,
                           .groupBox           = *groupBox,
                           .tab                = *tab,
                           .menu               = *menu,
                           .header             = *header,
                           .itemView           = *itemView,
                           .toolTip            = *toolTip,
                           .splitter           = *splitter,
                           .layout             = *layout,
                           .transitionDuration = *transitionDuration};
}
} // namespace Fooyin
