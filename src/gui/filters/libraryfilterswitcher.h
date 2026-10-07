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
 *
 */

#pragma once

#include <gui/fywidget.h>

class QComboBox;
class QContextMenuEvent;
class QLabel;

namespace Fooyin {
struct LibraryFilter;
class MusicLibrary;
class SettingsManager;

namespace Filters {
class LibraryFilterRegistry;

class LibraryFilterSwitcher : public FyWidget
{
    Q_OBJECT

public:
    LibraryFilterSwitcher(LibraryFilterRegistry* registry, MusicLibrary* library, SettingsManager* settings,
                          QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;
    void populateContextMenu(QMenu* menu) override;
    void finalise() override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void populate();
    void filterChanged(const LibraryFilter& filter);
    void activateCurrent();
    void setShowLabel(bool showLabel);
    void showContextMenu(const QPoint& globalPos);

    LibraryFilterRegistry* m_registry;
    MusicLibrary* m_library;
    SettingsManager* m_settings;

    QLabel* m_label;
    QComboBox* m_presets;
    QString m_allLibraryName;
    bool m_showLabel;
    bool m_rememberLastFilter;
    int m_lastFilterId;
};
} // namespace Filters
} // namespace Fooyin
