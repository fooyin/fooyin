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

#pragma once

#include "fygui_export.h"

#include "filterwidget.h"

#include <gui/configdialog.h>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QRadioButton;

namespace Fooyin {
class BackgroundConfigWidget;

namespace Filters {
class FilterColumnRegistry;
class CoverFlowConfigWidget;
class ExpansionConfigWidget;

class FYGUI_EXPORT FilterConfigDialog : public WidgetConfigDialog<FilterWidget, FilterWidget::ConfigData>
{
    Q_OBJECT

public:
    explicit FilterConfigDialog(FilterWidget* filterWidget, FilterColumnRegistry* columnRegistry,
                                QWidget* parent = nullptr);

protected:
    [[nodiscard]] FilterWidget::ConfigData config() const override;
    void setConfig(const FilterWidget::ConfigData& config) override;
    void mergeExternalConfig(const FilterWidget::ConfigData& previous,
                             const FilterWidget::ConfigData& current) override;

private:
    void setupConnections();
    void updatePlaylistControls();

    FilterWidget* m_filterWidget;
    ExpansionConfigWidget* m_expansionConfig;
    CoverFlowConfigWidget* m_coverFlowConfig;
    FilterColumnRegistry* m_columnRegistry;

    QComboBox* m_source;
    QComboBox* m_middleClick;
    QComboBox* m_doubleClick;
    QComboBox* m_expansionMiddleClick;
    QComboBox* m_expansionDoubleClick;
    QCheckBox* m_playbackOnSend;

    QCheckBox* m_playlistEnabled;
    QCheckBox* m_autoSwitch;
    QCheckBox* m_preservePlaybackPlaylist;
    QLineEdit* m_playlistName;
    QCheckBox* m_restoreState;

    QRadioButton* m_columnsMode;
    QRadioButton* m_coverFlowMode;
    QRadioButton* m_artworkBottomMode;
    QRadioButton* m_artworkRightMode;
    QRadioButton* m_artworkOverlayMode;
    QRadioButton* m_artworkNoLabelsMode;
    QComboBox* m_coverType;
    QComboBox* m_coverSource;
    QCheckBox* m_showSummary;
    QCheckBox* m_showHeader;
    QCheckBox* m_showScrollbar;
    QCheckBox* m_alternatingRows;
    QCheckBox* m_overrideRowHeight;
    QSpinBox* m_rowHeight;
    QSpinBox* m_iconWidth;
    QSpinBox* m_iconHeight;
    QCheckBox* m_autoFitArtwork;
    QComboBox* m_artworkScaling;
    QSpinBox* m_iconHorizontalGap;
    QSpinBox* m_iconVerticalGap;
    QCheckBox* m_overrideGridMargins;
    QSpinBox* m_gridLeftMargin;
    QSpinBox* m_gridTopMargin;
    QSpinBox* m_gridRightMargin;
    QSpinBox* m_gridBottomMargin;
    QSpinBox* m_artworkCornerRadius;
    QCheckBox* m_overrideArtworkMargins;
    QSpinBox* m_artworkLeftMargin;
    QSpinBox* m_artworkTopMargin;
    QSpinBox* m_artworkRightMargin;
    QSpinBox* m_artworkBottomMargin;
    BackgroundConfigWidget* m_background;
    QPushButton* m_manageColumns;
};
} // namespace Filters
} // namespace Fooyin
