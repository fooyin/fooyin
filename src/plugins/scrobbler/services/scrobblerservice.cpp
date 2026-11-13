/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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
#include "settings/scrobblersettings.h"

#include <core/network/networkaccessmanager.h>
#include <core/scripting/trackqueryfilter.h>
#include <utils/fypaths.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QClipboard>
#include <QCryptographicHash>
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
constexpr auto MinLovedDelay           = 500;
constexpr auto MinLovedDelayOnError    = 30000;

namespace {
bool canBeScrobbled(const Fooyin::Track& track)
{
    return track.isValid() && track.hasArtists() && !track.title().trimmed().isEmpty()
        && (track.duration() >= 30000 || (track.isRemote() && track.duration() == 0));
}
} // namespace

namespace Fooyin::Scrobbler {
ScrobblerService::ScrobblerService(ServiceDetails details, NetworkAccessManager* network, SettingsManager* settings,
                                   DbConnectionPoolPtr dbPool, QObject* parent)
    : QObject{parent}
    , m_network{network}
    , m_settings{settings}
    , m_accountStore{std::move(dbPool)}
    , m_details{std::move(details)}
    , m_authSession{nullptr}
    , m_cache{nullptr}
    , m_lovedCache{nullptr}
    , m_submitError{false}
    , m_submitFailures{0}
    , m_submitCooldown{0}
    , m_lovedSubmitError{false}
    , m_timestamp{0}
    , m_scrobbled{false}
    , m_submitted{false}
    , m_lovedSubmitted{false}
{ }

ScrobblerService::~ScrobblerService()
{
    deleteAll();
}

bool ScrobblerService::isEnabled() const
{
    return m_details.isEnabled;
}

QString ScrobblerService::name() const
{
    return m_details.name;
}

QUrl ScrobblerService::url() const
{
    return m_details.url;
}

QUrl ScrobblerService::authUrl() const
{
    return {};
}

QString ScrobblerService::username() const
{
    return {};
}

bool ScrobblerService::requiresAuthentication() const
{
    return false;
}

bool ScrobblerService::isAuthenticated() const
{
    return true;
}

bool ScrobblerService::supportsLoved() const
{
    return false;
}

bool ScrobblerService::supportsTrackStatsSync() const
{
    return false;
}

bool ScrobblerService::isCustom() const
{
    return m_details.isCustom();
}

ServiceDetails ScrobblerService::details() const
{
    return m_details;
}

void ScrobblerService::updateDetails(const ServiceDetails& details)
{
    const bool wasActive          = isEnabled() && isAuthenticated();
    const bool wasSubmittingLoved = m_details.submitLoved;
    const QString previousAccount = cacheAccount();

    if(isCustom() && details.name != m_details.name) {
        deleteSession();
    }
    if(details.token != m_details.token || details.url != m_details.url) {
        stopSubmissions();
    }
    m_details = details;
    if(isAuthenticated()) {
        synchroniseAccount();
    }

    const bool isActive = isEnabled() && isAuthenticated();
    if(!isActive) {
        stopSubmissions();
    }
    else if(!wasActive || previousAccount != cacheAccount()) {
        doDelayedSubmit();
        doDelayedLovedSubmit();
    }
    else if(details.submitLoved && !wasSubmittingLoved) {
        doDelayedLovedSubmit();
    }
}

void ScrobblerService::initialise()
{
    if(!m_cache) {
        m_cache = new ScrobblerCache(Utils::cachePath() + "/"_L1 + name().toLower() + ".cache"_L1, m_settings, this);
    }
    if(!m_lovedCache) {
        m_lovedCache = new LovedCache(Utils::cachePath() + "/"_L1 + name().toLower() + ".loved.cache"_L1, this);
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

    const QString messageTitle = tr("Authentication");
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
            case QMessageBox::Cancel:
                cleanupAuth();
                Q_EMIT authenticationFinished(false);
            default:
                break;
        }
    });

    message->show();
}

void ScrobblerService::saveSession() { }

void ScrobblerService::loadSession() { }

void ScrobblerService::deleteSession() { }

void ScrobblerService::logout() { }

void ScrobblerService::saveCache()
{
    if(m_cache) {
        m_cache->writeCache();
    }
    if(m_lovedCache) {
        m_lovedCache->writeCache();
    }
}

void ScrobblerService::resumePendingSubmissions()
{
    if(!m_cache || !m_lovedCache) {
        return;
    }

    if(isAuthenticated()) {
        synchroniseAccount();
    }

    const int pendingCount = m_cache->count();
    if(!isEnabled()) {
        qCDebug(SCROBBLER) << "Pending scrobbles will not resume for disabled service" << name() << "count"
                           << pendingCount;
        return;
    }

    if(!isAuthenticated()) {
        qCDebug(SCROBBLER) << "Pending scrobbles will not resume until authentication succeeds for" << name() << "count"
                           << pendingCount;
        return;
    }

    if(pendingCount > 0) {
        qCDebug(SCROBBLER) << "Resuming pending scrobbles for" << name() << "count" << pendingCount;
        doDelayedSubmit();
    }
    doDelayedLovedSubmit();
}

void ScrobblerService::restartScrobbleSession(const Track& track)
{
    m_currentTrack = track;
    m_timestamp    = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());
    m_scrobbled    = false;
    m_pendingScrobbleId.clear();
}

void ScrobblerService::updateNowPlaying(const Track& track)
{
    restartScrobbleSession(track);

    if(!shouldUpdateNowPlaying(track)) {
        return;
    }

    updateNowPlaying();
}

void ScrobblerService::refreshNowPlaying()
{
    if(!shouldUpdateNowPlaying(m_currentTrack)) {
        return;
    }

    updateNowPlaying();
}

void ScrobblerService::refreshMetadata(const Track& track)
{
    if(track.id() != m_currentTrack.id() || track.uniqueFilepath() != m_currentTrack.uniqueFilepath()) {
        return;
    }
    if(isAuthenticated() && !synchroniseAccount()) {
        return;
    }

    const Metadata previous{&m_scriptParser, m_settings, m_currentTrack};
    m_currentTrack = track;
    if(m_cache) {
        m_cache->updateMetadata(m_pendingScrobbleId, track);
    }

    const Metadata current{&m_scriptParser, m_settings, track};
    if(current != previous) {
        refreshNowPlaying();
    }
}

void ScrobblerService::scrobble(const Track& track)
{
    if(m_scrobbled || !settings()->value<Settings::Scrobbler::ScrobblingEnabled>()) {
        return;
    }
    if(track.isRemote() || !canBeScrobbled(track)) {
        return;
    }
    if(track.id() != m_currentTrack.id() || track.uniqueFilepath() != m_currentTrack.uniqueFilepath()) {
        return;
    }
    if(m_settings->value<Settings::Scrobbler::EnableScrobbleFilter>() && !allowedByFilter(track)) {
        return;
    }

    if(isAuthenticated() && !synchroniseAccount()) {
        return;
    }

    if(const auto* item = cache()->add(track, m_timestamp)) {
        m_pendingScrobbleId = item->id;
        setScrobbled(true);
        doDelayedSubmit(true);
    }
}

bool ScrobblerService::scrobbleStream(const Track& track, uint64_t timestamp)
{
    if(!settings()->value<Settings::Scrobbler::ScrobblingEnabled>()
       || settings()->value<Settings::Scrobbler::StreamMode>() != static_cast<int>(StreamScrobbleMode::Scrobbling)) {
        return false;
    }
    if(!track.isRemote() || !canBeScrobbled(track)) {
        return false;
    }
    if(settings()->value<Settings::Scrobbler::EnableScrobbleFilter>() && !allowedByFilter(track)) {
        return false;
    }

    if(isAuthenticated() && !synchroniseAccount()) {
        return false;
    }

    if(const auto* item = cache()->add(track, timestamp)) {
        m_pendingScrobbleId = item->id;
        cache()->writeCache();
        doDelayedSubmit(true);
        return true;
    }

    return false;
}

void ScrobblerService::updateLoved(const Track& track)
{
    if(!supportsLoved() || !m_details.submitLoved || !m_lovedCache) {
        return;
    }

    Metadata metadata{&m_scriptParser, m_settings, track};
    if(metadata.artist.isEmpty() || metadata.title.isEmpty()) {
        qCWarning(SCROBBLER) << "Unable to submit Loved state without an artist and title for" << track.filepath();
        return;
    }

    if(isAuthenticated() && !synchroniseAccount()) {
        return;
    }

    m_lovedCache->set(std::move(metadata), track.isLoved());
    doDelayedLovedSubmit(true);
}

bool ScrobblerService::hasPendingLoved(const Track& track)
{
    if(!m_details.submitLoved || !m_lovedCache) {
        return false;
    }

    const Metadata metadata{&m_scriptParser, m_settings, track};
    return m_lovedCache->contains(metadata);
}

void ScrobblerService::fetchTrackStats(const Track& /*track*/) { }

QString ScrobblerService::tokenSetting() const
{
    return {};
}

QUrl ScrobblerService::tokenUrl() const
{
    return {};
}

void ScrobblerService::setupAuthQuery(ScrobblerAuthSession* /*session*/, QUrlQuery& /*query*/) { }

void ScrobblerService::requestAuth(const QString& /*token*/) { }

void ScrobblerService::authFinished(QNetworkReply* /*reply*/) { }

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

LovedCache* ScrobblerService::lovedCache() const
{
    return m_lovedCache;
}

SettingsManager* ScrobblerService::settings() const
{
    return m_settings;
}

const AccountStore& ScrobblerService::accountStore() const
{
    return m_accountStore;
}

QString ScrobblerService::accountName() const
{
    return isCustom() ? u"Scrobbler-"_s + name() : name();
}

ServiceDetails& ScrobblerService::detailsRef()
{
    return m_details;
}

ScriptParser* ScrobblerService::scriptParser()
{
    return &m_scriptParser;
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

bool ScrobblerService::shouldUpdateNowPlaying(const Track& track)
{
    if(!settings()->value<Settings::Scrobbler::ScrobblingEnabled>()
       || !settings()->value<Settings::Scrobbler::SendNowPlaying>()) {
        return false;
    }

    if(!isAuthenticated() || !canBeScrobbled(track)
       || (track.isRemote()
           && settings()->value<Settings::Scrobbler::StreamMode>() == static_cast<int>(StreamScrobbleMode::Disabled))) {
        return false;
    }

    if(m_settings->value<Settings::Scrobbler::EnableScrobbleFilter>() && !allowedByFilter(track)) {
        return false;
    }

    return true;
}

bool ScrobblerService::allowedByFilter(const Track& track)
{
    const QString query = m_settings->value<Settings::Scrobbler::ScrobbleFilter>();
    TrackQueryFilter filter;
    return filter.filter(query, {track}).empty();
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

void ScrobblerService::submitLoved(const LovedItem& /*item*/) { }

void ScrobblerService::lovedUpdateFinished(const LovedItem& item, const LovedUpdateResult result)
{
    m_lovedSubmitted = false;

    if(result != LovedUpdateResult::Retry) {
        m_lovedCache->remove(item.key, item.revision);
    }
    m_lovedSubmitError = result == LovedUpdateResult::Retry;
    doDelayedLovedSubmit();
}

void ScrobblerService::handleTestError(const char* error)
{
    qCWarning(SCROBBLER) << error;
    Q_EMIT testApiFinished(false, QString::fromUtf8(error));
}

void ScrobblerService::handleAuthError(const char* error)
{
    qCWarning(SCROBBLER) << error;
    Q_EMIT authenticationFinished(false, QString::fromUtf8(error));
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

QString ScrobblerService::cacheAccount() const
{
    const QString identity = requiresAuthentication() ? username().trimmed().toCaseFolded() : details().token.trimmed();
    if(identity.isEmpty()) {
        return {};
    }
    const QByteArray key
        = url().adjusted(QUrl::NormalizePathSegments | QUrl::RemoveFragment | QUrl::StripTrailingSlash).toEncoded()
        + '\0' + identity.toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex());
}

void ScrobblerService::stopSubmissions()
{
    m_submitTimer.stop();
    m_lovedSubmitTimer.stop();
    deleteAll();
    m_submitted      = false;
    m_lovedSubmitted = false;
    if(m_cache) {
        for(auto* item : m_cache->items()) {
            item->submitted = false;
        }
    }
}

bool ScrobblerService::synchroniseAccount()
{
    if(!m_cache || !m_lovedCache || !isAuthenticated()) {
        return false;
    }
    const QString account = cacheAccount();
    if(account.isEmpty()) {
        return false;
    }
    if(m_cache->account() != account || m_lovedCache->account() != account) {
        stopSubmissions();
        m_submitError    = false;
        m_submitFailures = 0;
        m_submitCooldown.setRemainingTime(0);
        m_lovedSubmitError = false;
        m_cache->setAccount(account);
        m_lovedCache->setAccount(account);
    }
    return true;
}

void ScrobblerService::doDelayedSubmit(bool initial)
{
    if(isAuthenticated() && !synchroniseAccount()) {
        return;
    }

    const int pendingCount = m_cache ? m_cache->count() : 0;
    if(m_submitted) {
        if(pendingCount > 0) {
            qCDebug(SCROBBLER) << "Scrobble submit already in progress for" << name() << "pending count"
                               << pendingCount;
        }
        return;
    }

    if(pendingCount == 0) {
        return;
    }

    if(!isEnabled()) {
        return;
    }

    if(!isAuthenticated()) {
        qCDebug(SCROBBLER) << "Pending scrobbles will not submit until authentication succeeds for" << name() << "count"
                           << pendingCount;
        return;
    }

    const int scrobbleDelay = std::clamp(m_settings->value<Settings::Scrobbler::ScrobblingDelay>(), 0, 600) * 1000;

    if(initial && !m_submitError && m_submitCooldown.hasExpired() && scrobbleDelay <= 0) {
        if(m_submitTimer.isActive()) {
            m_submitTimer.stop();
        }
        qCDebug(SCROBBLER) << "Submitting pending scrobbles immediately for" << name() << "count" << pendingCount;
        submit();
    }
    else if(!m_submitTimer.isActive()) {
        const int delay
            = std::max({scrobbleDelay, MinScrobbleDelay, static_cast<int>(m_submitCooldown.remainingTime())});
        qCDebug(SCROBBLER) << "Scheduling scrobble submit for" << name() << "count" << pendingCount << "delayMs"
                           << delay << "afterError" << m_submitError;
        m_submitTimer.start(delay, this);
    }
}

void ScrobblerService::doDelayedLovedSubmit(const bool initial)
{
    if(isAuthenticated() && !synchroniseAccount()) {
        return;
    }
    if(m_lovedSubmitted || !m_lovedCache || m_lovedCache->count() == 0 || !m_details.submitLoved || !isEnabled()
       || !isAuthenticated()) {
        return;
    }

    if(initial && !m_lovedSubmitError) {
        if(m_lovedSubmitTimer.isActive()) {
            m_lovedSubmitTimer.stop();
        }
        m_lovedSubmitted = true;
        submitLoved(*m_lovedCache->first());
    }
    else if(!m_lovedSubmitTimer.isActive()) {
        m_lovedSubmitTimer.start(m_lovedSubmitError ? MinLovedDelayOnError : MinLovedDelay, this);
    }
}

void ScrobblerService::setSubmitted(bool submitted)
{
    m_submitted = submitted;
}

void ScrobblerService::setSubmitError(bool error)
{
    m_submitError    = error;
    m_submitFailures = error ? std::min(m_submitFailures + 1, 8) : 0;
    if(error) {
        setSubmitCooldown(std::min(60 * 60 * 1000, MinScrobbleDelayOnError * (1 << (m_submitFailures - 1))));
    }
}

void ScrobblerService::setSubmitCooldown(int delayMs)
{
    const auto remaining = static_cast<int>(m_submitCooldown.remainingTime());
    m_submitCooldown.setRemainingTime(std::max(remaining, delayMs));
    if(m_submitTimer.isActive()) {
        m_submitTimer.start(static_cast<int>(m_submitCooldown.remainingTime()), this);
    }
}

void ScrobblerService::setScrobbled(bool scrobbled)
{
    m_scrobbled = scrobbled;
}

void ScrobblerService::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_submitTimer.timerId()) {
        m_submitTimer.stop();
        if(!isEnabled() || !synchroniseAccount()) {
            return;
        }
        submit();
    }
    else if(event->timerId() == m_lovedSubmitTimer.timerId()) {
        m_lovedSubmitTimer.stop();

        if(m_details.submitLoved && isEnabled() && synchroniseAccount()) {
            if(const auto item = m_lovedCache->first()) {
                m_lovedSubmitted = true;
                submitLoved(*item);
            }
        }
    }
    QObject::timerEvent(event);
}
} // namespace Fooyin::Scrobbler
