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

#include <QtConcurrent>

#include "inhibitormacos.h"

namespace Fooyin::SleepInhibitor {
InhibitorMacOs::InhibitorMacOs(QObject* parent)
    : InhibitorPrivate{parent}
{ }

void InhibitorMacOs::inhibitSleep(InhibitionType type)
{
    m_desiredState = State::Inhibited;
    m_desiredType  = type;

    if(state() == State::Error || state() == State::Inhibited) {
        return;
    }

    QtConcurrent::run([this, type] -> IOReturn {
        qCDebug(SLEEPINHIBITOR) << "Inhibiting sleep";

        const auto assertionName = tr("fooyin is running").toCFString();
        const auto assertionType = type == InhibitionType::DisplayAndSystem
                                     ? kIOPMAssertionTypePreventUserIdleDisplaySleep
                                     : kIOPMAssertionTypePreventUserIdleSystemSleep;
        const auto status
            = IOPMAssertionCreateWithName(assertionType, kIOPMAssertionLevelOn, assertionName, &m_assertionId);
        CFRelease(assertionName);
        return status;
    }).then(this, [this](IOReturn status) {
        if(status == kIOReturnSuccess) {
            setState(State::Inhibited);
        }
        else {
            qCWarning(SLEEPINHIBITOR) << "Inhibit call error, status:" << status;
            setState(State::Error);
        }
        if(m_desiredState == State::Uninhibited) {
            uninhibitSleep();
            m_desiredState = {};
        }
    });
}

void InhibitorMacOs::uninhibitSleep()
{
    m_desiredState = State::Uninhibited;

    if(state() != State::Inhibited) {
        return;
    }

    QtConcurrent::run([this] -> IOReturn {
        qCDebug(SLEEPINHIBITOR) << "Uninhibiting sleep";
        return IOPMAssertionRelease(m_assertionId);
    }).then(this, [this](IOReturn status) {
        if(status == kIOReturnSuccess) {
            setState(State::Uninhibited);
        }
        else {
            qCWarning(SLEEPINHIBITOR) << "Uninhibit call error, status:" << status;
            setState(State::Error);
        }
        if(m_desiredState == State::Inhibited) {
            inhibitSleep(m_desiredType);
            m_desiredState = {};
        }
    });
}
} // namespace Fooyin::SleepInhibitor
