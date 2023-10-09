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

#include "expressiontreemodel.h"

namespace Fy::Sandbox {
ExpressionTreeItem::ExpressionTreeItem()
    : ExpressionTreeItem{""}
{ }

ExpressionTreeItem::ExpressionTreeItem(QString expression)
    : m_expression{std::move(expression)}
{ }

QString ExpressionTreeItem::expression() const
{
    return m_expression;
}

ExpressionTreeModel::ExpressionTreeModel(QObject* parent)
    : TreeModel{parent}
{ }

void ExpressionTreeModel::populate(const Core::Scripting::ExpressionList& expressions)
{
    for(const auto& expression : expressions) {
        Q_UNUSED(expression)
    }
}

QVariant ExpressionTreeModel::data(const QModelIndex& index, int role) const
{
    if(role != Qt::DisplayRole && role != Qt::DecorationRole) {
        return {};
    }

    if(checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<ExpressionTreeItem*>(index.internalPointer());

    if(role == Qt::DisplayRole) {
        return item->expression();
    }

    return {};
}
} // namespace Fy::Sandbox
