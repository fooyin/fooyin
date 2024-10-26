/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <core/track.h>

#include <QLoggingCategory>
#include <QObject>
#include <QPointer>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(LYRICS)

class QNetworkReply;

namespace Fooyin {
class NetworkAccessManager;
class SettingsManager;

namespace Lyrics {
struct SearchParams
{
    Track track;
    QString title;
    QString album;
    QString artist;
};

struct LyricData
{
    QString id;   // Used by some services to query lyrics after searching
    QString path; // Only useful for local sources
    QString data;

    QString title;
    QString album;
    QString artist;
    uint64_t duration{0}; // In seconds
};

class LyricSource : public QObject
{
    Q_OBJECT

public:
    explicit LyricSource(NetworkAccessManager* network, SettingsManager* settings, int index, bool enabled,
                         QObject* parent = nullptr);

    [[nodiscard]] virtual QString name() const = 0;
    [[nodiscard]] virtual int index() const;
    [[nodiscard]] virtual bool enabled() const;
    [[nodiscard]] virtual bool isLocal() const;

    virtual void search(const SearchParams& params) = 0;

    void setIndex(int index);
    void setEnabled(bool enabled);

signals:
    void searchResult(const std::vector<Fooyin::Lyrics::LyricData>& data);

protected:
    [[nodiscard]] NetworkAccessManager* network() const;
    [[nodiscard]] SettingsManager* settings() const;

    static QString toUtf8(QIODevice* file);
    static QString encode(const QString& str);

    bool getJsonFromReply(QNetworkReply* reply, QJsonObject* obj);
    bool extractJsonObj(const QByteArray& data, QJsonObject* obj);

    [[nodiscard]] QNetworkReply* reply() const;
    void setReply(QNetworkReply* reply);
    void resetReply();

private:
    NetworkAccessManager* m_network;
    SettingsManager* m_settings;
    int m_index;
    bool m_enabled;
    QPointer<QNetworkReply> m_reply;
};
} // namespace Lyrics
} // namespace Fooyin
