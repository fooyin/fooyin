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

#include "projectmpreset.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidgetItem>

namespace Fooyin::ProjectM {
class PresetDialog : public QDialog
{
    Q_OBJECT

public:
    PresetDialog(const std::vector<ProjectMPreset>& presets, int currentIndex, const QString& currentPath,
                 QWidget* parent);

    [[nodiscard]] int selectedPresetIndex() const;
    [[nodiscard]] QString selectedPresetPath() const;

    void markPresetFailed(int index, const QString& path, const QString& message);

Q_SIGNALS:
    void presetIndexSelected(int index);
    void presetPathSelected(const QString& path);

private:
    void markPresetFailed(QListWidgetItem* item, const QString& message);
    void refilter();
    void emitSelectionChanged();

    QLineEdit* m_filterEdit;
    QListWidget* m_presetList;
    QDialogButtonBox* m_buttons;
    int m_lastSelectedIndex;
    QString m_lastSelectedPath;
};
} // namespace Fooyin::ProjectM
