/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <QDialog>

class QListWidget;
class QPushButton;

namespace Fooyin {
class FyLayout;
struct FyTheme;
class LayoutProvider;
struct PlaylistPreset;
class PresetRegistry;
class SettingsManager;
class ThemeRegistry;

class QuickSetupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QuickSetupDialog(LayoutProvider* layoutProvider, ThemeRegistry* themeRegistry,
                              PresetRegistry* presetRegistry, SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;

Q_SIGNALS:
    void layoutChanged(const Fooyin::FyLayout& layout);
    void systemThemeRequested();
    void themeChanged(const Fooyin::FyTheme& theme);
    void playlistPresetChanged(const Fooyin::PlaylistPreset& preset);

private:
    enum ItemRole
    {
        Id = Qt::UserRole
    };

    void populateLayouts() const;
    void populateThemes() const;
    void populatePlaylistPresets() const;
    void changeLayout();
    void changeTheme();
    void changePlaylistPreset();

    LayoutProvider* m_layoutProvider;
    ThemeRegistry* m_themeRegistry;
    PresetRegistry* m_presetRegistry;
    SettingsManager* m_settings;

    QListWidget* m_layoutList;
    QListWidget* m_themeList;
    QListWidget* m_playlistPresetList;
    QPushButton* m_accept;
};
} // namespace Fooyin
