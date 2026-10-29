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

#include "cddadrivesettings.h"

#include <QDialog>

#include <memory>

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace Fooyin {
class NetworkAccessManager;
}

namespace Fooyin::Cdda {
class CdDriveSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    CdDriveSettingsDialog(CdDriveInfo drive, std::shared_ptr<CdDriveSettingsStore> settingsStore,
                          std::shared_ptr<NetworkAccessManager> networkAccess, QWidget* parent = nullptr);

    void accept() override;

private:
    void lookupAccurateRipOffset();

    CdDriveInfo m_drive;
    std::shared_ptr<CdDriveSettingsStore> m_settingsStore;
    std::shared_ptr<NetworkAccessManager> m_networkAccess;

    QSpinBox* m_readOffset;
    QComboBox* m_security;
    QComboBox* m_speedLimit;
    QPushButton* m_lookupOffset;
    QLabel* m_offsetStatus;
};
} // namespace Fooyin::Cdda
