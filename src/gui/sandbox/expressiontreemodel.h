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

#include <core/scripting/expression.h>
#include <utils/treeitem.h>
#include <utils/treemodel.h>

namespace Fy::Sandbox {
class ExpressionTreeItem : public Utils::TreeItem<ExpressionTreeItem>
{
public:
    ExpressionTreeItem();
    explicit ExpressionTreeItem(QString expression);

    QString expression() const;

private:
    QString m_expression;
};

class ExpressionTreeModel : public Utils::TreeModel<ExpressionTreeItem>
{
public:
    ExpressionTreeModel(QObject* parent = nullptr);

    void populate(const Core::Scripting::ExpressionList& expressions);

    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

private:
    std::unordered_map<QString, ExpressionTreeItem> m_nodes;
};
} // namespace Fy::Sandbox
