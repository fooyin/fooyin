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

#include "radiobrowserwidget.h"

#include <gui/configdialog.h>

class QCheckBox;
class QComboBox;
class QSpinBox;

namespace Fooyin::RadioBrowser {
class RadioBrowserConfigDialog : public WidgetConfigDialog<RadioBrowserWidget, RadioBrowserWidget::ConfigData>
{
    Q_OBJECT

public:
    explicit RadioBrowserConfigDialog(RadioBrowserWidget* radioBrowser, QWidget* parent = nullptr);

    void apply() override;

protected:
    [[nodiscard]] RadioBrowserWidget::ConfigData config() const override;
    void setConfig(const RadioBrowserWidget::ConfigData& config) override;

private:
    QComboBox* m_middleClick;
    QComboBox* m_doubleClick;
    QCheckBox* m_playbackOnSend;
    QCheckBox* m_hideBroken;
    QCheckBox* m_sendClicks;
    QCheckBox* m_showToolTips;
    QCheckBox* m_overrideRowHeight;
    QSpinBox* m_rowHeight;
    QSpinBox* m_iconSize;
    QSpinBox* m_iconHorizontalGap;
    QSpinBox* m_iconVerticalGap;
    QSpinBox* m_iconItemBorder;
    QCheckBox* m_uniformStationIcons;
};
} // namespace Fooyin::RadioBrowser
