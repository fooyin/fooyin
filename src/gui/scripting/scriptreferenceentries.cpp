/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "scriptreferenceentries.h"

#include <core/scripting/scriptregistry.h>
#include <gui/scripting/scriptcommandhandler.h>

#include <QCoreApplication>

using namespace Qt::StringLiterals;

namespace {
QString translate(const char* sourceText)
{
    return QCoreApplication::translate("Fooyin", sourceText);
}

Fooyin::ScriptReferenceEntry formattingEntry(const char* name, const char* signature, const char* category,
                                             const char* description, int cursorOffset)
{
    const QString tagName = QString::fromLatin1(name);
    const QString sig     = QString::fromLatin1(signature);

    return {.kind         = Fooyin::ScriptReferenceKind::Formatting,
            .label        = sig,
            .insertText   = u"%1</%2>"_s.arg(sig, tagName),
            .category     = translate(category),
            .description  = translate(description),
            .cursorOffset = cursorOffset};
}
} // namespace

namespace Fooyin {
std::vector<ScriptReferenceEntry> scriptReferenceEntries(const ScriptRegistry& registry)
{
    static const std::vector Formatting = {
        formattingEntry("hr", "<hr/>", QT_TRANSLATE_NOOP("Fooyin", "Layout"),
                        QT_TRANSLATE_NOOP("Fooyin", "Draws a separator in supported views"), 7),
        formattingEntry("left", "<left>", QT_TRANSLATE_NOOP("Fooyin", "Alignment"),
                        QT_TRANSLATE_NOOP("Fooyin", "Aligns enclosed text to the left in supported views"), 7),
        formattingEntry("right", "<right>", QT_TRANSLATE_NOOP("Fooyin", "Alignment"),
                        QT_TRANSLATE_NOOP("Fooyin", "Aligns enclosed text to the right in supported views"), 8),
        formattingEntry("b", "<b>", QT_TRANSLATE_NOOP("Fooyin", "Style"),
                        QT_TRANSLATE_NOOP("Fooyin", "Makes the enclosed text bold"), 4),
        formattingEntry("i", "<i>", QT_TRANSLATE_NOOP("Fooyin", "Style"),
                        QT_TRANSLATE_NOOP("Fooyin", "Makes the enclosed text italic"), 4),
        formattingEntry("u", "<u>", QT_TRANSLATE_NOOP("Fooyin", "Style"),
                        QT_TRANSLATE_NOOP("Fooyin", "Underlines the enclosed text"), 4),
        formattingEntry("s", "<s>", QT_TRANSLATE_NOOP("Fooyin", "Style"),
                        QT_TRANSLATE_NOOP("Fooyin", "Strikes through the enclosed text"), 4),
        formattingEntry("font", "<font=sans>", QT_TRANSLATE_NOOP("Fooyin", "Style"),
                        QT_TRANSLATE_NOOP("Fooyin", "Sets the font family for the enclosed text"), 7),
        formattingEntry("weight", "<weight=600>", QT_TRANSLATE_NOOP("Fooyin", "Style"),
                        QT_TRANSLATE_NOOP("Fooyin", "Sets the font weight from 1 to 1000"), 9),
        formattingEntry("size", "<size=12>", QT_TRANSLATE_NOOP("Fooyin", "Style"),
                        QT_TRANSLATE_NOOP("Fooyin", "Sets the font size in points"), 7),
        formattingEntry("sized", "<sized=2>", QT_TRANSLATE_NOOP("Fooyin", "Style"),
                        QT_TRANSLATE_NOOP("Fooyin", "Adjusts the current font size by a positive or negative delta"),
                        8),
        formattingEntry("alpha", "<alpha=180>", QT_TRANSLATE_NOOP("Fooyin", "Colour"),
                        QT_TRANSLATE_NOOP("Fooyin", "Sets the text alpha channel from 0 to 255"), 8),
        formattingEntry("rgb", "<rgb=255,0,0>", QT_TRANSLATE_NOOP("Fooyin", "Colour"),
                        QT_TRANSLATE_NOOP("Fooyin", "Sets the text colour from red, green and blue components"), 6),
        formattingEntry("rgba", "<rgba=255,0,0,255>", QT_TRANSLATE_NOOP("Fooyin", "Colour"),
                        QT_TRANSLATE_NOOP("Fooyin", "Sets the text colour from red, green, blue, and alpha components"),
                        7),
        formattingEntry("color", "<color=red>", QT_TRANSLATE_NOOP("Fooyin", "Colour"),
                        QT_TRANSLATE_NOOP("Fooyin", "Sets the text colour from a named colour or hex code"), 8),
    };

    std::vector<ScriptReferenceEntry> entries;
    entries.reserve(Formatting.size() + registry.variables().size() + registry.functions().size());

    const auto addVariable = [&entries](const ScriptVariableDescriptor& variable, const QString& name) {
        const QString label = u"%%1%"_s.arg(name.toLower());
        entries.push_back(
            {.kind         = ScriptReferenceKind::Variable,
             .label        = label,
             .insertText   = label,
             .category     = variable.category.isEmpty()
                               ? translate("Plugins")
                               : QCoreApplication::translate("Fooyin", variable.category.toUtf8().constData()),
             .description  = QCoreApplication::translate("Fooyin", variable.description.toUtf8().constData()),
             .cursorOffset = 0,
             .isAlias      = name != variable.name});
    };
    for(const auto& variable : registry.variables()) {
        addVariable(variable, variable.name);
        for(const auto& alias : variable.aliases) {
            addVariable(variable, alias);
        }
    }

    const auto addFunction = [&entries](const ScriptFunctionDescriptor& function, const QString& name) {
        const QString insertion = u"$%1()"_s.arg(name);
        QString signature       = function.signature;
        if(name != function.name && signature.startsWith(u"$%1("_s.arg(function.name))) {
            signature.replace(1, function.name.size(), name);
        }
        entries.push_back(
            {.kind         = ScriptReferenceKind::Function,
             .label        = signature.isEmpty() ? insertion : signature,
             .insertText   = insertion,
             .category     = function.category.isEmpty()
                               ? translate("Plugins")
                               : QCoreApplication::translate("Fooyin", function.category.toUtf8().constData()),
             .description  = QCoreApplication::translate("Fooyin", function.description.toUtf8().constData()),
             .cursorOffset = 1,
             .isAlias      = name != function.name});
    };
    for(const auto& function : registry.functions()) {
        addFunction(function, function.name);
        for(const auto& alias : function.aliases) {
            addFunction(function, alias);
        }
    }

    entries.insert(entries.end(), Formatting.begin(), Formatting.end());
    return entries;
}
} // namespace Fooyin
