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

#include "filterwidget.h"

#include <gui/configdialog.h>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace Fooyin::Filters {
class FilterColumnRegistry;

class FilterConfigDialog : public WidgetConfigDialog<FilterWidget, FilterWidget::ConfigData>
{
    Q_OBJECT

public:
    explicit FilterConfigDialog(FilterWidget* filterWidget, ActionManager* actionManager,
                                FilterColumnRegistry* columnRegistry, QWidget* parent = nullptr);

private:
    [[nodiscard]] FilterWidget::ConfigData config() const override;
    void setConfig(const FilterWidget::ConfigData& config) override;

    ActionManager* m_actionManager;
    FilterColumnRegistry* m_columnRegistry;

    QComboBox* m_middleClick;
    QComboBox* m_doubleClick;
    QCheckBox* m_playbackOnSend;

    QCheckBox* m_playlistEnabled;
    QCheckBox* m_autoSwitch;
    QCheckBox* m_keepAlive;
    QLineEdit* m_playlistName;

    QCheckBox* m_overrideRowHeight;
    QSpinBox* m_rowHeight;
    QSpinBox* m_iconWidth;
    QSpinBox* m_iconHeight;
    QSpinBox* m_iconHorizontalGap;
    QSpinBox* m_iconVerticalGap;
    QPushButton* m_manageColumns;
};
} // namespace Fooyin::Filters
