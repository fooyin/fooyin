/*
 * Fooyin
 * Copyright 2025, ripdog <https://github.com/ripdog>
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

#include "notifyplugin.h"

#include "backends/freedesktopnotificationbackend.h"
#include "backends/portalnotificationbackend.h"
#include "settings/notifypage.h"
#include "settings/notifysettings.h"

#include <core/coresettings.h>
#include <core/player/playercontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QDBusMetaType>
#include <QPixmap>

using namespace Qt::StringLiterals;

constexpr auto PreviousActionId  = "previous";
constexpr auto PlayPauseActionId = "play-pause";
constexpr auto NextActionId      = "next";

namespace Fooyin::Notify {
NotifyPlugin::NotifyPlugin()
    : m_playerController{nullptr}
    , m_settings{nullptr}
    , m_coverProvider{nullptr}
    , m_activeBackend{nullptr}
    , m_notifyPage{nullptr}
    , m_notificationGeneration{0}
    , m_notificationRequestInFlight{false}
{ }

void NotifyPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_audioLoader      = context.audioLoader;
    m_settings         = context.settingsManager;
}

void NotifyPlugin::initialise(const GuiPluginContext& context)
{
    m_coverProvider = new CoverProvider(context.coverRepository, this);
    m_coverProvider->setUsePlaceholder(false);

    m_notifySettings = std::make_unique<NotifySettings>(m_settings);
    m_notifyPage     = new NotifyPage(m_settings, this, this);

    qDBusRegisterMetaType<ImageData>();
    qDBusRegisterMetaType<PortalButtons>();

    initialiseNotificationBackends();

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &NotifyPlugin::trackChanged);
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this, &NotifyPlugin::playStateChanged);
}

bool NotifyPlugin::supportsAlbumArt()
{
    initialiseNotificationBackends();
    return currentCapabilities().albumArt;
}

bool NotifyPlugin::supportsPlaybackControls()
{
    initialiseNotificationBackends();
    return currentCapabilities().playbackControls;
}

bool NotifyPlugin::supportsTimeout()
{
    initialiseNotificationBackends();
    return currentCapabilities().timeout;
}

void NotifyPlugin::playStateChanged(Player::PlayState state)
{
    if(state == Player::PlayState::Stopped) {
        resetNotificationIdentities();
    }
}

void NotifyPlugin::trackChanged(const Track& track)
{
    ++m_notificationGeneration;

    if(!m_settings->value<Settings::Notify::Enabled>()) {
        return;
    }

    if(!track.isValid()) {
        resetNotificationIdentities();
        return;
    }

    initialiseNotificationBackends();

    if(!currentCapabilities().albumArt || !m_settings->value<Settings::Notify::ShowAlbumArt>()) {
        showNotification(track, QPixmap{});
        return;
    }

    m_coverProvider->trackCoverFull(track, Track::Cover::Front)
        .then(this, [this, loadGeneration = m_notificationGeneration, track](const QPixmap& cover) {
            if(loadGeneration == m_notificationGeneration && m_settings->value<Settings::Notify::Enabled>()) {
                showNotification(track, cover);
            }
        });
}

void NotifyPlugin::showNotification(const Track& track, const QPixmap& cover)
{
    const QString titleField = m_settings->value<Settings::Notify::TitleField>();
    const QString bodyField  = m_settings->value<Settings::Notify::BodyField>();

    const QString title = m_scriptParser.evaluate(titleField, track);
    const QString body  = m_scriptParser.evaluate(bodyField, track);

    sendNotification(title, body, cover);
}

void NotifyPlugin::sendNotification(const QString& title, const QString& body, const QPixmap& cover)
{
    m_pendingNotification = PendingNotification{
        .title = title,
        .body  = body,
        .cover = cover,
    };

    if(m_notificationRequestInFlight) {
        return;
    }

    sendPendingNotification();
}

void NotifyPlugin::sendPendingNotification()
{
    if(!m_pendingNotification.has_value()) {
        return;
    }

    initialiseNotificationBackends();

    const PendingNotification notification = std::move(*m_pendingNotification);
    m_pendingNotification.reset();
    m_notificationRequestInFlight = true;

    if(!m_activeBackend) {
        qCWarning(NOTIFY) << "Failed to connect to any notification backend";
        m_notificationRequestInFlight = false;
        return;
    }

    const NotificationRequest request{
        .title     = notification.title,
        .body      = notification.body,
        .cover     = notification.cover,
        .actions   = notificationActions(),
        .timeoutMs = m_settings->value<Settings::Notify::Timeout>(),
    };

    m_activeBackend->sendNotification(request);
}

void NotifyPlugin::initialiseNotificationBackends()
{
    if(!m_backends.empty()) {
        return;
    }

    m_backends = {new FreedesktopNotificationBackend(this), new PortalNotificationBackend(this)};

    for(int index{0}; std::cmp_less(index, m_backends.size()); ++index) {
        auto* backend = m_backends.at(index);

        QObject::connect(backend, &NotificationBackend::actionInvoked, this, &NotifyPlugin::handleNotificationAction);
        QObject::connect(backend, &NotificationBackend::notificationSent, this,
                         [this, backend](bool success) { notificationSent(backend, success); });

        if(!m_activeBackend && backend->isValid()) {
            m_activeBackend = backend;
        }
    }
}

NotificationCapabilities NotifyPlugin::currentCapabilities() const
{
    return m_activeBackend ? m_activeBackend->capabilities() : NotificationCapabilities{};
}

PlaybackControls NotifyPlugin::playbackControls() const
{
    return PlaybackControls::fromInt(m_settings->value<Settings::Notify::Controls>());
}

void NotifyPlugin::resetNotificationIdentities()
{
    for(auto* backend : std::as_const(m_backends)) {
        backend->resetIdentities();
    }
}

QList<NotificationAction> NotifyPlugin::notificationActions() const
{
    const PlaybackControls controls = playbackControls();
    if(controls == PlaybackControls{} || !currentCapabilities().playbackControls) {
        return {};
    }

    const QString playPauseLabel
        = m_playerController->playState() == Player::PlayState::Playing ? tr("Pause") : tr("Play");

    QList<NotificationAction> actions;

    if(controls.testFlag(PlaybackControlFlag::Previous) && m_playerController->hasPreviousTrack()) {
        actions << NotificationAction{
            .id    = QString::fromLatin1(PreviousActionId),
            .label = tr("Previous"),
        };
    }

    if(controls.testFlag(PlaybackControlFlag::PlayPause)) {
        actions << NotificationAction{
            .id    = QString::fromLatin1(PlayPauseActionId),
            .label = playPauseLabel,
        };
    }

    if(controls.testFlag(PlaybackControlFlag::Next) && m_playerController->hasNextTrack()) {
        actions << NotificationAction{
            .id    = QString::fromLatin1(NextActionId),
            .label = tr("Next"),
        };
    }

    return actions;
}

void NotifyPlugin::handleNotificationAction(const QString& actionKey)
{
    if(m_settings->value<Settings::Core::Shutdown>()) {
        return;
    }

    if(actionKey == QString::fromLatin1(PreviousActionId)) {
        m_playerController->previous();
    }
    else if(actionKey == QString::fromLatin1(PlayPauseActionId)) {
        m_playerController->playPause();
    }
    else if(actionKey == QString::fromLatin1(NextActionId)) {
        m_playerController->next();
    }
}

void NotifyPlugin::notificationSent(NotificationBackend* backend, bool success)
{
    m_notificationRequestInFlight = false;

    if(!success && backend == m_activeBackend) {
        qCWarning(NOTIFY) << "Notification delivery failed on active backend";
    }

    if(m_pendingNotification.has_value()) {
        sendPendingNotification();
    }
}
} // namespace Fooyin::Notify

#include "moc_notifyplugin.cpp"
