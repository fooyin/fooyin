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

#include "systemtrayicon.h"

#include <gui/guiconstants.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/utils.h>

#include <QMenu>

constexpr auto IconSize = 22;

namespace Fooyin {
SystemTrayIcon::SystemTrayIcon(ActionManager* actionManager, QObject* parent)
    : QSystemTrayIcon{parent}
    , m_actionManager{actionManager}
    , m_menu{std::make_unique<QMenu>()}
{
    setIcon(Utils::iconFromTheme(Constants::Icons::Fooyin).pixmap(IconSize));

    auto* stop      = actionManager->command(Constants::Actions::Stop)->action();
    auto* prev      = actionManager->command(Constants::Actions::Previous)->action();
    auto* playPause = actionManager->command(Constants::Actions::PlayPause)->action();
    auto* next      = actionManager->command(Constants::Actions::Next)->action();
    auto* mute      = actionManager->command(Constants::Actions::Mute)->action();
    auto* quit      = actionManager->command(Constants::Actions::Exit)->action();

    m_menu->addAction(stop);
    m_menu->addAction(prev);
    m_menu->addAction(playPause);
    m_menu->addAction(next);
    m_menu->addSeparator();
    m_menu->addAction(mute);
    m_menu->addSeparator();
    m_menu->addAction(quit);

    setContextMenu(m_menu.get());

    QObject::connect(this, &QSystemTrayIcon::activated, this, [this, playPause](const auto reason) {
        switch(reason) {
            case(QSystemTrayIcon::DoubleClick):
            case(QSystemTrayIcon::Trigger):
                emit toggleVisibility();
                break;
            case(QSystemTrayIcon::MiddleClick):
                playPause->trigger();
                break;
            default:
                break;
        }
    });
}
} // namespace Fooyin
