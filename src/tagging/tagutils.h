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

#include <taglib/audioproperties.h>
#include <taglib/tpropertymap.h>

class QString;
class QPixmap;

namespace TagLib {
class FileRef;
class String;
class StringList;
class Tag;
} // namespace TagLib

#include <QStringList>

namespace Tagging {

Q_NAMESPACE

enum class Quality
{
    Fast = TagLib::AudioProperties::Fast,
    Standard = TagLib::AudioProperties::Average,
    Quality = TagLib::AudioProperties::Accurate,
};

enum class TagType
{
    ID3v1 = 0,
    ID3v2,
    Xiph,
    MP4,
    Unsupported,
    Unknown
};
Q_ENUM_NS(TagType)

struct FileTags
{
    TagLib::Tag* tag{nullptr};
    TagLib::PropertyMap map;
    TagType type{TagType::Unknown};
};

bool isValidFile(const TagLib::FileRef& fileRef);
FileTags tagsFromFile(const TagLib::FileRef& fileRef);
QPixmap coverFromFile(const TagLib::FileRef& fileRef);

QString convertString(const TagLib::String& str);
QStringList convertStringList(const TagLib::StringList& str);
}; // namespace Tagging
