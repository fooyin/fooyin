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

#include "progressstyle.h"
#include "scrollbarstyle.h"
#include "styleanimation.h"
#include "theme/themeappearance.h"

#include <QProxyStyle>

namespace Fooyin {
class FooyinStyle : public QCommonStyle
{
    Q_OBJECT

public:
    FooyinStyle();
    explicit FooyinStyle(ThemeAppearance appearance);

    [[nodiscard]] const SliderAppearance& sliderAppearance() const;
    [[nodiscard]] const ThemeAppearance& appearance() const;

    void setTransitionDuration(int duration);

    QBrush animatedBrush(const QStyleOption& option, const QWidget* widget, Style::TransitionChannel channel,
                         const QBrush& target) const;

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
                       const QWidget* widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter,
                     const QWidget* widget = nullptr) const override;
    void drawComplexControl(ComplexControl control, const QStyleOptionComplex* option, QPainter* painter,
                            const QWidget* widget = nullptr) const override;
    [[nodiscard]] int pixelMetric(PixelMetric metric, const QStyleOption* option = nullptr,
                                  const QWidget* widget = nullptr) const override;
    [[nodiscard]] int styleHint(StyleHint hint, const QStyleOption* option = nullptr, const QWidget* widget = nullptr,
                                QStyleHintReturn* returnData = nullptr) const override;
    [[nodiscard]] QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& contentsSize,
                                         const QWidget* widget = nullptr) const override;
    [[nodiscard]] QRect subElementRect(SubElement element, const QStyleOption* option,
                                       const QWidget* widget = nullptr) const override;
    [[nodiscard]] QRect subControlRect(ComplexControl control, const QStyleOptionComplex* option, SubControl subControl,
                                       const QWidget* widget = nullptr) const override;

    void polish(QPalette& palette) override;
    void polish(QWidget* widget) override;
    void unpolish(QWidget* widget) override;

    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    ThemeAppearance m_appearance;
    mutable Style::StateAnimation m_stateAnimation;
    mutable Style::Progress::Animation m_progressAnimation;
    mutable Style::ScrollBar::Animation m_scrollBarAnimation;
};
} // namespace Fooyin
