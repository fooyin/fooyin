/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "globalshortcutportalbackend.h"

#include "globalshortcutkeyutils.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QLoggingCategory>

#include <map>

Q_LOGGING_CATEGORY(GLOBAL_SHORTCUTS, "fy.globalshortcuts")

using namespace Qt::StringLiterals;

constexpr auto PortalService          = "org.freedesktop.portal.Desktop"_L1;
constexpr auto PortalPath             = "/org/freedesktop/portal/desktop"_L1;
constexpr auto PortalInterface        = "org.freedesktop.portal.GlobalShortcuts"_L1;
constexpr auto PortalRequestInterface = "org.freedesktop.portal.Request"_L1;
constexpr auto PortalSessionInterface = "org.freedesktop.portal.Session"_L1;

namespace Fooyin::GlobalHotkeys {
namespace {
QString requestPath(const QString& token)
{
    QString sender = QDBusConnection::sessionBus().baseService();
    sender.remove(0, 1);
    sender.replace(u'.', u'_');
    return u"/org/freedesktop/portal/desktop/request/"_s + sender + u'/' + token;
}
} // namespace

GlobalShortcutPortalBackend::GlobalShortcutPortalBackend(QObject* parent)
    : GlobalShortcutBackend{parent}
    , m_portal{std::make_unique<QDBusInterface>(PortalService, PortalPath, PortalInterface,
                                                QDBusConnection::sessionBus())}
    , m_requestStage{RequestStage::None}
    , m_token{0}
    , m_available{false}
    , m_configurationAvailable{false}
{
    qDBusRegisterMetaType<PortalShortcut>();
    qDBusRegisterMetaType<PortalShortcutList>();

    QObject::connect(&m_repeater, &GlobalShortcutRepeater::activated, this, &GlobalShortcutBackend::activated);

    QDBusConnection bus = QDBusConnection::sessionBus();
    m_available         = bus.isConnected() && m_portal->isValid();
    if(!m_available) {
        qCWarning(GLOBAL_SHORTCUTS) << "The desktop global shortcuts portal is unavailable";
        return;
    }

    m_configurationAvailable = m_portal->property("version").toUInt() >= 2;

    bus.connect(PortalService, PortalPath, PortalInterface, u"Activated"_s, this,
                // clang-format off
                SLOT(shortcutActivated(QDBusObjectPath,QString,qulonglong,QVariantMap)));
    bus.connect(PortalService, PortalPath, PortalInterface, u"Deactivated"_s, this,
                SLOT(shortcutDeactivated(QDBusObjectPath,QString,qulonglong,QVariantMap)));
    // clang-format on
}

GlobalShortcutPortalBackend::~GlobalShortcutPortalBackend()
{
    clearBindings();
}

GlobalShortcutAvailability GlobalShortcutPortalBackend::availability() const
{
    return m_available ? GlobalShortcutAvailability::PortalManaged : GlobalShortcutAvailability::Unavailable;
}

void GlobalShortcutPortalBackend::applyBindings(const GlobalShortcutDescriptorList& bindings)
{
    if(bindings == m_bindings) {
        return;
    }

    closeRequest();
    closeSession();
    m_bindings = bindings;

    if(!m_available || m_bindings.empty()) {
        return;
    }

    m_portalShortcuts.clear();
    m_shortcutDescriptors.clear();
    m_boundShortcutIds.clear();

    for(const auto& binding : m_bindings) {
        const QString shortcutId = binding.commandId.name();
        QVariantMap options{{u"description"_s, binding.description}};
        if(!binding.shortcut.isEmpty()) {
            const QString trigger = xdgShortcutTrigger(binding.shortcut);
            if(trigger.isEmpty()) {
                Q_EMIT registrationFailed(binding.commandId, binding.shortcut,
                                          tr("The shortcut cannot be represented by the Wayland portal"));
                continue;
            }
            options.insert(u"preferred_trigger"_s, trigger);
        }

        m_portalShortcuts.append({.id = shortcutId, .options = options});
        m_shortcutDescriptors.emplace(shortcutId, binding);
    }

    if(!m_portalShortcuts.empty()) {
        createSession();
    }
}

void GlobalShortcutPortalBackend::clearBindings()
{
    closeRequest();
    closeSession();

    m_bindings.clear();
    m_portalShortcuts.clear();
    m_shortcutDescriptors.clear();
    m_boundShortcutIds.clear();
    m_configurePending = false;
}

bool GlobalShortcutPortalBackend::configurationAvailable() const
{
    return m_configurationAvailable;
}

void GlobalShortcutPortalBackend::configure()
{
    if(!m_configurationAvailable || m_bindings.empty()) {
        return;
    }

    if(m_sessionPath.isEmpty() || m_requestStage != RequestStage::None) {
        m_configurePending = true;
        return;
    }

    m_portal->asyncCall(u"ConfigureShortcuts"_s, QVariant::fromValue(QDBusObjectPath{m_sessionPath}), QString{},
                        QVariantMap{});
}

void GlobalShortcutPortalBackend::createSession()
{
    const QString handleToken  = nextToken(u"fooyin_global_shortcuts"_s);
    const QString sessionToken = nextToken(u"fooyin_global_shortcuts_session"_s);
    const QVariantMap options{{u"handle_token"_s, handleToken}, {u"session_handle_token"_s, sessionToken}};

    const QString path = requestPath(handleToken);
    beginRequest(path, RequestStage::CreateSession);
    watchCall(new QDBusPendingCallWatcher(m_portal->asyncCall(u"CreateSession"_s, options), this), path);
}

void GlobalShortcutPortalBackend::bindShortcuts()
{
    const QString handleToken = nextToken(u"fooyin_global_shortcuts"_s);
    const QVariantMap options{{u"handle_token"_s, handleToken}};
    const QString path = requestPath(handleToken);

    beginRequest(path, RequestStage::BindShortcuts);
    watchCall(new QDBusPendingCallWatcher(
                  m_portal->asyncCall(u"BindShortcuts"_s, QVariant::fromValue(QDBusObjectPath{m_sessionPath}),
                                      QVariant::fromValue(m_portalShortcuts), QString{}, options),
                  this),
              path);
}

void GlobalShortcutPortalBackend::watchCall(QDBusPendingCallWatcher* watcher, const QString& requestPath)
{
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, requestPath](auto* finishedWatcher) {
        const QDBusPendingReply<QDBusObjectPath> reply = *finishedWatcher;
        finishedWatcher->deleteLater();
        if(reply.isError() && requestPath == m_requestPath) {
            failBindings(reply.error().message());
        }
    });
}

void GlobalShortcutPortalBackend::beginRequest(const QString& path, RequestStage stage)
{
    m_requestPath  = path;
    m_requestStage = stage;

    QDBusConnection::sessionBus().connect(PortalService, path, PortalRequestInterface, u"Response"_s, this,
                                          // clang-format off
                                          SLOT(requestResponse(uint,QVariantMap)));
    // clang-format on
}

void GlobalShortcutPortalBackend::endRequest()
{
    if(m_requestPath.isEmpty()) {
        return;
    }

    QDBusConnection::sessionBus().disconnect(PortalService, m_requestPath, PortalRequestInterface, u"Response"_s, this,
                                             // clang-format off
                                             SLOT(requestResponse(uint,QVariantMap)));
    // clang-format on

    m_requestPath.clear();
    m_requestStage = RequestStage::None;
}

void GlobalShortcutPortalBackend::closeRequest()
{
    if(m_requestPath.isEmpty()) {
        return;
    }

    QDBusInterface request{PortalService, m_requestPath, PortalRequestInterface, QDBusConnection::sessionBus()};
    request.asyncCall(u"Close"_s);
    endRequest();
}

void GlobalShortcutPortalBackend::closeSession()
{
    m_repeater.clear();

    if(m_sessionPath.isEmpty()) {
        m_boundShortcutIds.clear();
        return;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.disconnect(PortalService, m_sessionPath, PortalSessionInterface, u"Closed"_s, this, SLOT(sessionClosed()));
    QDBusInterface session{PortalService, m_sessionPath, PortalSessionInterface, bus};
    session.asyncCall(u"Close"_s);

    m_sessionPath.clear();
    m_boundShortcutIds.clear();
}

void GlobalShortcutPortalBackend::failBindings(const QString& error)
{
    qCWarning(GLOBAL_SHORTCUTS) << "Global shortcut registration failed:" << error;

    endRequest();
    closeSession();

    for(const auto& binding : m_bindings) {
        Q_EMIT registrationFailed(binding.commandId, binding.shortcut, error);
    }
}

QString GlobalShortcutPortalBackend::nextToken(const QString& prefix)
{
    return prefix + u'_' + QString::number(++m_token);
}

void GlobalShortcutPortalBackend::requestResponse(uint response, const QVariantMap& results)
{
    const RequestStage stage{m_requestStage};
    endRequest();

    if(response != 0) {
        failBindings(response == 1 ? tr("Global shortcut registration was cancelled")
                                   : tr("The Wayland portal rejected the global shortcuts"));
        return;
    }

    if(stage == RequestStage::CreateSession) {
        m_sessionPath = results.value(u"session_handle"_s).toString();

        if(m_sessionPath.isEmpty()) {
            failBindings(tr("The Wayland portal returned an invalid shortcut session"));
            return;
        }

        QDBusConnection::sessionBus().connect(PortalService, m_sessionPath, PortalSessionInterface, u"Closed"_s, this,
                                              SLOT(sessionClosed()));
        bindShortcuts();
        return;
    }

    if(stage != RequestStage::BindShortcuts) {
        return;
    }

    const auto boundShortcuts = qdbus_cast<PortalShortcutList>(results.value(u"shortcuts"_s));
    m_boundShortcutIds.clear();

    for(const auto& shortcut : boundShortcuts) {
        m_boundShortcutIds.emplace(shortcut.id);
    }

    for(const auto& [shortcutId, binding] : m_shortcutDescriptors) {
        if(!m_boundShortcutIds.contains(shortcutId)) {
            Q_EMIT registrationFailed(binding.commandId, binding.shortcut,
                                      tr("The Wayland portal did not bind this shortcut"));
        }
    }

    if(std::exchange(m_configurePending, false)) {
        configure();
    }
}

void GlobalShortcutPortalBackend::shortcutActivated(const QDBusObjectPath& sessionHandle, const QString& shortcutId,
                                                    qulonglong /*timestamp*/, const QVariantMap& /*options*/)
{
    const auto binding = m_shortcutDescriptors.find(shortcutId);
    if(sessionHandle.path() != m_sessionPath || !m_boundShortcutIds.contains(shortcutId)
       || binding == m_shortcutDescriptors.cend()) {
        return;
    }

    m_repeater.press(shortcutId, binding->second.commandId);
}

void GlobalShortcutPortalBackend::shortcutDeactivated(const QDBusObjectPath& sessionHandle, const QString& shortcutId,
                                                      qulonglong /*timestamp*/, const QVariantMap& /*options*/)
{
    if(sessionHandle.path() == m_sessionPath) {
        m_repeater.release(shortcutId);
    }
}

void GlobalShortcutPortalBackend::sessionClosed()
{
    m_repeater.clear();
    m_sessionPath.clear();
    m_boundShortcutIds.clear();

    if(!m_bindings.empty()) {
        failBindings(tr("The Wayland global shortcut session was closed"));
    }
}
} // namespace Fooyin::GlobalHotkeys

#include "moc_globalshortcutportalbackend.cpp"
