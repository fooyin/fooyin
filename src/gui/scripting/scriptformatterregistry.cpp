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

namespace Fooyin {
using FormatFunc = std::function<void(FormattedTextBlock&, const QString&)>;

void bold(FormattedTextBlock& text, const QString& /*option*/)
{
    text.format.font.setBold(true);
}

void italic(FormattedTextBlock& text, const QString& /*option*/)
{
    text.format.font.setItalic(true);
}

void fontFamily(FormattedTextBlock& text, const QString& option)
{
    if(option.isEmpty()) {
        return;
    }

    text.format.font.setFamily(option);
}

void fontSize(FormattedTextBlock& text, const QString& option)
{
    if(option.isEmpty()) {
        return;
    }

    bool isInt{false};
    const int size = option.toInt(&isInt);

    if(isInt) {
        text.format.font.setPointSize(size);
    }
}

void fontDelta(FormattedTextBlock& text, const QString& option)
{
    if(option.isEmpty()) {
        return;
    }

    bool isInt{false};
    const int delta = option.toInt(&isInt);

    if(isInt) {
        text.format.font.setPointSize(text.format.font.pointSize() + delta);
    }
}

void colourAlpha(FormattedTextBlock& text, const QString& option)
{
    if(option.isEmpty()) {
        return;
    }

    bool isInt{false};
    const int alpha = option.toInt(&isInt);

    if(isInt) {
        text.format.colour.setAlpha(alpha);
    }
}

void colourRgb(FormattedTextBlock& text, const QString& option)
{
    if(option.isEmpty()) {
        return;
    }

    const QStringList rgb = option.split(QStringLiteral(","));

    if(rgb.size() < 3) {
        return;
    }

    bool isInt{false};
    const int red   = rgb.at(0).toInt(&isInt);
    const int green = rgb.at(1).toInt(&isInt);
    const int blue  = rgb.at(2).toInt(&isInt);
    const int alpha = (rgb.size() == 4) ? rgb.at(3).toInt(&isInt) : 255;

    if(isInt) {
        text.format.colour.setRgb(red, green, blue, alpha);
    }
}

struct ScriptFormatterRegistry::Private
{
    std::unordered_map<QString, FormatFunc> funcs{
        {QStringLiteral("b"), bold},          {QStringLiteral("i"), italic},
        {QStringLiteral("font"), fontFamily}, {QStringLiteral("size"), fontSize},
        {QStringLiteral("sized"), fontDelta}, {QStringLiteral("alpha"), colourAlpha},
        {QStringLiteral("rgb"), colourRgb},
    };
};

ScriptFormatterRegistry::ScriptFormatterRegistry()
    : p{std::make_unique<Private>()}
{ }

ScriptFormatterRegistry::~ScriptFormatterRegistry() = default;

bool ScriptFormatterRegistry::isFormatFunc(const QString& option) const
{
    return p->funcs.contains(option);
}

void ScriptFormatterRegistry::format(FormattedTextBlock& text, const QString& func, const QString& option) const
{
    if(!isFormatFunc(func)) {
        return;
    }

    p->funcs.at(func)(text, option);
}
} // namespace Fooyin
