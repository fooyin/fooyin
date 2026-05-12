/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
 * Copyright © 2026, Gustav Oechler <gustavoechler@gmail.com>
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

#include <core/coresettings.h>

#include <QCheckBox>
#include <QDialog>
#include <QObject>

namespace Fooyin {
namespace SleepInhibitor::Settings {
constexpr auto Enabled            = "SleepInhibitor/Enabled";
constexpr auto OnlyDuringPlayback = "SleepInhibitor/OnlyDuringPlayback";
} // namespace SleepInhibitor::Settings

class SleepInhibitorSettings : public QDialog
{
    Q_OBJECT

public:
    explicit SleepInhibitorSettings(QWidget* parent = nullptr);

    void accept() override;

private:
    FySettings m_settings;
    QCheckBox* m_enabled;
    QCheckBox* m_onlyDuringPlayback;
};
} // namespace Fooyin
