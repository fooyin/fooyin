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
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <set>

Q_DECLARE_LOGGING_CATEGORY(ARTWORK)

class QNetworkReply;

namespace Fooyin {
class NetworkAccessManager;
class SettingsManager;

struct SearchParams
{
    Track::Cover type;
    QString artist;
    QString album;
    QString title;
};
using CoverTypes = std::set<Track::Cover>;

struct SearchResult
{
    QString artist;
    QString album;
    QUrl imageUrl;
};
using SearchResults = std::vector<SearchResult>;

struct ArtworkResult
{
    QString mimeType;
    QByteArray image;
};

class ArtworkSource : public QObject
{
    Q_OBJECT

public:
    ArtworkSource(NetworkAccessManager* network, SettingsManager* settings, int index, bool enabled,
                  QObject* parent = nullptr);

    [[nodiscard]] virtual QString name() const = 0;
    [[nodiscard]] virtual int index() const;
    [[nodiscard]] virtual bool enabled() const;
    [[nodiscard]] virtual CoverTypes supportedTypes() const = 0;

    virtual void search(const SearchParams& params) = 0;

    void setIndex(int index);
    void setEnabled(bool enabled);

    static QImage readImage(QByteArray data);
    static QImage readImage(const QString& path, int requestedSize, const char* hintType);

signals:
    void searchResult(const Fooyin::SearchResults& results);

protected:
    [[nodiscard]] NetworkAccessManager* network() const;
    [[nodiscard]] SettingsManager* settings() const;

    static QString toUtf8(QIODevice* file);
    static QString encode(const QString& str);

    static QNetworkRequest createRequest(const QUrl& url, const std::map<QString, QString>& params,
                                         const QString& secret = {});
    static bool getJsonFromReply(QNetworkReply* reply, QJsonObject* obj);
    static bool extractJsonObj(const QByteArray& data, QJsonObject* obj);

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
} // namespace Fooyin
