/*
 * Fooyin
 * Copyright 2026, Luke Taylor <luket@pm.me>
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

#include <QDBusArgument>
#include <QList>
#include <QLoggingCategory>
#include <QObject>
#include <QPixmap>
#include <QString>

namespace Fooyin::Notify {
Q_DECLARE_LOGGING_CATEGORY(NOTIFY)

struct NotificationAction
{
    QString id;
    QString label;
};

struct NotificationCapabilities
{
    bool albumArt{false};
    bool playbackControls{false};
    bool timeout{false};
};

struct NotificationRequest
{
    QString title;
    QString body;
    QPixmap cover;
    QList<NotificationAction> actions;
    int timeoutMs{0};
    int maxAlbumArtSize{128};
};

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

using PortalButtons = QList<QVariantMap>;

class NotificationBackend : public QObject
{
    Q_OBJECT

public:
    explicit NotificationBackend(QObject* parent = nullptr);
    ~NotificationBackend() override;

    [[nodiscard]] virtual bool isValid() const                          = 0;
    [[nodiscard]] virtual NotificationCapabilities capabilities() const = 0;

    virtual void sendNotification(const NotificationRequest& request) = 0;
    virtual void resetIdentities()                                    = 0;

Q_SIGNALS:
    void actionInvoked(const QString& actionKey);
    void notificationSent(bool success);
};
} // namespace Fooyin::Notify

QDBusArgument& operator<<(QDBusArgument& argument, const Fooyin::Notify::ImageData& image);
const QDBusArgument& operator>>(const QDBusArgument& argument, Fooyin::Notify::ImageData& image);

Q_DECLARE_METATYPE(Fooyin::Notify::ImageData)
Q_DECLARE_METATYPE(Fooyin::Notify::PortalButtons)
