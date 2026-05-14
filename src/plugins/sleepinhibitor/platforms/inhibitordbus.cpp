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

static const auto FreedesktopPortalService   = "org.freedesktop.portal.Desktop"_L1;
static const auto FreedesktopPortalPath      = "/org/freedesktop/portal/desktop"_L1;
static const auto FreedesktopPortalInterface = "org.freedesktop.portal.Inhibit"_L1;
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
        m_interface = Interface::GnomeSessionManager;
        return;
    }

    invalidateBusInterface();
    m_busInterface = new QDBusInterface(FreedesktopPowerMgmtService, FreedesktopPowerMgmtPath,
                                        FreedesktopPowerMgmtInterface, QDBusConnection::sessionBus(), this);
    if(m_busInterface->isValid()) {
        qCDebug(SLEEPINHIBITOR) << "Using" << FreedesktopPowerMgmtInterface;
        m_interface = Interface::FreedesktopPower;
        return;
    }

    invalidateBusInterface();
    m_busInterface = new QDBusInterface(FreedesktopPortalService, FreedesktopPortalPath, FreedesktopPortalInterface,
                                        QDBusConnection::sessionBus(), this);
    if(m_busInterface->isValid()) {
        qCDebug(SLEEPINHIBITOR) << "Using" << FreedesktopPortalInterface;
        m_interface = Interface::FreedesktopPortal;
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

    constexpr auto BlockSuspendFlag = 4U;
    static const auto Reason        = QStringLiteral("fooyin is running");

    QList<QVariant> args;
    switch(m_interface) {
        case Interface::None:
            break;
        case Interface::GnomeSessionManager: {
            constexpr auto XWindowId = 0U;
            args                     = {"fooyin"_L1, XWindowId, Reason, BlockSuspendFlag};
        }
        case Interface::FreedesktopPower:
            args = {"fooyin"_L1, Reason};
            break;
        case Interface::FreedesktopPortal: {
            QMap<QString, QVariant> options;
            options["reason"_L1] = Reason;
            args                 = {"fooyin"_L1, BlockSuspendFlag, options};
        } break;
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

    if(m_interface == Interface::FreedesktopPortal) {
        auto* inhibitObjectInterface
            = new QDBusInterface(DbusConstants::FreedesktopPortalService, m_inhibitHandle.path(),
                                 "org.freedesktop.portal.Request"_L1, QDBusConnection::sessionBus());
        if(!inhibitObjectInterface->isValid()) [[unlikely]] {
            qCWarning(SLEEPINHIBITOR) << "Bad inhibit handle? Object path:" << m_inhibitHandle.path();
            setState(State::Error);
            return;
        }

        const auto pendingCall = inhibitObjectInterface->asyncCall("Close"_L1);
        auto* watcher          = new QDBusPendingCallWatcher(pendingCall, this);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, &InhibitorDbus::onUninhibitCallFinished);

        delete inhibitObjectInterface;
    }
    else {
        if(m_inhibitCookie == 0) [[unlikely]] {
            qCWarning(SLEEPINHIBITOR) << "Cookie is 0?";
            setState(State::Error);
            return;
        }

        const auto pendingCall = m_busInterface->asyncCall(
            m_interface == Interface::FreedesktopPower ? "UnInhibit"_L1 : "Uninhibit"_L1, m_inhibitCookie);
        auto* watcher = new QDBusPendingCallWatcher(pendingCall, this);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, &InhibitorDbus::onUninhibitCallFinished);
    }
}

void InhibitorDbus::onInhibitCallFinished(QDBusPendingCallWatcher* watcher)
{
    watcher->deleteLater();

    const auto handleReply = [this]<typename T>(const QDBusPendingReply<T>& reply, auto&& onSuccess) {
        if(reply.isValid()) {
            onSuccess();
            setState(State::Inhibited);
        }
        else {
            qCWarning(SLEEPINHIBITOR) << "Inhibit call error:" << reply.error().message();
            setState(State::Error);
        }
    };

    if(m_interface == Interface::FreedesktopPortal) {
        const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        handleReply(reply, [this, &reply] { m_inhibitHandle = reply.value(); });
    }
    else {
        const QDBusPendingReply<uint> reply = *watcher;
        handleReply(reply, [this, &reply] { m_inhibitCookie = reply.value(); });
    }
}

void InhibitorDbus::onUninhibitCallFinished(QDBusPendingCallWatcher* watcher)
{
    const QDBusPendingReply<> reply = *watcher;
    watcher->deleteLater();

    if(reply.isValid()) {
        setState(State::Uninhibited);
    }
    else {
        qCWarning(SLEEPINHIBITOR) << "Uninhibit call error:" << reply.error().message();
        setState(State::Error);
    }
}
} // namespace Fooyin::SleepInhibitor
