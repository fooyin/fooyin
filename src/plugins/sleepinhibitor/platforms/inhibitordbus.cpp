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

#include "inhibitordbus.h"
#include "../sleepinhibitorplugin.h"

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusPendingCall>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>

using namespace Qt::StringLiterals;

namespace DbusConstants {
static const auto GnomeSessionManagerService   = "org.gnome.SessionManager"_L1;
static const auto GnomeSessionManagerPath      = "/org/gnome/SessionManager"_L1;
static const auto GnomeSessionManagerInterface = "org.gnome.SessionManager"_L1;

static const auto FreedesktopPowerMgmtService   = "org.freedesktop.PowerManagement"_L1;
static const auto FreedesktopPowerMgmtPath      = "/org/freedesktop/PowerManagement/Inhibit"_L1;
static const auto FreedesktopPowerMgmtInterface = "org.freedesktop.PowerManagement.Inhibit"_L1;
} // namespace DbusConstants

namespace Fooyin::SleepInhibitor {
InhibitorDbus::InhibitorDbus(QObject* parent)
    : InhibitorPrivate{parent}
{
    using namespace DbusConstants;

    const auto invalidateBusInterface = [this] {
        delete m_busInterface;
        m_busInterface = nullptr;
    };

    m_busInterface = new QDBusInterface(GnomeSessionManagerService, GnomeSessionManagerPath,
                                        GnomeSessionManagerInterface, QDBusConnection::sessionBus(), this);
    if(m_busInterface->isValid()) {
        qCDebug(SLEEPINHIBITOR) << "Using" << GnomeSessionManagerInterface;
        m_usingFreedesktopInterface = false;
        return;
    }

    invalidateBusInterface();
    m_busInterface = new QDBusInterface(FreedesktopPowerMgmtService, FreedesktopPowerMgmtPath,
                                        FreedesktopPowerMgmtInterface, QDBusConnection::sessionBus(), this);
    if(m_busInterface->isValid()) {
        qCDebug(SLEEPINHIBITOR) << "Using" << FreedesktopPowerMgmtInterface;
        m_usingFreedesktopInterface = true;
        return;
    }

    invalidateBusInterface();
    setState(State::Error);
    qCWarning(SLEEPINHIBITOR) << "Could not get usable DBus interface";
}

void InhibitorDbus::inhibitSleep()
{
    if(state() == State::Error || state() == State::Inhibited) {
        return;
    }

    qCDebug(SLEEPINHIBITOR) << "Inhibiting sleep";

    QList<QVariant> args;
    if(m_usingFreedesktopInterface) {
        args = {"fooyin"_L1, "fooyin is running"_L1};
    }
    else {
        constexpr auto XWindowId        = 0U;
        constexpr auto BlockSuspendFlag = 4U;
        args                            = {"fooyin"_L1, XWindowId, "fooyin is running"_L1, BlockSuspendFlag};
    }
    const QDBusPendingCall pendingCall = m_busInterface->asyncCallWithArgumentList("Inhibit"_L1, args);

    auto* watcher = new QDBusPendingCallWatcher(pendingCall, this);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, &InhibitorDbus::onInhibitCallFinished);
}

void InhibitorDbus::uninhibitSleep()
{
    if(state() != State::Inhibited) {
        return;
    }

    qCDebug(SLEEPINHIBITOR) << "Uninhibiting sleep";

    if(m_inhibitCookie == 0) [[unlikely]] {
        qCWarning(SLEEPINHIBITOR) << "Cookie is 0?";
        return;
    }

    const QDBusPendingCall pendingCall
        = m_busInterface->asyncCall(m_usingFreedesktopInterface ? "UnInhibit"_L1 : "Uninhibit"_L1, m_inhibitCookie);

    auto* watcher = new QDBusPendingCallWatcher(pendingCall, this);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, &InhibitorDbus::onUninhibitCallFinished);
}

void InhibitorDbus::onInhibitCallFinished(QDBusPendingCallWatcher* watcher)
{
    const QDBusPendingReply<uint> reply = *watcher;
    watcher->deleteLater();

    if(!reply.isError()) {
        m_inhibitCookie = reply.value();
        setState(State::Inhibited);
    }
    else {
        qCWarning(SLEEPINHIBITOR) << "Inhibit call error:" << reply.error().message();
        setState(State::Error);
    }
}

void InhibitorDbus::onUninhibitCallFinished(QDBusPendingCallWatcher* watcher)
{
    const QDBusPendingReply<> reply = *watcher;
    watcher->deleteLater();

    if(!reply.isError()) {
        setState(State::Uninhibited);
    }
    else {
        qCWarning(SLEEPINHIBITOR) << "Uninhibit call error:" << reply.error().message();
        setState(State::Error);
    }
}
} // namespace Fooyin::SleepInhibitor
