/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

bool parseRgbComponent(const QString& value, int* component)
{
    bool isInt{false};
    const int parsed = value.trimmed().toInt(&isInt);
    if(!isInt || parsed < 0 || parsed > 255) {
        return false;
    }

    *component = parsed;
    return true;
}

bool parseRgb(const QString& option, QColor* colour)
{
    if(option.isEmpty()) {
        return false;
    }

    const QStringList parts = option.split(u","_s);
    if(parts.size() < 3 || parts.size() > 4) {
        return false;
    }

    int red{0};
    int green{0};
    int blue{0};
    int alpha{255};

    if(!parseRgbComponent(parts.at(0), &red) || !parseRgbComponent(parts.at(1), &green)
       || !parseRgbComponent(parts.at(2), &blue)) {
        return false;
    }

    if(parts.size() == 4 && !parseRgbComponent(parts.at(3), &alpha)) {
        return false;
    }

    colour->setRgb(red, green, blue, alpha);
    return true;
}

bool parseColour(const QString& option, QColor* colour)
{
    if(option.isEmpty()) {
        return false;
    }

    const QString trimmed{option.trimmed()};

    const QColor namedOrHex = QColor::fromString(trimmed);
    if(namedOrHex.isValid()) {
        *colour = namedOrHex;
        return true;
    }

    return parseRgb(trimmed, colour);
}

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
    QColor colour;
    if(parseRgb(option, &colour)) {
        formatting.colour = colour;
        return true;
    }

    return false;
}

bool colourGeneric(RichFormatting& formatting, const QString& option)
{
    QColor colour;
    if(parseColour(option, &colour)) {
        formatting.colour = colour;
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
    return true;
}

bool alignRight(RichFormatting& formatting, const QString& /*option*/)
{
    formatting.alignment = RichAlignment::Right;
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
    FormatterHandlerEntry{.name = "rgba"_L1, .handler = &colourRgb},
    FormatterHandlerEntry{.name = "color"_L1, .handler = &colourGeneric},
    FormatterHandlerEntry{.name = "a"_L1, .handler = &linkHref},
    FormatterHandlerEntry{.name = "right"_L1, .handler = &alignRight},
};
} // namespace

bool ScriptFormatterRegistry::isKnown(const QString& func)
{
    return std::ranges::any_of(FormatterHandlers, [&](const auto& entry) { return func == entry.name; });
}

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
