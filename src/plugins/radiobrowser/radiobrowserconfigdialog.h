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

namespace Fooyin {
class BackgroundConfigWidget;
class ColourButton;

namespace RadioBrowser {
class RadioBrowserConfigDialog : public WidgetConfigDialog<RadioBrowserWidget, RadioBrowserWidget::ConfigData>
{
    Q_OBJECT

public:
    explicit RadioBrowserConfigDialog(RadioBrowserWidget* radioBrowser, QWidget* parent = nullptr);

    void apply() override;

protected:
    void changeEvent(QEvent* event) override;

    [[nodiscard]] RadioBrowserWidget::ConfigData config() const override;
    void setConfig(const RadioBrowserWidget::ConfigData& config) override;
    void mergeExternalConfig(const RadioBrowserWidget::ConfigData& previous,
                             const RadioBrowserWidget::ConfigData& current) override;

private:
    void updateAppearanceControls();

    QComboBox* m_middleClick;
    QComboBox* m_doubleClick;
    QCheckBox* m_playbackOnSend;
    QCheckBox* m_hideBroken;
    QCheckBox* m_sendClicks;
    QCheckBox* m_showSearchBar;
    QComboBox* m_displayMode;
    QComboBox* m_captions;
    QCheckBox* m_showHeader;
    QCheckBox* m_showScrollbar;
    QCheckBox* m_alternatingRows;
    QCheckBox* m_showStationIcons;
    QCheckBox* m_showSavedStationIcons;
    QCheckBox* m_showToolTips;
    QCheckBox* m_separateSavedStationsViewState;
    QCheckBox* m_overrideRowHeight;
    QSpinBox* m_rowHeight;
    QSpinBox* m_iconSize;
    QCheckBox* m_autoFitArtwork;
    QSpinBox* m_iconHorizontalGap;
    QSpinBox* m_iconVerticalGap;
    QCheckBox* m_useIconGapsForSideCaptions;
    QSpinBox* m_iconLeftMargin;
    QSpinBox* m_iconTopMargin;
    QSpinBox* m_iconRightMargin;
    QSpinBox* m_iconBottomMargin;
    QSpinBox* m_iconCornerRadius;
    QCheckBox* m_uniformStationIcons;
    ColourButton* m_playingRowColour;
    BackgroundConfigWidget* m_background;
};
} // namespace RadioBrowser
} // namespace Fooyin
