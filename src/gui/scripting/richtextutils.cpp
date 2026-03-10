/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include <gui/scripting/richtextutils.h>

using namespace Qt::StringLiterals;

namespace Fooyin {
QString richTextToHtml(const RichText& richText, const QColor& linkColour)
{
    QString html;
    html.reserve(richText.joinedText().size() * 2);

    for(const auto& [text, format] : richText.blocks) {
        QStringList styles;

        const bool isLink = !format.link.isEmpty();

        if(!format.font.family().isEmpty()) {
            styles.emplace_back(u"font-family:'%1'"_s.arg(format.font.family().toHtmlEscaped()));
        }
        if(format.font.pointSizeF() > 0) {
            styles.emplace_back(u"font-size:%1pt"_s.arg(format.font.pointSizeF()));
        }
        if(format.font.bold()) {
            styles.emplace_back(u"font-weight:bold"_s);
        }
        if(format.font.italic()) {
            styles.emplace_back(u"font-style:italic"_s);
        }
        if(format.font.underline()) {
            styles.emplace_back(u"text-decoration:underline"_s);
        }
        if(format.font.strikeOut()) {
            styles.emplace_back(u"text-decoration:line-through"_s);
        }
        if(isLink && linkColour.isValid()) {
            styles.emplace_back(u"color:%1"_s.arg(linkColour.name(QColor::HexArgb)));
        }
        else if(format.colour.isValid()) {
            styles.emplace_back(u"color:%1"_s.arg(format.colour.name(QColor::HexArgb)));
        }

        QString htmlText = text.toHtmlEscaped();
        htmlText.replace(u'\n', u"<br/>"_s);

        const QString style = u"white-space:pre-wrap;%1"_s.arg(styles.join(u";"_s));

        if(!isLink) {
            html += u"<span style=\"%1\">%2</span>"_s.arg(style, htmlText);
        }
        else {
            html += u"<a href=\"%1\"><span style=\"%2\">%3</span></a>"_s.arg(format.link.toHtmlEscaped(), style, text);
        }
    }

    return html;
}
} // namespace Fooyin
