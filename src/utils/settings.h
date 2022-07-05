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

#pragma once

#include <QReadWriteLock>
#include <QSettings>

class Settings : public QObject
{
    Q_OBJECT

public:
    enum class Setting : qint8
    {
        Version = 0,
        DatabaseVersion,
        FirstRun,
        LayoutEditing,
        SplitterHandles,
        DiscHeaders,
        SplitDiscs,
        SimplePlaylist,
        PlayMode,
        Geometry,
        ElapsedTotal,
        FilterAltColours,
        FilterHeader,
        FilterScrollBar,
        InfoAltColours,
        InfoHeader,
        InfoScrollBar,
        PlaylistAltColours,
        PlaylistHeader,
        PlaylistScrollBar,
        Layout,
    };
    Q_ENUM(Setting);

    static Settings* instance();
    ~Settings() override;
    Settings(const Settings& other) = delete;
    Settings& operator=(const Settings& other) = delete;
    Settings(const Settings&& other) = delete;
    Settings& operator=(const Settings&& other) = delete;

    QMap<Setting, QVariant>& settings();
    void loadSettings();
    void storeSettings();

    static QVariant defaults(Setting key);
    QVariant value(Setting key);
    void set(Setting key, const QVariant& value);
    static QString getKeyString(Setting key);

signals:
    void layoutEditingChanged(bool enabled);
    void playlistSettingChanged();
    void elapsedTotalChanged(bool enabled);
    void filterAltColorsChanged(bool enabled);
    void filterHeaderChanged(bool enabled);
    void filterScrollBarChanged(bool enabled);
    void infoAltColorsChanged(bool enabled);
    void infoHeaderChanged(bool enabled);
    void infoScrollBarChanged(bool enabled);
    void playlistAltColorsChanged(bool enabled);
    void playlistHeaderChanged(bool enabled);
    void playlistScrollBarChanged(bool enabled);
    void splitterHandlesChanged(bool enabled);

protected:
    Settings();

private:
    QSettings* m_settings;
    QMap<Setting, QVariant> m_values;
    QReadWriteLock m_lock;
};
