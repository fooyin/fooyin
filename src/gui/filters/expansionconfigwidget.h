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

#include "filterwidget.h"

#include <QBasicTimer>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QSpinBox;

namespace Fooyin {
class BackgroundConfigWidget;
class ColourButton;
class FontButton;
class ScriptSubscription;
class ScriptTextEdit;

namespace Filters {
class ExpansionConfigWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ExpansionConfigWidget(FilterWidget* filterWidget, QWidget* parent = nullptr);

    void updateConfig(FilterWidget::ConfigData& config) const;
    void setConfig(const FilterWidget::ConfigData& config);

Q_SIGNALS:
    void expansionEnabledChanged(bool enabled);

protected:
    void changeEvent(QEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void setupConnections();
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
    ScriptTextEdit* m_headerScript;
    ScriptTextEdit* m_trackScript;
    ScriptTextEdit* m_footerScript;
    ScriptTextEdit* m_groupByScript;
    ScriptTextEdit* m_subheaderScript;
    FilterExpansionWidget* m_preview;
    std::shared_ptr<const TrackList> m_previewTracks;
    ScriptSubscription* m_previewSubscription;
    QBasicTimer m_previewTimer;
    uint64_t m_previewGeneration;

    BackgroundConfigWidget* m_expansionBackground;
    ColourButton* m_expansionBackgroundColour;
    FontButton* m_expansionPlayingRowFont;
    ColourButton* m_expansionPlayingRowColour;
    QCheckBox* m_expandGroups;
    QCheckBox* m_expansionAnimated;
    QSpinBox* m_expansionAnimationDuration;
    QCheckBox* m_expansionAlternatingRows;
    QCheckBox* m_expansionMatchGridMargins;
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
};
} // namespace Filters
} // namespace Fooyin
