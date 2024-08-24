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

#include "expressiontreemodel.h"

#include "gui/guiconstants.h"
#include "utils/crypto.h"
#include "utils/utils.h"

#include <QIcon>
#include <utility>

namespace Fooyin {
ExpressionTreeItem::ExpressionTreeItem()
    : ExpressionTreeItem{{}, {}, {}}
{ }

ExpressionTreeItem::ExpressionTreeItem(QString key, QString name, Expression expression)
    : m_key{std::move(key)}
    , m_name{std::move(name)}
    , m_expression{std::move(expression)}
{ }

QString ExpressionTreeItem::key() const
{
    return m_key;
}

Expr::Type ExpressionTreeItem::type() const
{
    return m_expression.type;
}

QString ExpressionTreeItem::name() const
{
    return m_name;
}

Expression ExpressionTreeItem::expression() const
{
    return m_expression;
}

ExpressionTreeModel::ExpressionTreeModel(QObject* parent)
    : TreeModel{parent}
    , m_iconExpression{Utils::iconFromTheme(Constants::Icons::ScriptExpression)}
    , m_iconLiteral{Utils::iconFromTheme(Constants::Icons::ScriptLiteral)}
    , m_iconVariable{Utils::iconFromTheme(Constants::Icons::ScriptVariable)}
    , m_iconFunction{Utils::iconFromTheme(Constants::Icons::ScriptFunction)}
{ }

void ExpressionTreeModel::populate(const ExpressionList& expressions)
{
    beginResetModel();

    resetRoot();
    m_nodes.clear();

    ExpressionTreeItem* parent = rootItem();

    if(expressions.size() > 1) {
        Expression const fullExpression{Expr::FunctionArg, expressions};
        parent = insertNode(generateKey(parent->key(), QStringLiteral(" ... ")), QStringLiteral(" ... "),
                            fullExpression, parent);
    }

    for(const auto& expression : expressions) {
        iterateExpression(expression, parent);
    }
    endResetModel();
}

QVariant ExpressionTreeModel::data(const QModelIndex& index, int role) const
{
    if(role != Qt::DisplayRole && role != Qt::DecorationRole) {
        return {};
    }

    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const auto* item = static_cast<ExpressionTreeItem*>(index.internalPointer());

    if(role == Qt::DisplayRole) {
        return item->name();
    }

    switch(item->type()) {
        case(Expr::Literal):
            return m_iconLiteral;
        case(Expr::Variable):
            return m_iconVariable;
        case(Expr::Function):
            return m_iconFunction;
        case(Expr::FunctionArg):
            return m_iconExpression;
        case(Expr::Null):
        case(Expr::Conditional):
        case(Expr::VariableList):
        default:
            break;
    }

    return {};
}

ExpressionTreeItem* ExpressionTreeModel::insertNode(const QString& key, const QString& name,
                                                    const Expression& expression, ExpressionTreeItem* parent)
{
    if(!m_nodes.contains(key)) {
        auto* item = m_nodes.contains(key)
                       ? &m_nodes.at(key)
                       : &m_nodes.emplace(key, ExpressionTreeItem{key, name, expression}).first->second;
        parent->appendChild(item);
    }
    return &m_nodes.at(key);
}

QString ExpressionTreeModel::generateKey(const QString& parentKey, const QString& name) const
{
    return Utils::generateHash(parentKey, name, QString::number(m_nodes.size()));
}

void ExpressionTreeModel::iterateExpression(const Expression& expression, ExpressionTreeItem* parent)
{
    QString name;

    if(const auto* val = std::get_if<QString>(&expression.value)) {
        name = *val;
        insertNode(generateKey(parent->key(), name), name, expression, parent);
    }

    else if(const auto* funcVal = std::get_if<FuncValue>(&expression.value)) {
        name       = funcVal->name;
        auto* node = insertNode(generateKey(parent->key(), name), name, expression, parent);

        for(const auto& argExpr : funcVal->args) {
            iterateExpression(argExpr, node);
        }
    }

    else if(const auto* listVal = std::get_if<ExpressionList>(&expression.value)) {
        if(expression.type == Expr::Conditional) {
            name   = QStringLiteral("[ ... ]");
            parent = insertNode(generateKey(parent->key(), name), name, expression, parent);
        }

        for(const auto& listExpr : *listVal) {
            iterateExpression(listExpr, parent);
        }
    }
}
} // namespace Fooyin
