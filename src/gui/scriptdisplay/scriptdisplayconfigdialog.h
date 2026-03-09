/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include <gui/configdialog.h>

#include "scriptdisplay.h"

class QCheckBox;
class QComboBox;
class QTabWidget;

namespace Fooyin {
class ColourButton;
class FontButton;
class ScriptTextEdit;

class ScriptDisplayConfigDialog : public WidgetConfigDialog<ScriptDisplay, ScriptDisplay::ConfigData>
{
public:
    explicit ScriptDisplayConfigDialog(ScriptDisplay* widget, QWidget* parent = nullptr);

protected:
    [[nodiscard]] ScriptDisplay::ConfigData config() const override;
    void setConfig(const ScriptDisplay::ConfigData& config) override;

private:
    ScriptTextEdit* m_script;

    QTabWidget* m_tabs;
    QWidget* m_formatTab;
    QWidget* m_styleTab;

    QComboBox* m_horizontalAlignment;
    QComboBox* m_verticalAlignment;

    QCheckBox* m_customFont;
    FontButton* m_font;

    QCheckBox* m_customTextColour;
    ColourButton* m_textColour;
    QCheckBox* m_customBackgroundColour;
    ColourButton* m_backgroundColour;
    QCheckBox* m_customLinkColour;
    ColourButton* m_linkColour;
};
} // namespace Fooyin
