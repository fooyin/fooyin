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

#include "inhibitorwin32.h"
#include "../sleepinhibitorplugin.h"

#include <Windows.h>

namespace Fooyin::SleepInhibitor {
InhibitorWin32::InhibitorWin32(QObject* parent)
    : InhibitorPrivate{parent}
{ }

void InhibitorWin32::inhibitSleep()
{
    if(state() == State::Error || state() == State::Inhibited) {
        return;
    }

    qCDebug(SLEEPINHIBITOR) << "Inhibiting sleep";

    const auto prevState = SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
    if(prevState != 0) {
        setState(State::Inhibited);
    }
    else {
        qCWarning(SLEEPINHIBITOR) << "Inhibit call error";
        setState(State::Error);
    }
}

void InhibitorWin32::uninhibitSleep()
{
    if(state() != State::Inhibited) {
        return;
    }

    qCDebug(SLEEPINHIBITOR) << "Uninhibiting sleep";

    const auto prevState = SetThreadExecutionState(ES_CONTINUOUS);
    if(prevState != 0) {
        setState(State::Uninhibited);
    }
    else {
        qCWarning(SLEEPINHIBITOR) << "Uninhibit call error";
        setState(State::Error);
    }
}
} // namespace Fooyin::SleepInhibitor
