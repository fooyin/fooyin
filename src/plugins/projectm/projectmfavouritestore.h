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

#include <QObject>
#include <QString>

#include <vector>

namespace Fooyin {
class SettingsManager;

namespace ProjectM {
class ProjectMFavouriteStore : public QObject
{
    Q_OBJECT

public:
    explicit ProjectMFavouriteStore(SettingsManager* settings, QObject* parent = nullptr);

    [[nodiscard]] bool contains(const QString& identifier) const;
    [[nodiscard]] const std::vector<QString>& favourites() const;

    void setFavourite(const QString& identifier, bool favourite);

Q_SIGNALS:
    void favouriteChanged(const QString& identifier, bool favourite);

private:
    void save() const;

    SettingsManager* m_settings;
    std::vector<QString> m_favourites;
};
} // namespace ProjectM
} // namespace Fooyin
