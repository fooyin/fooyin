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

#pragma once

#include <core/tagging/tagparser.h>

#include <QtPlugin>

namespace Fooyin {
struct TagParserContext
{
    QString name;
    std::unique_ptr<TagParser> parser;
};

/*!
 * An abstract interface for plugins which add a parser for tags/metadata.
 */
class TagParserPlugin
{
public:
    virtual ~TagParserPlugin() = default;

    /*!
     * This is called after all core plugins have been initialised.
     * This must return the name of the parser and a  a unique_ptr
     * to a TagParser subclass.
     */
    virtual TagParserContext registerTagParser() = 0;
};
} // namespace Fooyin

Q_DECLARE_INTERFACE(Fooyin::TagParserPlugin, "com.fooyin.plugin.tagging.parser")
