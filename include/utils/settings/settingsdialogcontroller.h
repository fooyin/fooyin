/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "fyutils_export.h"

#include <QObject>

class QMainWindow;

namespace Fooyin {
class SettingsDialogControllerPrivate;
class SettingsManager;
class SettingsPage;
class Id;

class FYUTILS_EXPORT SettingsDialogController : public QObject
{
    Q_OBJECT

public:
    SettingsDialogController(SettingsManager* settings, QMainWindow* mainWindow);
    ~SettingsDialogController() override;

    void open();
    void openAtPage(const Id& page);

    void addPage(SettingsPage* page);

    void saveState();
    void restoreState();

signals:
    void opening();
    void closing();

private:
    std::unique_ptr<SettingsDialogControllerPrivate> p;
};
} // namespace Fooyin
