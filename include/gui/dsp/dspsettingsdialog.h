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

#include "fygui_export.h"

#include <QDialog>

class QDialogButtonBox;
class QVBoxLayout;

namespace Fooyin {
class FYGUI_EXPORT DspSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DspSettingsDialog(QWidget* parent = nullptr);
    ~DspSettingsDialog() override;

    virtual void loadSettings(const QByteArray& settings) = 0;
    [[nodiscard]] virtual QByteArray saveSettings() const = 0;

protected:
    [[nodiscard]] QVBoxLayout* contentLayout() const;
    void setRestoreDefaultsVisible(bool visible);
    void publishPreviewSettings();
    virtual void restoreDefaults() = 0;

signals:
    void previewSettingsChanged(const QByteArray& settings);

private:
    QVBoxLayout* m_mainLayout;
    QVBoxLayout* m_contentLayout;
    QDialogButtonBox* m_restoreButtonBox;
    QDialogButtonBox* m_buttonBox;
};
} // namespace Fooyin
