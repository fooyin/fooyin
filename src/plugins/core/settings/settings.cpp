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

#include "version.h"

#include <pluginsystem/pluginmanager.h>
#include <QSettings>
#include <utils/enumhelper.h>
#include <utils/helpers.h>
#include <utils/paths.h>
#include <utils/typedefs.h>
#include <utils/utils.h>

Settings::Settings(QObject* parent)
    : QObject(parent)
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
    m_values[Setting::SplitterHandles] = defaults(Setting::SplitterHandles);
    m_values[Setting::Layout] = defaults(Setting::Layout);

    m_settings = new QSettings(Util::settingsPath(), QSettings::IniFormat, this);
    if(Util::File::exists(Util::settingsPath())) {
        loadSettings();
        m_values[Setting::FirstRun] = false;
    }
    else {
        storeSettings();
        m_values[Setting::FirstRun] = true;
    }

    PluginSystem::addObject(this);
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

    bool checked = value.toBool();

    switch(key) {
        case(Setting::LayoutEditing):
            return emit layoutEditingChanged(checked);
        case(Setting::SimplePlaylist):
            return emit playlistSettingChanged();
        case(Setting::ElapsedTotal):
            return emit elapsedTotalChanged(checked);
        case(Setting::FilterAltColours):
            return emit filterAltColorsChanged(checked);
        case(Setting::FilterHeader):
            return emit filterHeaderChanged(checked);
        case(Setting::FilterScrollBar):
            return emit filterScrollBarChanged(checked);
        case(Setting::InfoAltColours):
            return emit infoAltColorsChanged(checked);
        case(Setting::InfoHeader):
            return emit infoHeaderChanged(checked);
        case(Setting::InfoScrollBar):
            return emit infoScrollBarChanged(checked);
        case(Setting::PlaylistAltColours):
            return emit playlistAltColorsChanged(checked);
        case(Setting::PlaylistHeader):
            return emit playlistHeaderChanged(checked);
        case(Setting::PlaylistScrollBar):
            return emit playlistScrollBarChanged(checked);
        case(Setting::DiscHeaders):
        case(Setting::SplitDiscs):
            return emit playlistSettingChanged();
        case(Setting::SplitterHandles):
            return emit splitterHandlesChanged(checked);
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
        case(Setting::SplitterHandles):
            return true;
        case(Setting::Layout):
            return "";
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
        case(Setting::SplitterHandles):
            keyString = "Splitter";
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
