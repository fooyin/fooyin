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

#include <gui/scripting/scriptformatterregistry.h>

#include <gui/scripting/scriptformatter.h>

using namespace Qt::StringLiterals;

namespace Fooyin {
using FormatFunc = std::function<void(RichFormatting&, const QString&)>;

void bold(RichFormatting& formatting, const QString& /*option*/)
{
    formatting.font.setBold(true);
}

void italic(RichFormatting& formatting, const QString& /*option*/)
{
    formatting.font.setItalic(true);
}

void fontFamily(RichFormatting& formatting, const QString& option)
{
    if(option.isEmpty()) {
        return;
    }

    formatting.font.setFamily(option);
}

void fontSize(RichFormatting& formatting, const QString& option)
{
    if(option.isEmpty()) {
        return;
    }

    bool isInt{false};
    const int size = option.toInt(&isInt);

    if(isInt) {
        formatting.font.setPointSize(size);
    }
}

void fontDelta(RichFormatting& formatting, const QString& option)
{
    if(option.isEmpty()) {
        return;
    }

    bool isInt{false};
    const int delta = option.toInt(&isInt);

    if(isInt) {
        formatting.font.setPointSize(formatting.font.pointSize() + delta);
    }
}

void colourAlpha(RichFormatting& formatting, const QString& option)
{
    if(option.isEmpty()) {
        return;
    }

    bool isInt{false};
    const int alpha = option.toInt(&isInt);

    if(isInt) {
        formatting.colour.setAlpha(alpha);
    }
}

void colourRgb(RichFormatting& formatting, const QString& option)
{
    if(option.isEmpty()) {
        return;
    }

    const QStringList rgb = option.split(","_L1);

    if(rgb.size() < 3) {
        return;
    }

    bool isInt{false};
    const int red   = rgb.at(0).toInt(&isInt);
    const int green = rgb.at(1).toInt(&isInt);
    const int blue  = rgb.at(2).toInt(&isInt);
    const int alpha = (rgb.size() == 4) ? rgb.at(3).toInt(&isInt) : 255;

    if(isInt) {
        formatting.colour.setRgb(red, green, blue, alpha);
    }
}

class ScriptFormatterRegistryPrivate
{
public:
    std::unordered_map<QString, FormatFunc> funcs{
        {u"b"_s, bold},          {u"i"_s, italic},          {u"font"_s, fontFamily}, {u"size"_s, fontSize},
        {u"sized"_s, fontDelta}, {u"alpha"_s, colourAlpha}, {u"rgb"_s, colourRgb},
    };
};

ScriptFormatterRegistry::ScriptFormatterRegistry()
    : p{std::make_unique<ScriptFormatterRegistryPrivate>()}
{ }

ScriptFormatterRegistry::~ScriptFormatterRegistry() = default;

bool ScriptFormatterRegistry::isFormatFunc(const QString& option) const
{
    return p->funcs.contains(option);
}

void ScriptFormatterRegistry::format(RichFormatting& formatting, const QString& func, const QString& option) const
{
    if(!isFormatFunc(func)) {
        return;
    }

    p->funcs.at(func)(formatting, option);
}
} // namespace Fooyin
