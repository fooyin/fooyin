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
class QSpinBox;

namespace Fooyin {
class BackgroundConfigWidget;

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
    void updateReflectionControls() const;
    void updateStartPlaybackState() const;

    QCheckBox* m_header;
    QCheckBox* m_stickyHeaders;
    QCheckBox* m_stickySubheaders;
    QCheckBox* m_stickyArtwork;
    QCheckBox* m_scrollBar;
    QCheckBox* m_alternatingRows;
    QSpinBox* m_imagePadding;
    QSpinBox* m_imagePaddingTop;
    QSpinBox* m_artworkCornerRadius;
    QCheckBox* m_artworkReflection;
    QSpinBox* m_artworkReflectionHeight;
    QSpinBox* m_artworkReflectionOpacity;
    QSpinBox* m_artworkReflectionGap;
    BackgroundConfigWidget* m_background;
    QComboBox* m_doubleClick;
    QComboBox* m_middleClick;
    QCheckBox* m_startPlaybackOnSend;
};
} // namespace Fooyin
