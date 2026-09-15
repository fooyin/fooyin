/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "inhibitormacos.h"

#include <utils/async.h>

#include <IOKit/pwr_mgt/IOPMLib.h>

#include <mutex>

namespace Fooyin::SleepInhibitor {
struct InhibitorMacOs::AssertionState
{
    std::mutex mutex;
    std::optional<InhibitionType> desiredType;
    std::optional<InhibitionType> currentType;
    IOPMAssertionID assertionId{};
    bool reconciliationRunning{false};
};

InhibitorMacOs::InhibitorMacOs(QObject* parent)
    : InhibitorPrivate{parent}
    , m_assertionState{std::make_shared<AssertionState>()}
{ }

InhibitorMacOs::~InhibitorMacOs()
{
    setDesiredType({});
    for(auto& operation : m_operations) {
        operation.waitForFinished();
    }

    if(m_assertionState->currentType) {
        const auto status = IOPMAssertionRelease(m_assertionState->assertionId);
        if(status != kIOReturnSuccess) {
            qCWarning(SLEEPINHIBITOR) << "Sleep uninhibition call error during shutdown, status:" << status;
        }
    }
}

void InhibitorMacOs::inhibitSleep(InhibitionType type)
{
    if(m_desiredType == type && state() != State::Error) {
        return;
    }
    setDesiredType(type);
}

void InhibitorMacOs::uninhibitSleep()
{
    if(!m_desiredType && state() != State::Error) {
        return;
    }
    setDesiredType({});
}

void InhibitorMacOs::setDesiredType(std::optional<InhibitionType> type)
{
    m_desiredType = type;
    setState(type ? State::Inhibiting : State::Uninhibiting);

    bool shouldStartReconciliation{false};
    {
        const std::scoped_lock lock{m_assertionState->mutex};
        m_assertionState->desiredType = type;
        if(!m_assertionState->reconciliationRunning) {
            m_assertionState->reconciliationRunning = true;
            shouldStartReconciliation               = true;
        }
    }

    if(shouldStartReconciliation) {
        startReconciliation();
    }
}

void InhibitorMacOs::startReconciliation()
{
    std::erase_if(m_operations, [](const QFuture<void>& operation) { return operation.isFinished(); });
    m_operations.emplace_back(
        Utils::asyncExec([assertionState = m_assertionState, this] { reconcile(assertionState, this); }));
}

void InhibitorMacOs::reconcile(const std::shared_ptr<AssertionState>& assertionState, InhibitorMacOs* inhibitor)
{
    const auto reportResult = [inhibitor](std::optional<InhibitionType> actualType, IOReturn status) {
        QMetaObject::invokeMethod(
            inhibitor,
            [inhibitor, actualType, status] {
                if(status != kIOReturnSuccess) {
                    qCWarning(SLEEPINHIBITOR) << "Sleep inhibition call error, status:" << status;
                    inhibitor->setState(State::Error);
                }
                else if(actualType == inhibitor->m_desiredType) {
                    inhibitor->setState(actualType ? State::Inhibited : State::Uninhibited);
                }
            },
            Qt::QueuedConnection);
    };

    while(true) {
        std::optional<InhibitionType> desiredType;
        {
            const std::scoped_lock lock{assertionState->mutex};
            desiredType = assertionState->desiredType;
        }

        IOReturn status{kIOReturnSuccess};
        if(assertionState->currentType && assertionState->currentType != desiredType) {
            qCDebug(SLEEPINHIBITOR) << "Uninhibiting sleep";
            status = IOPMAssertionRelease(assertionState->assertionId);
            if(status == kIOReturnSuccess) {
                assertionState->currentType.reset();
            }
        }

        if(status == kIOReturnSuccess && desiredType && assertionState->currentType != desiredType) {
            qCDebug(SLEEPINHIBITOR) << "Inhibiting sleep";

            const auto assertionName = tr("fooyin is running").toCFString();
            const auto assertionType = desiredType == InhibitionType::DisplayAndSystem
                                         ? kIOPMAssertionTypePreventUserIdleDisplaySleep
                                         : kIOPMAssertionTypePreventUserIdleSystemSleep;
            status                   = IOPMAssertionCreateWithName(assertionType, kIOPMAssertionLevelOn, assertionName,
                                                                   &assertionState->assertionId);
            CFRelease(assertionName);
            if(status == kIOReturnSuccess) {
                assertionState->currentType = desiredType;
            }
        }

        const std::scoped_lock lock{assertionState->mutex};
        if(desiredType != assertionState->desiredType) {
            continue;
        }

        reportResult(assertionState->currentType, status);
        assertionState->reconciliationRunning = false;
        return;
    }
}
} // namespace Fooyin::SleepInhibitor
