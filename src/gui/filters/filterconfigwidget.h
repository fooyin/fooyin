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
class QLabel;
class QTimerEvent;
class QPushButton;
class QSpinBox;
class QRadioButton;

namespace Fooyin {
class ScriptTextEdit;
class BackgroundConfigWidget;
class ColourButton;

namespace Filters {
class FilterColumnRegistry;

class FYGUI_EXPORT FilterConfigDialog : public WidgetConfigDialog<FilterWidget, FilterWidget::ConfigData>
{
    Q_OBJECT

public:
    explicit FilterConfigDialog(FilterWidget* filterWidget, FilterColumnRegistry* columnRegistry,
                                QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    [[nodiscard]] FilterWidget::ConfigData config() const override;
    void setConfig(const FilterWidget::ConfigData& config) override;
    void mergeExternalConfig(const FilterWidget::ConfigData& previous,
                             const FilterWidget::ConfigData& current) override;

private:
    void setupConnections();
    void updatePlaylistControls();
    void updateExpansionControls();
    void updatePreviewVisibility();
    void updatePreviewArtworkMargins();
    void updatePreviewArtworkReflection();
    void updatePreviewBackground();
    void applyPreview(const ExpansionResult& result);
    void schedulePreview();
    void updatePreview(bool synchronous = false);
    [[nodiscard]] ExpansionScripts scripts() const;

    FilterWidget* m_filterWidget;
    QWidget* m_scriptsTab;
    ScriptTextEdit* m_headerScript;
    ScriptTextEdit* m_trackScript;
    ScriptTextEdit* m_footerScript;
    ScriptTextEdit* m_groupByScript;
    ScriptTextEdit* m_subheaderScript;
    QLabel* m_scriptErrors;
    FilterExpansionWidget* m_preview;
    std::shared_ptr<const TrackList> m_previewTracks;
    ScriptSubscription* m_previewSubscription;
    QBasicTimer m_previewTimer;
    uint64_t m_previewGeneration;

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
    QRadioButton* m_artworkBottomMode;
    QRadioButton* m_artworkRightMode;
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
    QSpinBox* m_iconHorizontalGap;
    QSpinBox* m_iconVerticalGap;
    QSpinBox* m_artworkCornerRadius;
    QSpinBox* m_artworkLeftMargin;
    QSpinBox* m_artworkTopMargin;
    QSpinBox* m_artworkRightMargin;
    QSpinBox* m_artworkBottomMargin;
    BackgroundConfigWidget* m_background;
    BackgroundConfigWidget* m_expansionBackground;
    ColourButton* m_expansionBackgroundColour;
    QCheckBox* m_expandGroups;
    QCheckBox* m_expansionAnimated;
    QCheckBox* m_expansionAlternatingRows;
    QCheckBox* m_expandedArtwork;
    QCheckBox* m_expansionCloseButton;
    QCheckBox* m_expansionArtworkReflection;
    QSpinBox* m_expansionArtworkReflectionHeight;
    QSpinBox* m_expansionArtworkReflectionOpacity;
    QSpinBox* m_expansionArtworkReflectionGap;
    QCheckBox* m_expansionArtworkAutoMargins;
    QSpinBox* m_expansionArtworkLeftMargin;
    QSpinBox* m_expansionArtworkTopMargin;
    QSpinBox* m_expansionArtworkRightMargin;
    QSpinBox* m_expansionArtworkBottomMargin;
    QComboBox* m_expansionArtworkPosition;
    QSpinBox* m_expansionArtworkSize;
    QSpinBox* m_expansionMinimumRows;
    QSpinBox* m_expansionMaximumColumns;
    QComboBox* m_expansionColumnSizing;
    QSpinBox* m_expansionMinimumColumnWidth;
    QPushButton* m_manageColumns;
};
} // namespace Filters
} // namespace Fooyin
