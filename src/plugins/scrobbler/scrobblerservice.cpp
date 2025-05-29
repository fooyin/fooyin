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

#include "scrobblerservice.h"

#include "scrobblerauthsession.h"
#include "scrobblersettings.h"

#include <core/network/networkaccessmanager.h>
#include <utils/fypaths.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QNetworkReply>
#include <QPointer>
#include <QPushButton>
#include <QTcpServer>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(SCROBBLER, "fy.scrobbler")

using namespace Qt::StringLiterals;

constexpr auto MinScrobbleDelay        = 5000;
constexpr auto MinScrobbleDelayOnError = 30000;

namespace {
bool canBeScrobbled(const Fooyin::Track& track)
{
    return track.isValid() && !track.artists().empty() && !track.title().isEmpty() && track.duration() >= 30000;
}
} // namespace

namespace Fooyin::Scrobbler {
ScrobblerService::ScrobblerService(NetworkAccessManager* network, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_network{network}
    , m_settings{settings}
    , m_authSession{nullptr}
    , m_cache{nullptr}
    , m_submitError{false}
    , m_timestamp{0}
    , m_scrobbled{false}
    , m_submitted{false}
{ }

ScrobblerService::~ScrobblerService()
{
    deleteAll();
}

QString ScrobblerService::username() const
{
    return {};
}

void ScrobblerService::initialise()
{
    if(!m_cache) {
        m_cache = new ScrobblerCache(Utils::cachePath() + "/"_L1 + name().toLower() + ".cache"_L1, this);
    }
}

void ScrobblerService::authenticate()
{
    if(!m_authSession) {
        m_authSession = new ScrobblerAuthSession(this);
        QObject::connect(m_authSession, &ScrobblerAuthSession::tokenReceived, this,
                         [this](const QString& token) { requestAuth(token); });
    }

    QUrlQuery query;
    setupAuthQuery(m_authSession, query);

    QUrl url{authUrl()};
    url.setQuery(query);

    const QString messageTitle = tr("%1 Authentication").arg(name());
    const QString messageSubtitle
        = tr("Open url in web browser?") + u"<br /><br /><a href=\"%1\">%1</a><br />"_s.arg(url.toString());

    const QPointer<QMessageBox> message
        = new QMessageBox(QMessageBox::Information, messageTitle, messageSubtitle, QMessageBox::Cancel);
    message->setAttribute(Qt::WA_DeleteOnClose);
    message->setTextFormat(Qt::RichText);

    auto* openButton = new QPushButton(tr("Open"), message);
    message->addButton(openButton, QMessageBox::AcceptRole);
    auto* copyButton = new QPushButton(tr("Copy URL"), message);
    message->addButton(copyButton, QMessageBox::ActionRole);

    QObject::connect(openButton, &QPushButton::clicked, this, [url]() { QDesktopServices::openUrl(url); });
    QObject::connect(copyButton, &QPushButton::clicked, this,
                     [url]() { QApplication::clipboard()->setText(url.toString()); });

    QObject::connect(this, &ScrobblerService::authenticationFinished, this, [this, message](const bool success) {
        if(success) {
            if(message) {
                message->deleteLater();
            }
            cleanupAuth();
        }
    });
    QObject::connect(message, &QMessageBox::finished, this, [this, url](const int result) {
        switch(result) {
            case(QMessageBox::Cancel):
                cleanupAuth();
                emit authenticationFinished(false);
            default:
                break;
        }
    });

    message->show();
}

void ScrobblerService::saveCache()
{
    m_cache->writeCache();
}

void ScrobblerService::updateNowPlaying(const Track& track)
{
    m_currentTrack = track;
    m_timestamp    = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());
    m_scrobbled    = false;

    if(!settings()->value<Settings::Scrobbler::ScrobblingEnabled>()) {
        return;
    }

    if(!isAuthenticated() || !canBeScrobbled(track)) {
        return;
    }

    updateNowPlaying();
}

void ScrobblerService::scrobble(const Track& track)
{
    if(!settings()->value<Settings::Scrobbler::ScrobblingEnabled>()) {
        return;
    }

    if(!canBeScrobbled(track)) {
        return;
    }

    if(track.id() != m_currentTrack.id() || track.uniqueFilepath() != m_currentTrack.uniqueFilepath()) {
        return;
    }

    setScrobbled(true);
    cache()->add(track, m_timestamp);

    if(!isAuthenticated()) {
        return;
    }

    doDelayedSubmit(true);
}

QString ScrobblerService::tokenSetting() const
{
    return {};
}

QUrl ScrobblerService::tokenUrl() const
{
    return {};
}

Track ScrobblerService::currentTrack() const
{
    return m_currentTrack;
}

NetworkAccessManager* ScrobblerService::network() const
{
    return m_network;
}

ScrobblerAuthSession* ScrobblerService::authSession() const
{
    return m_authSession;
}

ScrobblerCache* ScrobblerService::cache() const
{
    return m_cache;
}

SettingsManager* ScrobblerService::settings() const
{
    return m_settings;
}

QNetworkReply* ScrobblerService::addReply(QNetworkReply* reply)
{
    return m_replies.emplace_back(reply);
}

bool ScrobblerService::removeReply(QNetworkReply* reply)
{
    if(std::erase(m_replies, reply) == 0) {
        return false;
    }

    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->deleteLater();
    return true;
}

bool ScrobblerService::extractJsonObj(const QByteArray& data, QJsonObject* obj, QString* errorDesc)
{
    QJsonParseError error;
    const QJsonDocument json = QJsonDocument::fromJson(data, &error);

    if(errorDesc && error.error != QJsonParseError::NoError) {
        *errorDesc = error.errorString();
        return false;
    }

    if(json.isObject()) {
        *obj = json.object();
    }

    return true;
}

void ScrobblerService::deleteAll()
{
    for(auto* reply : m_replies) {
        QObject::disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }

    m_replies.clear();
    cleanupAuth();
}

void ScrobblerService::handleAuthError(const char* error)
{
    qCWarning(SCROBBLER) << error;
    emit authenticationFinished(false, QString::fromUtf8(error));
    cleanupAuth();
}

void ScrobblerService::cleanupAuth()
{
    if(m_authSession) {
        QObject::disconnect(m_authSession, nullptr, this, nullptr);
        m_authSession->deleteLater();
        m_authSession = nullptr;
    }
}

void ScrobblerService::doDelayedSubmit(bool initial)
{
    if(m_submitted || m_cache->count() == 0) {
        return;
    }

    const int scrobbleDelay = m_settings->value<Settings::Scrobbler::ScrobblingDelay>();

    if(initial && !m_submitError && scrobbleDelay <= 0) {
        if(m_submitTimer.isActive()) {
            m_submitTimer.stop();
        }
        submit();
    }
    else if(!m_submitTimer.isActive()) {
        const int delay = std::max(scrobbleDelay, m_submitError ? MinScrobbleDelayOnError : MinScrobbleDelay);
        m_submitTimer.start(delay, this);
    }
}

void ScrobblerService::setSubmitted(bool submitted)
{
    m_submitted = submitted;
}

void ScrobblerService::setSubmitError(bool error)
{
    m_submitError = error;
}

void ScrobblerService::setScrobbled(bool scrobbled)
{
    m_scrobbled = scrobbled;
}

void ScrobblerService::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_submitTimer.timerId()) {
        m_submitTimer.stop();
        submit();
    }
    QObject::timerEvent(event);
}
} // namespace Fooyin::Scrobbler
