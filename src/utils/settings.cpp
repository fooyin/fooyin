/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "settings.h"

#include "enumhelper.h"
#include "helpers.h"
#include "paths.h"
#include "typedefs.h"
#include "utils.h"
#include "utils/version.h"

#include <QSettings>

Settings::Settings()
{
    m_values[Setting::Version] = defaults(Setting::Version);
    m_values[Setting::DatabaseVersion] = defaults(Setting::DatabaseVersion);
    m_values[Setting::DiscHeaders] = defaults(Setting::DiscHeaders);
    m_values[Setting::SplitDiscs] = defaults(Setting::SplitDiscs);
    m_values[Setting::SimplePlaylist] = defaults(Setting::SimplePlaylist);
    m_values[Setting::LayoutEditing] = defaults(Setting::LayoutEditing);
    m_values[Setting::PlayMode] = defaults(Setting::PlayMode);
    m_values[Setting::Geometry] = defaults(Setting::Geometry);
    m_values[Setting::ElapsedTotal] = defaults(Setting::ElapsedTotal);
    m_values[Setting::FilterAltColours] = defaults(Setting::FilterAltColours);
    m_values[Setting::FilterHeader] = defaults(Setting::FilterHeader);
    m_values[Setting::FilterScrollBar] = defaults(Setting::FilterScrollBar);
    m_values[Setting::InfoAltColours] = defaults(Setting::InfoAltColours);
    m_values[Setting::InfoHeader] = defaults(Setting::InfoHeader);
    m_values[Setting::InfoScrollBar] = defaults(Setting::InfoScrollBar);
    m_values[Setting::PlaylistAltColours] = defaults(Setting::PlaylistAltColours);
    m_values[Setting::PlaylistHeader] = defaults(Setting::PlaylistHeader);
    m_values[Setting::PlaylistScrollBar] = defaults(Setting::PlaylistScrollBar);
    m_values[Setting::Layout] = defaults(Setting::Layout);

    m_settings = new QSettings(Util::settingsPath(), QSettings::IniFormat);
    if(Util::File::exists(Util::settingsPath())) {
        loadSettings();
        m_values[Setting::FirstRun] = false;
    }
    else {
        storeSettings();
        m_values[Setting::FirstRun] = true;
    }
}

Settings* Settings::instance()
{
    static Settings instance;
    return &instance;
}

Settings::~Settings() = default;

QMap<Settings::Setting, QVariant>& Settings::settings()
{
    return m_values;
}

void Settings::loadSettings()
{
    for(const auto& [key, value] : asRange(m_values)) {
        const auto keyString = getKeyString(key);
        if(!keyString.isEmpty()) {
            const auto diskValue = m_settings->value(keyString);
            if(!diskValue.isNull() && diskValue != value) {
                set(key, diskValue);
            }
        }
    }
}

void Settings::storeSettings()
{
    for(const auto& [key, value] : asRange(m_values)) {
        const auto keyString = getKeyString(key);
        if(!keyString.isEmpty()) {
            m_settings->setValue(keyString, value);
        }
    }

    m_settings->sync();
}

QVariant Settings::value(Setting key)
{
    m_lock.lockForRead();
    auto value = m_values.value(key);
    m_lock.unlock();
    return value;
}

void Settings::set(Setting key, const QVariant& value)
{
    m_lock.lockForWrite();
    m_values.insert(key, value);
    m_lock.unlock();

    switch(key) {
        case(Setting::LayoutEditing):
            return emit layoutEditingChanged();
        case(Setting::SimplePlaylist):
            return emit playlistSettingChanged();
        case(Setting::ElapsedTotal):
            return emit elapsedTotalChanged();
        case(Setting::FilterAltColours):
            return emit filterAltColorsChanged();
        case(Setting::FilterHeader):
            return emit filterHeaderChanged();
        case(Setting::FilterScrollBar):
            return emit filterScrollBarChanged();
        case(Setting::InfoAltColours):
            return emit infoAltColorsChanged();
        case(Setting::InfoHeader):
            return emit infoHeaderChanged();
        case(Setting::InfoScrollBar):
            return emit infoScrollBarChanged();
        case(Setting::PlaylistAltColours):
            return emit playlistAltColorsChanged();
        case(Setting::PlaylistHeader):
            return emit playlistHeaderChanged();
        case(Setting::PlaylistScrollBar):
            return emit playlistScrollBarChanged();
        case(Setting::DiscHeaders):
        case(Setting::SplitDiscs):
            return emit playlistSettingChanged();
        default:
            break;
    }
}

QVariant Settings::defaults(Setting key)
{
    switch(key) {
        case(Setting::Version):
            return VERSION;
        case(Setting::DatabaseVersion):
            return DATABASE_VERSION;
        case(Setting::PlayMode):
            return EnumHelper::toString(Player::PlayMode::Default);
        case(Setting::SplitDiscs):
        case(Setting::SimplePlaylist):
        case(Setting::LayoutEditing):
        case(Setting::ElapsedTotal):
        case(Setting::FilterAltColours):
        case(Setting::InfoHeader):
            return false;
        case(Setting::DiscHeaders):
        case(Setting::InfoAltColours):
        case(Setting::InfoScrollBar):
        case(Setting::FilterHeader):
        case(Setting::FilterScrollBar):
        case(Setting::PlaylistAltColours):
        case(Setting::PlaylistHeader):
        case(Setting::PlaylistScrollBar):
            return true;
        case(Setting::Layout):
            return QString("eyJMYXlvdXQiOnsiU3BsaXR0ZXIiOnsiQ2hpbGRyZW4iOlsiU3RhdHVzIiwiU2VhcmNoIix7IlNwbGl0dGVyIjp7IkN"
                           "oaWxkcmVuIjpbeyJGaWx0ZXIiOnsiVHlwZSI6IkFsYnVtQXJ0aXN0In19LCJQbGF5bGlzdCJdLCJTdGF0ZSI6IkFBQU"
                           "Evd0FBQUFFQUFBQUNBQUFCUXdBQUJoOEEvLy8vL3dFQUFBQUJBQT09IiwiVHlwZSI6Ikhvcml6b250YWwifX0sIkNvb"
                           "nRyb2xzIl0sIlN0YXRlIjoiQUFBQS93QUFBQUVBQUFBRUFBQUFHUUFBQUI0QUFBT05BQUFBRWdELy8vLy9BUUFBQUFJ"
                           "QSIsIlR5cGUiOiJWZXJ0aWNhbCJ9fX0=")
                .toUtf8();
        default:
            return {};
    }
}

QString Settings::getKeyString(Setting key)
{
    QString keyString = "";
    switch(key) {
        case(Setting::DiscHeaders):
        case(Setting::SplitDiscs):
        case(Setting::SimplePlaylist):
        case(Setting::PlaylistAltColours):
        case(Setting::PlaylistHeader):
        case(Setting::PlaylistScrollBar):
            keyString = "Playlist";
            break;
        case(Setting::FilterAltColours):
        case(Setting::FilterHeader):
        case(Setting::FilterScrollBar):
            keyString = "Filter";
            break;
        case(Setting::InfoAltColours):
        case(Setting::InfoHeader):
        case(Setting::InfoScrollBar):
            keyString = "Info";
            break;
        case(Setting::PlayMode):
        case(Setting::ElapsedTotal):
            keyString = "Player";
            break;
        case(Setting::Layout):
        case(Setting::Geometry):
            keyString = "Layout";
            break;
        case(Setting::Version):
        case(Setting::DatabaseVersion):
            break;
        case(Setting::LayoutEditing):
        case(Setting::FirstRun):
            // Don't save to disk
            return {};
    }
    keyString += "/" + EnumHelper::toString(key);
    return keyString;
}
