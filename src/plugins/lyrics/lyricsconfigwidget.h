/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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
    explicit LyricsConfigDialog(LyricsWidget* lyricsWidget, QWidget* parent = nullptr);

private:
    [[nodiscard]] LyricsWidget::ConfigData config() const override;
    void setConfig(const LyricsWidget::ConfigData& config) override;

    [[nodiscard]] ScrollMode scrollMode() const;
    void setScrollMode(ScrollMode mode);

    QTabWidget* m_tabs;

    QCheckBox* m_seekOnClick;
    ScriptLineEdit* m_noLyricsScript;

    SliderEditor* m_scrollDuration;
    QRadioButton* m_scrollManual;
    QRadioButton* m_scrollSynced;
    QRadioButton* m_scrollAutomatic;

    QCheckBox* m_showScrollbar;
    QComboBox* m_alignment;
    QSpinBox* m_lineSpacing;
    QSpinBox* m_leftMargin;
    QSpinBox* m_topMargin;
    QSpinBox* m_rightMargin;
    QSpinBox* m_bottomMargin;

    QGroupBox* m_coloursGroup;
    QCheckBox* m_bgColour;
    ColourButton* m_bgColourBtn;
    QCheckBox* m_lineColour;
    ColourButton* m_lineColourBtn;
    QCheckBox* m_unplayedColour;
    ColourButton* m_unplayedColourBtn;
    QCheckBox* m_playedColour;
    ColourButton* m_playedColourBtn;
    QCheckBox* m_syncedLineColour;
    ColourButton* m_syncedLineColourBtn;
    QCheckBox* m_wordLineColour;
    ColourButton* m_wordLineColourBtn;
    QCheckBox* m_wordColour;
    ColourButton* m_wordColourBtn;

    QCheckBox* m_lineFont;
    FontButton* m_lineFontBtn;
    QCheckBox* m_wordLineFont;
    FontButton* m_wordLineFontBtn;
    QCheckBox* m_wordFont;
    FontButton* m_wordFontBtn;
};
} // namespace Lyrics
} // namespace Fooyin
