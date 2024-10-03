/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "tageditorfieldregistry.h"

constexpr auto TagFieldSetting = "TagEditor/Fields";

namespace Fooyin::TagEditor {
TagEditorFieldRegistry::TagEditorFieldRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{QString::fromLatin1(TagFieldSetting), settings, parent}
{
    QObject::connect(this, &RegistryBase::itemChanged, this, [this](int id) {
        if(const auto field = itemById(id)) {
            emit fieldChanged(field.value());
        }
    });

    loadItems();

    if(settings->fileValue(QLatin1String{TagFieldSetting}).isNull()) {
        loadDefaultFields();
    }
}

void TagEditorFieldRegistry::loadDefaultFields()
{
    addDefaultItem({.name = tr("Artist Name"), .scriptField = QStringLiteral("artist"), .multivalue = true}, true);
    addDefaultItem({.name = tr("Track Title"), .scriptField = QStringLiteral("title")}, true);
    addDefaultItem({.name = tr("Album Title"), .scriptField = QStringLiteral("album")}, true);
    addDefaultItem({.name = tr("Date"), .scriptField = QStringLiteral("date")}, true);
    addDefaultItem({.name = tr("Genre"), .scriptField = QStringLiteral("genre"), .multivalue = true}, true);
    addDefaultItem({.name = tr("Composer"), .scriptField = QStringLiteral("composer"), .multivalue = true}, true);
    addDefaultItem({.name = tr("Performer"), .scriptField = QStringLiteral("performer"), .multivalue = true}, true);
    addDefaultItem({.name = tr("Album Artist"), .scriptField = QStringLiteral("albumartist"), .multivalue = true},
                   true);
    addDefaultItem({.name = tr("Track Number"), .scriptField = QStringLiteral("track")}, true);
    addDefaultItem({.name = tr("Total Tracks"), .scriptField = QStringLiteral("tracktotal")}, true);
    addDefaultItem({.name = tr("Disc Number"), .scriptField = QStringLiteral("disc")}, true);
    addDefaultItem({.name = tr("Total Discs"), .scriptField = QStringLiteral("disctotal")}, true);
    addDefaultItem({.name = tr("Comment"), .scriptField = QStringLiteral("comment")}, true);
    addDefaultItem({.name = tr("Rating"), .scriptField = QStringLiteral("rating_editor")}, true);
}
} // namespace Fooyin::TagEditor
