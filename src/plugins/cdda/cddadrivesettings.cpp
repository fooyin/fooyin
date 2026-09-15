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
 */

#include "cddadrivesettings.h"

#include <utils/settings/settingsmanager.h>

#include <QCryptographicHash>
#include <QLoggingCategory>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(CDDA_SETTINGS, "fy.cdda.settings")

namespace Fooyin::Cdda {
CdDriveSettings normaliseDriveSettings(CdDriveSettings settings)
{
    settings.readOffsetFrames
        = std::clamp(settings.readOffsetFrames, -MaximumReadOffsetFrames, MaximumReadOffsetFrames);

    switch(settings.security) {
        case CdRippingSecurity::Disabled:
        case CdRippingSecurity::Standard:
        case CdRippingSecurity::Paranoid:
            break;
        default:
            settings.security = CdRippingSecurity::Disabled;
            break;
    }

    settings.readSpeedLimit = std::clamp(settings.readSpeedLimit, 0, MaximumReadSpeed);
    return settings;
}

QString cdDriveSettingsKey(const QString& backend, QString vendor, QString model, const QString& location)
{
    const QString backendName = backend.simplified().toCaseFolded();
    vendor                    = vendor.simplified().toCaseFolded();
    model                     = model.simplified().toCaseFolded();

    const auto encodedIdentityPart = [](const QString& value) {
        return QString::fromLatin1(
            value.toUtf8().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
    };

    if(!model.isEmpty()) {
        return u"%1-model:%2:%3"_s.arg(backendName, encodedIdentityPart(vendor), encodedIdentityPart(model));
    }
    if(location.isEmpty()) {
        return {};
    }
    return u"%1-location:%2"_s.arg(backendName, encodedIdentityPart(location));
}

QString cdDriveSettingsGroup(const QString& settingsKey)
{
    const QByteArray digest = QCryptographicHash::hash(settingsKey.toUtf8(), QCryptographicHash::Sha256)
                                  .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return u"CDDA-Drive-%1"_s.arg(QString::fromLatin1(digest));
}

CdDriveSettings CdDriveSettingsStore::settingsForDrive(const CdDriveInfo& drive) const
{
    CdDriveSettings result;
    if(drive.settingsKey.isEmpty()) {
        return result;
    }

    const QString prefix    = cdDriveSettingsGroup(drive.settingsKey);
    result.readOffsetFrames = m_settings.value(prefix + u"/ReadOffsetFrames"_s, 0).toInt();
    result.security         = static_cast<CdRippingSecurity>(
        m_settings.value(prefix + u"/Security"_s, static_cast<int>(CdRippingSecurity::Disabled)).toInt());
    result.readSpeedLimit = m_settings.value(prefix + u"/ReadSpeedLimit"_s, 0).toInt();

    return normaliseDriveSettings(result);
}

bool CdDriveSettingsStore::hasSettingsForDrive(const CdDriveInfo& drive) const
{
    return !drive.settingsKey.isEmpty()
        && m_settings.contains(cdDriveSettingsGroup(drive.settingsKey) + u"/Identity"_s);
}

void CdDriveSettingsStore::setSettingsForDrive(const CdDriveInfo& drive, const CdDriveSettings& settings)
{
    if(drive.settingsKey.isEmpty()) {
        qCWarning(CDDA_SETTINGS) << "Cannot save CD drive settings without a stable identity:" << drive.id;
        return;
    }

    const CdDriveSettings normalised = normaliseDriveSettings(settings);

    const QString prefix = cdDriveSettingsGroup(drive.settingsKey);
    m_settings.setValue(prefix + u"/Identity"_s, drive.settingsKey);
    m_settings.setValue(prefix + u"/ReadOffsetFrames"_s, normalised.readOffsetFrames);
    m_settings.setValue(prefix + u"/Security"_s, static_cast<int>(normalised.security));
    m_settings.setValue(prefix + u"/ReadSpeedLimit"_s, normalised.readSpeedLimit);
}

} // namespace Fooyin::Cdda
