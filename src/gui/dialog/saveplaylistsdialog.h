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

#include <QDialog>

class QComboBox;

namespace Fooyin {
class Application;
class SettingsManager;

class SavePlaylistsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SavePlaylistsDialog(Application* core, QWidget* parent = nullptr);

    void accept() override;

private:
    Application* m_core;
    SettingsManager* m_settings;
    QComboBox* m_formats;
};
} // namespace Fooyin
