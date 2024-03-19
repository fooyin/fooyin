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

#include "fygui_export.h"

#include <QColor>
#include <QFont>

namespace Fooyin {
class ScriptFormatterRegistry;

struct Formatting
{
    QFont font;
    QColor colour;

    bool operator==(const Formatting& other) const
    {
        return std::tie(font, colour) == std::tie(other.font, other.colour);
    };
};

struct FormattedTextBlock
{
    QString text;
    Formatting format;

    bool operator==(const FormattedTextBlock& other) const
    {
        return std::tie(text, format) == std::tie(other.text, other.format);
    };
};
using FormattedText = std::vector<FormattedTextBlock>;

class FYGUI_EXPORT ScriptFormatter
{
public:
    ScriptFormatter();
    ~ScriptFormatter();

    FormattedText evaluate(const QString& input);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
