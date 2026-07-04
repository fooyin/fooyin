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

#include "lyricswidget.h"

#include <gui/configdialog.h>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QRadioButton;
class QSpinBox;
class QTabWidget;

namespace Fooyin {
class ColourButton;
class FontButton;
class ScriptLineEdit;
class SliderEditor;

namespace Lyrics {
class LyricsConfigDialog : public WidgetConfigDialog<LyricsWidget, LyricsWidget::ConfigData>
{
    Q_OBJECT

public:
    explicit LyricsConfigDialog(LyricsWidget* lyricsWidget, GuiStyleProvider* styleProvider, QWidget* parent = nullptr);

protected:
    [[nodiscard]] LyricsWidget::ConfigData config() const override;
    void setConfig(const LyricsWidget::ConfigData& config) override;

private:
    [[nodiscard]] ScrollMode scrollMode() const;
    void setScrollMode(ScrollMode mode);

    GuiStyleProvider* m_styleProvider;

    QTabWidget* m_tabs;

    QCheckBox* m_seekOnClick;
    ScriptLineEdit* m_noLyricsScript;

    SliderEditor* m_scrollDuration;
    QComboBox* m_edgeFadeMode;
    SliderEditor* m_edgeFadeSize;
    QRadioButton* m_scrollManual;
    QRadioButton* m_scrollSynced;
    QRadioButton* m_scrollAutomatic;

    QCheckBox* m_showScrollbar;
    QCheckBox* m_centreFirstSyncedLine;
    QCheckBox* m_centreLastSyncedLine;
    QComboBox* m_progressMode;
    QComboBox* m_alignment;
    QSpinBox* m_lineSpacing;
    QSpinBox* m_leftMargin;
    QSpinBox* m_topMargin;
    QSpinBox* m_rightMargin;
    QSpinBox* m_bottomMargin;

    QGroupBox* m_coloursGroup;
    ColourButton* m_bgColourBtn;
    ColourButton* m_lineColourBtn;
    ColourButton* m_unplayedColourBtn;
    ColourButton* m_playedColourBtn;
    ColourButton* m_syncedLineColourBtn;
    ColourButton* m_wordLineColourBtn;
    ColourButton* m_wordColourBtn;

    FontButton* m_baseFontBtn;
    FontButton* m_lineFontBtn;
    FontButton* m_wordLineFontBtn;
    FontButton* m_wordFontBtn;
};
} // namespace Lyrics
} // namespace Fooyin
