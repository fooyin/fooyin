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

#include "queueviewer.h"

#include <gui/configdialog.h>

class QCheckBox;
class QGroupBox;
class QSpinBox;

namespace Fooyin {
class ScriptLineEdit;

class QueueViewerConfigDialog : public WidgetConfigDialog<QueueViewer, QueueViewer::ConfigData>
{
    Q_OBJECT

public:
    explicit QueueViewerConfigDialog(QueueViewer* queueViewer, QWidget* parent = nullptr);

private:
    void setConfig(const QueueViewer::ConfigData& config) override;
    [[nodiscard]] QueueViewer::ConfigData config() const override;

    ScriptLineEdit* m_titleScript;
    ScriptLineEdit* m_subtitleScript;

    QCheckBox* m_headers;
    QCheckBox* m_scrollBars;
    QCheckBox* m_altRowColours;
    QCheckBox* m_showCurrent;
    QGroupBox* m_showIcon;
    QSpinBox* m_iconWidth;
    QSpinBox* m_iconHeight;
};
} // namespace Fooyin
