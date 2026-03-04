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

#include "dirbrowser.h"

#include <gui/configdialog.h>

class QCheckBox;
class QComboBox;
class QRadioButton;

namespace Fooyin {
class DirBrowserConfigDialog : public WidgetConfigDialog<DirBrowser, DirBrowser::ConfigData>
{
    Q_OBJECT

public:
    explicit DirBrowserConfigDialog(DirBrowser* browser, QWidget* parent = nullptr);

protected:
    [[nodiscard]] DirBrowser::ConfigData config() const override;
    void setConfig(const DirBrowser::ConfigData& config) override;

private:
    QRadioButton* m_treeMode;
    QRadioButton* m_listMode;

    QCheckBox* m_showIcons;
    QCheckBox* m_indentList;
    QCheckBox* m_showHorizScrollbar;
    QCheckBox* m_showControls;
    QCheckBox* m_showLocation;
    QCheckBox* m_showSymLinks;
    QCheckBox* m_showHidden;

    QComboBox* m_doubleClick;
    QComboBox* m_middleClick;
    QCheckBox* m_playbackOnSend;
};
} // namespace Fooyin
