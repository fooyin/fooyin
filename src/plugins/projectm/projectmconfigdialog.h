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

#include "projectmpresetlibrary.h"
#include "projectmwidget.h"

#include <gui/configdialog.h>

class QPushButton;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;

namespace Fooyin::ProjectM {
class ProjectMConfigDialog : public WidgetConfigDialog<ProjectMWidget, ProjectMWidget::ConfigData>
{
    Q_OBJECT

public:
    explicit ProjectMConfigDialog(ProjectMWidget* widget, QWidget* parent = nullptr);

protected:
    [[nodiscard]] ProjectMWidget::ConfigData config() const override;
    void setConfig(const ProjectMWidget::ConfigData& config) override;

private:
    void browsePresetDir();
    void updateSummary();

    QLineEdit* m_presetDir;
    QLabel* m_summary;
    QPushButton* m_browseDir;
    QCheckBox* m_scanRecursive;
    QComboBox* m_maxFps;
    QComboBox* m_meshSize;
    QCheckBox* m_aspectCorrection;
    QDoubleSpinBox* m_presetDuration;
    QDoubleSpinBox* m_softCutDuration;
    QCheckBox* m_hardCutsEnabled;
    QDoubleSpinBox* m_hardCutDuration;
    QDoubleSpinBox* m_hardCutSensitivity;
    QDoubleSpinBox* m_beatSensitivity;
};
} // namespace Fooyin::ProjectM
