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

#include "metadatalookupsource.h"

#include <core/coresettings.h>

#include <QBasicTimer>
#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>
#include <QUrl>

#include <deque>
#include <optional>
#include <unordered_map>

class QNetworkAccessManager;
class QNetworkReply;

namespace Fooyin {
class SettingsManager;

class FYGUI_EXPORT MusicBrainzMetadata : public MetadataLookupSource
{
    Q_OBJECT

public:
    explicit MusicBrainzMetadata(QNetworkAccessManager* network, QObject* parent = nullptr);
    ~MusicBrainzMetadata() override;

    [[nodiscard]] QString id() const override;
    [[nodiscard]] QString name() const override;
    [[nodiscard]] std::vector<LookupMode> supportedModes() const override;
    void search(const LookupQuery& query) override;
    void fetchRelease(const QString& releaseId) override;
    void cancel() override;

    static QString buildSearchExpression(const LookupQuery& query);
    static QUrl buildSearchUrl(const LookupQuery& query, int limit);
    static bool parseSearchResponse(const QByteArray& data, std::vector<ReleaseSummary>& releases, QString& error);
    static bool parseReleaseResponse(const QByteArray& data, Release& release, QString& error);

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    enum class OperationType : uint8_t
    {
        Search,
        Release,
    };

    struct Operation
    {
        OperationType type;
        QUrl url;
        uint64_t generation{0};
        int retries{0};
    };

    void enqueue(Operation operation);
    void startNext();
    void startOperation(const Operation& operation);
    void handleReply();
    void finishOperation();
    void retryOperation(Operation operation, int statusCode, const QByteArray& retryAfter);
    void setBusy(bool busy);

    FySettings m_settings;

    QBasicTimer m_startTimer;
    QElapsedTimer m_lastStarted;
    QPointer<QNetworkReply> m_reply;
    std::unordered_map<QString, Release> m_releaseCache;
    std::deque<Operation> m_queue;
    std::optional<Operation> m_active;
    uint64_t m_generation;
    bool m_busy;
};
} // namespace Fooyin
