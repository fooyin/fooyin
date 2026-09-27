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

#include "playlistwidget.h"

#include <gui/configdialog.h>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;

namespace Fooyin {
class SliderEditor;

class PlaylistConfigDialog final : public WidgetConfigDialog<PlaylistWidget, PlaylistWidget::ConfigData>
{
    Q_OBJECT

public:
    PlaylistConfigDialog(PlaylistWidget* playlist, const QString& title, QWidget* parent = nullptr);

protected:
    void setConfig(const PlaylistWidget::ConfigData& config) override;
    [[nodiscard]] PlaylistWidget::ConfigData config() const override;
    void mergeExternalConfig(const PlaylistWidget::ConfigData& previous,
                             const PlaylistWidget::ConfigData& current) override;

private:
    void browseBackgroundImage();
    void updateBackgroundControls() const;
    void updateStartPlaybackState() const;

    QCheckBox* m_header;
    QCheckBox* m_scrollBar;
    QCheckBox* m_alternatingRows;
    QSpinBox* m_imagePadding;
    QSpinBox* m_imagePaddingTop;
    QSpinBox* m_artworkCornerRadius;
    QComboBox* m_backgroundImage;
    QLabel* m_backgroundCoverTypeLabel;
    QComboBox* m_backgroundCoverType;
    QLabel* m_backgroundTrackPreferenceLabel;
    QComboBox* m_backgroundTrackPreference;
    QLabel* m_customImageLabel;
    QLineEdit* m_customImage;
    QComboBox* m_backgroundScaling;
    QComboBox* m_backgroundPosition;
    QSpinBox* m_backgroundMaxSize;
    SliderEditor* m_backgroundBlur;
    SliderEditor* m_backgroundOpacity;
    SliderEditor* m_backgroundFadeDuration;
    QComboBox* m_doubleClick;
    QComboBox* m_middleClick;
    QCheckBox* m_startPlaybackOnSend;
};
} // namespace Fooyin
