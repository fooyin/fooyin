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

#include <QApplication>
#include <QPalette>

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
using FormatterHandler = bool (*)(RichFormatting&, const QString&);

struct FormatterHandlerEntry
{
    QLatin1StringView name;
    FormatterHandler handler;
};

bool bold(RichFormatting& formatting, const QString& /*option*/)
{
    formatting.font.setBold(true);
    return true;
}

bool italic(RichFormatting& formatting, const QString& /*option*/)
{
    formatting.font.setItalic(true);
    return true;
}

bool fontFamily(RichFormatting& formatting, const QString& option)
{
    if(option.isEmpty()) {
        return false;
    }

    formatting.font.setFamily(option);
    return true;
}

bool fontSize(RichFormatting& formatting, const QString& option)
{
    if(option.isEmpty()) {
        return false;
    }

    bool isInt{false};
    const int size = option.toInt(&isInt);

    if(isInt) {
        formatting.font.setPointSize(size);
        return true;
    }

    return false;
}

bool fontDelta(RichFormatting& formatting, const QString& option)
{
    if(option.isEmpty()) {
        return false;
    }

    bool isInt{false};
    const int delta = option.toInt(&isInt);

    if(isInt) {
        formatting.font.setPointSize(formatting.font.pointSize() + delta);
        return true;
    }

    return false;
}

bool colourAlpha(RichFormatting& formatting, const QString& option)
{
    if(option.isEmpty()) {
        return false;
    }

    bool isInt{false};
    const int alpha = option.toInt(&isInt);

    if(isInt) {
        formatting.colour.setAlpha(alpha);
        return true;
    }

    return false;
}

bool colourRgb(RichFormatting& formatting, const QString& option)
{
    if(option.isEmpty()) {
        return false;
    }

    const QStringList rgb = option.split(","_L1);

    if(rgb.size() < 3) {
        return false;
    }

    bool isInt{false};
    const int red   = rgb.at(0).toInt(&isInt);
    const int green = rgb.at(1).toInt(&isInt);
    const int blue  = rgb.at(2).toInt(&isInt);
    const int alpha = (rgb.size() == 4) ? rgb.at(3).toInt(&isInt) : 255;

    if(isInt) {
        formatting.colour.setRgb(red, green, blue, alpha);
        return true;
    }

    return false;
}

bool linkHref(RichFormatting& formatting, const QString& option)
{
    if(option.isEmpty()) {
        return false;
    }

    formatting.link = option;
    formatting.font.setUnderline(true);
    formatting.colour = QApplication::palette().link().color();
    return true;
}

constexpr std::array FormatterHandlers{
    FormatterHandlerEntry{.name = "b"_L1, .handler = &bold},
    FormatterHandlerEntry{.name = "i"_L1, .handler = &italic},
    FormatterHandlerEntry{.name = "font"_L1, .handler = &fontFamily},
    FormatterHandlerEntry{.name = "size"_L1, .handler = &fontSize},
    FormatterHandlerEntry{.name = "sized"_L1, .handler = &fontDelta},
    FormatterHandlerEntry{.name = "alpha"_L1, .handler = &colourAlpha},
    FormatterHandlerEntry{.name = "rgb"_L1, .handler = &colourRgb},
    FormatterHandlerEntry{.name = "a"_L1, .handler = &linkHref},
};
} // namespace

bool ScriptFormatterRegistry::format(RichFormatting& formatting, const QString& func, const QString& option)
{
    for(const auto& entry : FormatterHandlers) {
        if(func == entry.name) {
            return entry.handler(formatting, option);
        }
    }

    return false;
}
} // namespace Fooyin
