/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <QSqlDatabase>

#include <map>

namespace Fy::Core::DB {
class Query;

using BindingsMap = std::map<QString, QString>;

class Module
{
public:
    explicit Module(QString connectionName);
    virtual ~Module() = default;

    [[nodiscard]] QSqlDatabase db() const;
    [[nodiscard]] QString connectionName() const;

    [[nodiscard]] Query runQuery(const QString& query, const QString& errorText) const;
    [[nodiscard]] Query runQuery(const QString& query, const std::pair<QString, QString>& bindings,
                                 const QString& errorText) const;
    [[nodiscard]] Query runQuery(const QString& query, const BindingsMap& bindings, const QString& errorText) const;

    Query update(const QString& tableName, const BindingsMap& fieldBindings,
                 const std::pair<QString, QString>& whereBinding, const QString& errorMessage);
    Query remove(const QString& tableName, const std::vector<std::pair<QString, QString>>& whereBinding,
                 const QString& errorMessage);
    Query insert(const QString& tableName, const BindingsMap& fieldBindings, const QString& errorMessage);

protected:
    Module* module();
    [[nodiscard]] const Module* module() const;

private:
    QString m_connectionName;
};
} // namespace Fy::Core::DB
