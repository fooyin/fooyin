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

#include "fygui_export.h"

#include <QObject>

class QMainWindow;

namespace Fooyin {
class FYGUI_EXPORT WindowController : public QObject
{
    Q_OBJECT

public:
    explicit WindowController(QMainWindow* window);

    [[nodiscard]] bool isFullScreen() const;
    void setFullScreen(bool fullscreen);

    void raise();

    bool eventFilter(QObject* watched, QEvent* event) override;

    [[nodiscard]] QMainWindow* mainWindow() const
    {
        return m_mainWindow;
    }

signals:
    void isFullScreenChanged(bool fullscreen);
    void windowShown();
    void windowHidden();

private:
    QMainWindow* m_mainWindow;
};
} // namespace Fooyin
