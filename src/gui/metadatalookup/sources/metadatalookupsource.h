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

#include "fygui_export.h"

#include "gui/metadatalookup/metadatatypes.h"

#include <QObject>

class QNetworkAccessManager;

namespace Fooyin {
class SettingsManager;

class FYGUI_EXPORT MetadataLookupSource : public QObject
{
    Q_OBJECT

public:
    explicit MetadataLookupSource(QNetworkAccessManager* network, QObject* parent = nullptr);

    [[nodiscard]] virtual QString id() const                             = 0;
    [[nodiscard]] virtual QString name() const                           = 0;
    [[nodiscard]] virtual std::vector<LookupMode> supportedModes() const = 0;

    virtual void search(const LookupQuery& query)       = 0;
    virtual void fetchRelease(const QString& releaseId) = 0;

    virtual void cancel() = 0;

Q_SIGNALS:
    void searchFinished(const std::vector<Fooyin::ReleaseSummary>& results);
    void releaseFetched(const Fooyin::Release& release);
    void failed(const QString& message);
    void busyChanged(bool busy);

protected:
    [[nodiscard]] QNetworkAccessManager* network() const;

private:
    QNetworkAccessManager* m_network;
};
} // namespace Fooyin
