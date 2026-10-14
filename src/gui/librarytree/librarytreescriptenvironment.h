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

#pragma once

#include <core/scripting/scriptenvironmenthelpers.h>

#include <QStringView>

#include <cstdint>

namespace Fooyin {
class ScriptVariableProvider;

enum class LibraryTreeNodePolicy : uint8_t
{
    Unresolved,
    Placeholder,
    Value,
};

class LibraryTreeScriptEnvironment : public LibraryScriptEnvironment
{
public:
    void setNodeVariablePolicy(LibraryTreeNodePolicy policy);
    void setNodeCounts(int trackCount, int childCount);

    [[nodiscard]] LibraryTreeNodePolicy nodeVariablePolicy() const;
    [[nodiscard]] int nodeTrackCount() const;
    [[nodiscard]] int nodeChildCount() const;

private:
    LibraryTreeNodePolicy m_nodeVariablePolicy{LibraryTreeNodePolicy::Unresolved};
    int m_nodeTrackCount{0};
    int m_nodeChildCount{0};
};

[[nodiscard]] const ScriptVariableProvider& libraryTreeNodeVariableProvider();

[[nodiscard]] QString defaultLibraryTreeSummaryTitle();

[[nodiscard]] QString resolveLibraryTreeNodeVariables(QString title, int trackCount, int childCount);

[[nodiscard]] bool usesLibraryTreeNodeVariables(QStringView text);
[[nodiscard]] bool usesLibraryTreeChildCount(QStringView text);
} // namespace Fooyin
