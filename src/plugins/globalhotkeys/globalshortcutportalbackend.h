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

#pragma once

#include "globalshortcutportaltypes.h"

#include "globalshortcutbackend.h"

#include <QDBusObjectPath>

#include <map>
#include <memory>
#include <set>

class QDBusInterface;
class QDBusPendingCallWatcher;

namespace Fooyin::GlobalHotkeys {
class GlobalShortcutPortalBackend : public GlobalShortcutBackend
{
    Q_OBJECT

public:
    explicit GlobalShortcutPortalBackend(QObject* parent = nullptr);
    ~GlobalShortcutPortalBackend() override;

    [[nodiscard]] GlobalShortcutAvailability availability() const override;
    void applyBindings(const GlobalShortcutDescriptorList& bindings) override;
    void clearBindings() override;
    [[nodiscard]] bool configurationAvailable() const override;
    void configure() override;

private Q_SLOTS:
    void requestResponse(uint response, const QVariantMap& results);
    void shortcutActivated(const QDBusObjectPath& sessionHandle, const QString& shortcutId, qulonglong timestamp,
                           const QVariantMap& options);
    void sessionClosed();

private:
    enum class RequestStage : uint8_t
    {
        None = 0,
        CreateSession,
        BindShortcuts,
    };

    void createSession();
    void bindShortcuts();
    void watchCall(QDBusPendingCallWatcher* watcher, const QString& requestPath);
    void beginRequest(const QString& requestPath, RequestStage stage);
    void endRequest();
    void closeRequest();
    void closeSession();
    void failBindings(const QString& error);

    QString nextToken(const QString& prefix);

    std::unique_ptr<QDBusInterface> m_portal;
    GlobalShortcutDescriptorList m_bindings;
    PortalShortcutList m_portalShortcuts;
    std::map<QString, GlobalShortcutDescriptor> m_shortcutDescriptors;
    std::set<QString> m_boundShortcutIds;
    QString m_requestPath;
    QString m_sessionPath;
    RequestStage m_requestStage;
    uint64_t m_token;
    bool m_available;
    bool m_configurationAvailable;
    bool m_configurePending{false};
};
} // namespace Fooyin::GlobalHotkeys
