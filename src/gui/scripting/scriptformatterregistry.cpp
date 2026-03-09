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

using namespace Qt::StringLiterals;

namespace Fooyin {
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

void linkHref(RichFormatting& formatting, const QString& option)
{
    if(option.isEmpty()) {
        return;
    }

    formatting.link = option;
    formatting.font.setUnderline(true);
    formatting.colour = QApplication::palette().link().color();
}

class ScriptFormatterRegistryPrivate
{ };

ScriptFormatterRegistry::ScriptFormatterRegistry()
    : p{std::make_unique<ScriptFormatterRegistryPrivate>()}
{ }

ScriptFormatterRegistry::~ScriptFormatterRegistry() = default;

bool ScriptFormatterRegistry::format(RichFormatting& formatting, const QString& func, const QString& option) const
{
    if(func == "b"_L1) {
        bold(formatting, option);
        return true;
    }
    if(func == "i"_L1) {
        italic(formatting, option);
        return true;
    }
    if(func == "font"_L1) {
        fontFamily(formatting, option);
        return true;
    }
    if(func == "size"_L1) {
        fontSize(formatting, option);
        return true;
    }
    if(func == "sized"_L1) {
        fontDelta(formatting, option);
        return true;
    }
    if(func == "alpha"_L1) {
        colourAlpha(formatting, option);
        return true;
    }
    if(func == "rgb"_L1) {
        colourRgb(formatting, option);
        return true;
    }
    if(func == "a"_L1) {
        linkHref(formatting, option);
        return !formatting.link.isEmpty();
    }

    return false;
}
} // namespace Fooyin
