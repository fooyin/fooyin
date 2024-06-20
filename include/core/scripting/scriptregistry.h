/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "fycore_export.h"

#include <core/scripting/scriptvalue.h>
#include <core/track.h>

#include <QObject>

namespace Fooyin {
class PlayerController;

class FYCORE_EXPORT ScriptRegistry
{
public:
    using FuncRet = std::variant<int, uint64_t, QString, QStringList>;

    explicit ScriptRegistry(PlayerController* playerController = nullptr);
    virtual ~ScriptRegistry();

    [[nodiscard]] virtual bool isVariable(const QString& var, const Track& track) const;
    [[nodiscard]] virtual bool isVariable(const QString& var, const TrackList& tracks) const;
    [[nodiscard]] virtual bool isFunction(const QString& func) const;

    [[nodiscard]] virtual ScriptResult value(const QString& var, const Track& track) const;
    [[nodiscard]] virtual ScriptResult value(const QString& var, const TrackList& tracks) const;
    [[nodiscard]] virtual ScriptResult function(const QString& func, const ScriptValueList& args,
                                                const Track& track) const;
    [[nodiscard]] virtual ScriptResult function(const QString& func, const ScriptValueList& args,
                                                const TrackList& tracks) const;

    virtual void setValue(const QString& var, const FuncRet& value, Track& track);

protected:
    template <typename NewCntr, typename Cntr>
    NewCntr containerCast(const Cntr& from) const
    {
        return NewCntr(from.cbegin(), from.cend());
    }

    [[nodiscard]] bool isListVariable(const QString& var) const;
    static ScriptResult calculateResult(FuncRet funcRet);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
