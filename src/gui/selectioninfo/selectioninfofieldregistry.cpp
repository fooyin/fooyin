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

#include "selectioninfofieldregistry.h"

#include <core/constants.h>
#include <utils/settings/settingsmanager.h>

#include <QSignalBlocker>

constexpr auto SelectionInfoFieldsSetting = "SelectionInfo/Fields";

namespace Fooyin {
SelectionInfoFieldRegistry::SelectionInfoFieldRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{QString::fromLatin1(SelectionInfoFieldsSetting), settings, parent}
{
    loadItems();

    if(settings->fileValue(SelectionInfoFieldsSetting).isNull()) {
        loadDefaultFields();
    }
}

void SelectionInfoFieldRegistry::loadDefaultFields()
{
    using namespace Constants::MetaData;

    addDefaultItem({.name = tr("Artist"), .scriptField = QString::fromLatin1(Artist)}, true);
    addDefaultItem({.name = tr("Title"), .scriptField = QString::fromLatin1(Title)}, true);
    addDefaultItem({.name = tr("Album"), .scriptField = QString::fromLatin1(Album)}, true);
    addDefaultItem({.name = tr("Date"), .scriptField = QString::fromLatin1(Date)}, true);
    addDefaultItem({.name = tr("Genre"), .scriptField = QString::fromLatin1(Genre)}, true);
    addDefaultItem({.name = tr("Album Artist"), .scriptField = QString::fromLatin1(AlbumArtist)}, true);
    addDefaultItem({.name = tr("Track Number"), .scriptField = QString::fromLatin1(TrackNumber)}, true);
}

void SelectionInfoFieldRegistry::resetToDefaults()
{
    {
        const QSignalBlocker blocker{this};
        reset();
        loadDefaultFields();
    }
    Q_EMIT fieldsReset();
}
} // namespace Fooyin
