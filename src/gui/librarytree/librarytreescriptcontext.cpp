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

#include "librarytreescriptcontext.h"

#include <QCoreApplication>

using namespace Qt::StringLiterals;

namespace Fooyin {
LibraryTreeScriptValues deferredLibraryTreeScriptValues()
{
    return {{{.name = u"TRACKCOUNT"_s, .result = {.value = u"\uE000LIBTREE_NODE_TRACKCOUNT\uE001"_s, .cond = true}},
             {.name = u"CHILDCOUNT"_s, .result = {.value = u"\uE000LIBTREE_NODE_CHILDCOUNT\uE001"_s, .cond = true}}}};
}

LibraryTreeScriptValues libraryTreeScriptValues(int trackCount, int childCount)
{
    return {{{.name = u"TRACKCOUNT"_s, .result = {.value = QString::number(trackCount), .cond = true}},
             {.name = u"CHILDCOUNT"_s, .result = {.value = QString::number(childCount), .cond = true}}}};
}

QString defaultLibraryTreeSummaryTitle()
{
    //: Default Library Tree summary node title. Keep %trackcount% unchanged; it is a script variable.
    return QCoreApplication::translate("Fooyin::LibraryTree", "All Music (%trackcount%)");
}

QString resolveLibraryTreeNodeVariables(QString title, int trackCount, int childCount)
{
    title.replace(u"\uE000LIBTREE_NODE_TRACKCOUNT\uE001"_s, QString::number(trackCount));
    title.replace(u"\uE000LIBTREE_NODE_CHILDCOUNT\uE001"_s, QString::number(childCount));
    return title;
}

bool usesLibraryTreeNodeVariables(QStringView text)
{
    return text.contains("%trackcount%"_L1, Qt::CaseInsensitive)
        || text.contains("%childcount%"_L1, Qt::CaseInsensitive)
        || text.contains(u"\uE000LIBTREE_NODE_TRACKCOUNT\uE001"_s)
        || text.contains(u"\uE000LIBTREE_NODE_CHILDCOUNT\uE001"_s);
}

bool usesLibraryTreeChildCount(QStringView text)
{
    return text.contains("%childcount%"_L1, Qt::CaseInsensitive)
        || text.contains(u"\uE000LIBTREE_NODE_CHILDCOUNT\uE001"_s);
}
} // namespace Fooyin
