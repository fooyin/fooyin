/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

namespace Fy::Core::Tagging {
enum class Quality
{
    Fast     = TagLib::AudioProperties::Fast,
    Standard = TagLib::AudioProperties::Average,
    Quality  = TagLib::AudioProperties::Accurate,
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

struct FileTags
{
    TagLib::Tag* tag{nullptr};
    TagLib::PropertyMap map;
    TagType type{TagType::Unknown};
};

bool isValidFile(const TagLib::FileRef& fileRef);
FileTags tagsFromFile(const TagLib::FileRef& fileRef);
QPixmap coverFromFile(const TagLib::FileRef& fileRef);
bool saveCover(const QPixmap& cover, const QString& filename);

TagLib::String convertString(const QString& string);
QString convertString(const TagLib::String& str);
TagLib::String convertString(int num);

int convertNumber(const TagLib::String& num);

TagLib::StringList convertStringList(const QStringList& str);
QStringList convertStringList(const TagLib::StringList& str);
} // namespace Fy::Core::Tagging
