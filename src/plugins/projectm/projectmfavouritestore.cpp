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

#include "projectmfavouritestore.h"

#include <utils/settings/settingsmanager.h>

#include <QDir>
#include <QStringList>

using namespace Qt::StringLiterals;

constexpr auto FavouritesKey = "ProjectM/Favourites"_L1;

namespace Fooyin::ProjectM {
namespace {
QString normaliseIdentifier(QString identifier)
{
    identifier = QDir::fromNativeSeparators(identifier.trimmed());
    while(identifier.startsWith('/'_L1)) {
        identifier.remove(0, 1);
    }
    return identifier.isEmpty() ? QString{} : QDir::cleanPath(identifier);
}

bool identifiersEqual(const QString& lhs, const QString& rhs)
{
#ifdef Q_OS_WIN
    return lhs.compare(rhs, Qt::CaseInsensitive) == 0;
#else
    return lhs == rhs;
#endif
}
} // namespace

ProjectMFavouriteStore::ProjectMFavouriteStore(SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_settings{settings}
{
    const QStringList favourites = m_settings->fileValue(FavouritesKey).toStringList();
    for(const QString& favourite : favourites) {
        const QString identifier = normaliseIdentifier(favourite);
        if(!identifier.isEmpty() && !contains(identifier)) {
            m_favourites.push_back(identifier);
        }
    }
}

bool ProjectMFavouriteStore::contains(const QString& identifier) const
{
    const QString normalised = normaliseIdentifier(identifier);
    return !normalised.isEmpty() && std::ranges::any_of(m_favourites, [&normalised](const QString& favourite) {
        return identifiersEqual(favourite, normalised);
    });
}

const std::vector<QString>& ProjectMFavouriteStore::favourites() const
{
    return m_favourites;
}

void ProjectMFavouriteStore::setFavourite(const QString& identifier, bool favourite)
{
    const QString normalised = normaliseIdentifier(identifier);
    if(normalised.isEmpty()) {
        return;
    }

    const auto existing = std::ranges::find_if(
        m_favourites, [&normalised](const QString& stored) { return identifiersEqual(stored, normalised); });
    if((existing != m_favourites.end()) == favourite) {
        return;
    }

    if(favourite) {
        m_favourites.push_back(normalised);
    }
    else {
        m_favourites.erase(existing);
    }

    save();
    Q_EMIT favouriteChanged(normalised, favourite);
}

void ProjectMFavouriteStore::save() const
{
    if(m_favourites.empty()) {
        m_settings->fileRemove(FavouritesKey);
        return;
    }

    QStringList favourites;
    favourites.reserve(static_cast<qsizetype>(m_favourites.size()));

    for(const QString& favourite : m_favourites) {
        favourites.push_back(favourite);
    }

    favourites.sort(Qt::CaseInsensitive);
    m_settings->fileSet(FavouritesKey, favourites);
}
} // namespace Fooyin::ProjectM
