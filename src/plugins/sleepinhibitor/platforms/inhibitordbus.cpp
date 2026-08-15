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

#include "inhibitordbus.h"
#include "inhibitor.h"

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusPendingCall>
#include <QtDBus/QDBusPendingReply>

using namespace Qt::StringLiterals;

namespace DbusConstants {
constexpr auto GnomeSessionManagerService   = "org.gnome.SessionManager"_L1;
constexpr auto GnomeSessionManagerPath      = "/org/gnome/SessionManager"_L1;
constexpr auto GnomeSessionManagerInterface = "org.gnome.SessionManager"_L1;

constexpr auto FreedesktopPowerMgmtService   = "org.freedesktop.PowerManagement"_L1;
constexpr auto FreedesktopPowerMgmtPath      = "/org/freedesktop/PowerManagement/Inhibit"_L1;
constexpr auto FreedesktopPowerMgmtInterface = "org.freedesktop.PowerManagement.Inhibit"_L1;

constexpr auto FreedesktopPortalService   = "org.freedesktop.portal.Desktop"_L1;
constexpr auto FreedesktopPortalPath      = "/org/freedesktop/portal/desktop"_L1;
constexpr auto FreedesktopPortalInterface = "org.freedesktop.portal.Inhibit"_L1;

constexpr auto FreedesktopScreenSaverService   = "org.freedesktop.ScreenSaver"_L1;
constexpr auto FreedesktopScreenSaverPath      = "/org/freedesktop/ScreenSaver"_L1;
constexpr auto FreedesktopScreenSaverInterface = "org.freedesktop.ScreenSaver"_L1;
} // namespace DbusConstants

namespace Fooyin::SleepInhibitor {
static const auto Reason = InhibitorDbus::tr("fooyin is running");

InhibitorDbus::InhibitorDbus(QObject* parent)
    : InhibitorPrivate{parent}
{
    using namespace DbusConstants;

    m_busInterface = new QDBusInterface(GnomeSessionManagerService, GnomeSessionManagerPath,
                                        GnomeSessionManagerInterface, QDBusConnection::sessionBus(), this);
    if(m_busInterface->isValid()) {
        qCDebug(SLEEPINHIBITOR) << "Using" << GnomeSessionManagerInterface;
        m_interface = Interface::GnomeSessionManager;
        return;
    }

    delete m_busInterface;
    m_busInterface = new QDBusInterface(FreedesktopPowerMgmtService, FreedesktopPowerMgmtPath,
                                        FreedesktopPowerMgmtInterface, QDBusConnection::sessionBus(), this);
    if(m_busInterface->isValid()) {
        qCDebug(SLEEPINHIBITOR) << "Using" << FreedesktopPowerMgmtInterface;
        m_interface = Interface::FreedesktopPower;
        return;
    }

    delete m_busInterface;
    m_busInterface = new QDBusInterface(FreedesktopPortalService, FreedesktopPortalPath, FreedesktopPortalInterface,
                                        QDBusConnection::sessionBus(), this);
    if(m_busInterface->isValid()) {
        qCDebug(SLEEPINHIBITOR) << "Using" << FreedesktopPortalInterface;
        m_interface = Interface::FreedesktopPortal;
        return;
    }

    delete m_busInterface;
    m_busInterface = nullptr;
    setState(State::Error);
    qCWarning(SLEEPINHIBITOR) << "Could not get usable D-Bus interface";
}

void InhibitorDbus::inhibitScreenSaver()
{
    if(m_screenSaverError) {
        return;
    }

    if(!m_screenSaverInterface) {
        m_screenSaverInterface = new QDBusInterface(
            DbusConstants::FreedesktopScreenSaverService, DbusConstants::FreedesktopScreenSaverPath,
            DbusConstants::FreedesktopScreenSaverInterface, QDBusConnection::sessionBus(), this);
        if(!m_screenSaverInterface->isValid()) [[unlikely]] {
            m_screenSaverError = true;
            return;
        }
    }

    const auto pendingCall = m_screenSaverInterface->asyncCall("Inhibit"_L1, "fooyin"_L1, Reason);
    auto* watcher          = new QDBusPendingCallWatcher(pendingCall, this);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this,
                     &InhibitorDbus::onScreenSaverInhibitCallFinished);
}

void InhibitorDbus::uninhibitScreenSaver()
{
    if(m_screenSaverError || !m_screenSaverInhibitCookie) {
        return;
    }

    const auto pendingCall = m_screenSaverInterface->asyncCall("UnInhibit"_L1, m_screenSaverInhibitCookie);
    auto* watcher          = new QDBusPendingCallWatcher(pendingCall, this);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this,
                     &InhibitorDbus::onScreenSaverUninhibitCallFinished);
}

void InhibitorDbus::onScreenSaverInhibitCallFinished(QDBusPendingCallWatcher* watcher)
{
    const QDBusPendingReply<uint> reply = *watcher;
    watcher->deleteLater();

    if(reply.isValid()) {
        m_screenSaverInhibitCookie = reply.value();
    }
    else {
        qCWarning(SLEEPINHIBITOR) << "ScreenSaver Inhibit call error:" << reply.error().message();
        m_screenSaverError = true;
    }
}

void InhibitorDbus::onScreenSaverUninhibitCallFinished(QDBusPendingCallWatcher* watcher)
{
    const QDBusPendingReply<> reply = *watcher;
    watcher->deleteLater();

    if(reply.isValid()) {
        m_screenSaverInhibitCookie = 0;
    }
    else {
        qCWarning(SLEEPINHIBITOR) << "ScreenSaver Uninhibit call error:" << reply.error().message();
        m_screenSaverError = true;
    }
}

void InhibitorDbus::inhibitSleep(InhibitionType type)
{
    m_desiredState = State::Inhibited;
    m_desiredType  = type;

    if(state() == State::Error || state() == State::Inhibited) {
        return;
    }

    qCDebug(SLEEPINHIBITOR) << "Inhibiting sleep";

    static constexpr auto BlockLogoutFlag  = 1U;
    static constexpr auto BlockSuspendFlag = 4U;
    static constexpr auto BlockIdleFlag    = 8U;

    auto flags = BlockLogoutFlag | BlockSuspendFlag;
    if(type == InhibitionType::DisplayAndSystem) {
        flags |= BlockIdleFlag;
        if(m_interface == Interface::FreedesktopPower) {
            inhibitScreenSaver();
        }
    }

    QList<QVariant> args;
    switch(m_interface) {
        case Interface::None:
            break;
        case Interface::GnomeSessionManager: {
            static constexpr auto XWindowId = 0U;
            args                            = {"org.fooyin.fooyin"_L1, XWindowId, Reason, flags};
            break;
        }
        case Interface::FreedesktopPower:
            args = {"fooyin"_L1, Reason};
            break;
        case Interface::FreedesktopPortal: {
            QMap<QString, QVariant> options;
            options["reason"_L1] = Reason;
            // Pass empty string for parent_window
            // https://flatpak.github.io/xdg-desktop-portal/docs/window-identifiers.html
            args = {QString{}, flags, options};
            break;
        }
    }
    const QDBusPendingCall pendingCall = m_busInterface->asyncCallWithArgumentList("Inhibit"_L1, args);

    auto* watcher = new QDBusPendingCallWatcher(pendingCall, this);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, &InhibitorDbus::onInhibitCallFinished);

    m_currentType = type;
}

void InhibitorDbus::uninhibitSleep()
{
    m_desiredState = State::Uninhibited;

    if(state() != State::Inhibited) {
        return;
    }

    qCDebug(SLEEPINHIBITOR) << "Uninhibiting sleep";

    if(m_interface == Interface::FreedesktopPortal) {
        auto* inhibitRequestInterface
            = new QDBusInterface(DbusConstants::FreedesktopPortalService, m_inhibitHandle.path(),
                                 "org.freedesktop.portal.Request"_L1, QDBusConnection::sessionBus());
        if(!inhibitRequestInterface->isValid()) [[unlikely]] {
            qCWarning(SLEEPINHIBITOR) << "Bad inhibit handle? Object path:" << m_inhibitHandle.path();
            delete inhibitRequestInterface;
            setState(State::Error);
            return;
        }

        const auto pendingCall = inhibitRequestInterface->asyncCall("Close"_L1);
        auto* watcher          = new QDBusPendingCallWatcher(pendingCall, this);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, &InhibitorDbus::onUninhibitCallFinished);

        delete inhibitRequestInterface;
    }
    else {
        if(m_inhibitCookie == 0) [[unlikely]] {
            qCWarning(SLEEPINHIBITOR) << "Cookie is 0?";
            setState(State::Error);
            return;
        }

        if(m_interface == Interface::FreedesktopPower && m_currentType == InhibitionType::DisplayAndSystem) {
            uninhibitScreenSaver();
        }

        const auto pendingCall = m_busInterface->asyncCall(
            m_interface == Interface::FreedesktopPower ? "UnInhibit"_L1 : "Uninhibit"_L1, m_inhibitCookie);
        auto* watcher = new QDBusPendingCallWatcher(pendingCall, this);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, &InhibitorDbus::onUninhibitCallFinished);
    }

    m_currentType = {};
}

void InhibitorDbus::onInhibitCallFinished(QDBusPendingCallWatcher* watcher)
{
    watcher->deleteLater();

    const auto handleReply = [this]<typename T>(const QDBusPendingReply<T>& reply, T& replyValueOut) {
        if(reply.isValid()) {
            replyValueOut = reply.value();
            setState(State::Inhibited);
        }
        else {
            qCWarning(SLEEPINHIBITOR) << "Inhibit call error:" << reply.error().message();
            setState(State::Error);
        }
    };

    if(m_interface == Interface::FreedesktopPortal) {
        const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        handleReply(reply, m_inhibitHandle);
    }
    else {
        const QDBusPendingReply<uint> reply = *watcher;
        handleReply(reply, m_inhibitCookie);
    }

    if(m_desiredState == State::Uninhibited) {
        uninhibitSleep();
        m_desiredState = {};
    }
}

void InhibitorDbus::onUninhibitCallFinished(QDBusPendingCallWatcher* watcher)
{
    const QDBusPendingReply<> reply = *watcher;
    watcher->deleteLater();

    if(reply.isValid()) {
        setState(State::Uninhibited);
        m_inhibitCookie = 0;
        m_inhibitHandle = {};
    }
    else {
        qCWarning(SLEEPINHIBITOR) << "Uninhibit call error:" << reply.error().message();
        setState(State::Error);
    }

    if(m_desiredState == State::Inhibited) {
        inhibitSleep(m_desiredType);
        m_desiredState = {};
    }
}
} // namespace Fooyin::SleepInhibitor
