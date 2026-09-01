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

#include <utils/id.h>

#include <QKeySequence>
#include <QObject>

#include <vector>

namespace Fooyin::GlobalHotkeys {
enum class GlobalShortcutAvailability : uint8_t
{
    Unavailable = 0,
    Available,
    PortalManaged,
};

struct GlobalShortcutDescriptor
{
    Id commandId;
    QString description;
    QKeySequence shortcut;

    bool operator==(const GlobalShortcutDescriptor&) const = default;
};
using GlobalShortcutDescriptorList = std::vector<GlobalShortcutDescriptor>;

class GlobalShortcutBackend : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~GlobalShortcutBackend() override = default;

    [[nodiscard]] virtual GlobalShortcutAvailability availability() const    = 0;
    virtual void applyBindings(const GlobalShortcutDescriptorList& bindings) = 0;
    virtual void clearBindings()                                             = 0;

    [[nodiscard]] virtual bool configurationAvailable() const
    {
        return false;
    }
    virtual void configure() { }

Q_SIGNALS:
    void activated(const Fooyin::Id& commandId);
    void registrationFailed(const Fooyin::Id& commandId, const QKeySequence& shortcut, const QString& error);
};
} // namespace Fooyin::GlobalHotkeys
