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

using namespace Qt::StringLiterals;

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

    if(settings->fileValue(TagFieldSetting).isNull()) {
        loadDefaultFields();
    }
}

void TagEditorFieldRegistry::loadDefaultFields()
{
    addDefaultItem({.name = tr("Artist Name"), .scriptField = u"artist"_s, .multivalue = true}, true);
    addDefaultItem({.name = tr("Track Title"), .scriptField = u"title"_s}, true);
    addDefaultItem({.name = tr("Album Title"), .scriptField = u"album"_s}, true);
    addDefaultItem({.name = tr("Date"), .scriptField = u"date"_s}, true);
    addDefaultItem({.name = tr("Genre"), .scriptField = u"genre"_s, .multivalue = true}, true);
    addDefaultItem({.name = tr("Composer"), .scriptField = u"composer"_s, .multivalue = true}, true);
    addDefaultItem({.name = tr("Performer"), .scriptField = u"performer"_s, .multivalue = true}, true);
    addDefaultItem({.name = tr("Album Artist"), .scriptField = u"albumartist"_s, .multivalue = true}, true);
    addDefaultItem({.name = tr("Track Number"), .scriptField = u"track"_s}, true);
    addDefaultItem({.name = tr("Total Tracks"), .scriptField = u"tracktotal"_s}, true);
    addDefaultItem({.name = tr("Disc Number"), .scriptField = u"disc"_s}, true);
    addDefaultItem({.name = tr("Total Discs"), .scriptField = u"disctotal"_s}, true);
    addDefaultItem({.name = tr("Comment"), .scriptField = u"comment"_s}, true);
    addDefaultItem({.name = tr("Rating"), .scriptField = u"rating_editor"_s}, true);
}
} // namespace Fooyin::TagEditor
