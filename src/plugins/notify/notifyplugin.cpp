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

#include "settings/notifypage.h"
#include "settings/notifysettings.h"

#include <core/player/playercontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QLoggingCategory>
#include <QPixmap>

Q_LOGGING_CATEGORY(NOTIFY, "fy.notify")

using namespace Qt::StringLiterals;

constexpr auto DbusService   = "org.freedesktop.Notifications";
constexpr auto DbusPath      = "/org/freedesktop/Notifications";
constexpr auto DbusInterface = "org.freedesktop.Notifications";

namespace {
struct ImageData
{
    int width{0};
    int height{0};
    int rowstride{0};
    bool hasAlpha{false};
    int bitsPerSample{0};
    int channels{0};
    QByteArray data;
};

QDBusArgument& operator<<(QDBusArgument& argument, const ImageData& image)
{
    argument.beginStructure();
    argument << image.width;
    argument << image.height;
    argument << image.rowstride;
    argument << image.hasAlpha;
    argument << image.bitsPerSample;
    argument << image.channels;
    argument << image.data;
    argument.endStructure();

    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, ImageData& image)
{
    argument.beginStructure();
    argument >> image.width;
    argument >> image.height;
    argument >> image.rowstride;
    argument >> image.hasAlpha;
    argument >> image.bitsPerSample;
    argument >> image.channels;
    argument >> image.data;
    argument.endStructure();

    return argument;
}
} // namespace

Q_DECLARE_METATYPE(ImageData)

namespace Fooyin::Notify {
NotifyPlugin::NotifyPlugin()
{
    qDBusRegisterMetaType<ImageData>();
}

void NotifyPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_audioLoader      = context.audioLoader;
    m_settings         = context.settingsManager;

    m_notifySettings = std::make_unique<NotifySettings>(m_settings);

    const bool connected = QDBusConnection::sessionBus().connect(
        QString::fromLatin1(DbusService), QString::fromLatin1(DbusPath), QString::fromLatin1(DbusInterface),
        u"NotificationClosed"_s, this, SLOT(notificationClosed(uint, uint)));

    if(!connected) {
        qCWarning(NOTIFY) << "Failed to connect to NotificationClosed signal";
    }

    QObject::connect(m_playerController, &PlayerController::playlistTrackChanged, this, &NotifyPlugin::trackChanged);
}

void NotifyPlugin::initialise(const GuiPluginContext& /*context*/)
{
    m_coverProvider = new CoverProvider(m_audioLoader, m_settings, this);
    m_coverProvider->setUsePlaceholder(false);

    m_notifyPage = new NotifyPage(m_settings, this);
}

void NotifyPlugin::shutdown()
{
    // Nothing to clean up
}

void NotifyPlugin::notificationClosed(uint id, uint /*reason*/)
{
    if(id == m_lastNotificationId) {
        m_lastNotificationId = 0;
    }
}

void NotifyPlugin::trackChanged(const PlaylistTrack& playlistTrack)
{
    if(!m_settings->value<Settings::Notify::Enabled>()) {
        return;
    }

    if(!playlistTrack.isValid()) {
        return;
    }

    const Track& track = playlistTrack.track;

    if(!m_coverProvider || !m_settings->value<Settings::Notify::ShowAlbumArt>()) {
        showNotification(track, QPixmap{});
        return;
    }

    m_coverProvider->trackCoverFull(track, Track::Cover::Front).then([this, track](const QPixmap& cover) {
        showNotification(track, cover);
    });
}

void NotifyPlugin::showNotification(const Track& track, const QPixmap& cover)
{
    const QString titleField = m_settings->value<Settings::Notify::TitleField>();
    const QString bodyField  = m_settings->value<Settings::Notify::BodyField>();

    const QString title = m_scriptParser.evaluate(titleField, track);
    const QString body  = m_scriptParser.evaluate(bodyField, track);

    sendDbusNotification(title, body, cover);
}

void NotifyPlugin::sendDbusNotification(const QString& title, const QString& body, const QPixmap& cover)
{
    QDBusInterface iface(QString::fromLatin1(DbusService), QString::fromLatin1(DbusPath),
                         QString::fromLatin1(DbusInterface), QDBusConnection::sessionBus());

    if(!iface.isValid()) {
        qCWarning(NOTIFY) << "Failed to connect to D-Bus notification service";
        return;
    }

    const QString appName = QApplication::applicationDisplayName();
    const int timeout     = m_settings->value<Settings::Notify::Timeout>();
    const QStringList actions;
    QVariantMap hints;

    // Add album art as image-data hint if available
    if(!cover.isNull()) {
        const QImage image = cover.toImage()
                                 .scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                                 .convertToFormat(QImage::Format_RGBA8888);

        const QByteArray imageBytes{reinterpret_cast<const char*>(image.constBits()),
                                    static_cast<int>(image.sizeInBytes())};

        const ImageData imageData{
            .width         = image.width(),
            .height        = image.height(),
            .rowstride     = static_cast<int>(image.bytesPerLine()),
            .hasAlpha      = true,
            .bitsPerSample = 8,
            .channels      = 4,
            .data          = imageBytes,
        };

        hints[u"image-data"_s] = QVariant::fromValue(imageData);
    }

    QDBusReply<uint> reply
        = iface.call(u"Notify"_s, appName, m_lastNotificationId, QString{}, title, body, actions, hints, timeout);

    if(reply.isValid()) {
        m_lastNotificationId = reply.value();
        qCDebug(NOTIFY) << "Notification sent:" << title << "-" << body;
    }
    else {
        qCWarning(NOTIFY) << "Failed to send notification:" << reply.error().message();
    }
}
} // namespace Fooyin::Notify

#include "moc_notifyplugin.cpp"
