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

#include "core/scripting/expression.h"
#include "utils/treeitem.h"
#include "utils/treemodel.h"

#include <QIcon>

#include <optional>

namespace Fooyin {
class ExpressionTreeItem : public TreeItem<ExpressionTreeItem>
{
public:
    ExpressionTreeItem();
    explicit ExpressionTreeItem(QString key, QString name, Expression expression, bool isFormatting = false);

    QString key() const;
    Expr::Type type() const;
    QString name() const;
    Expression expression() const;
    bool isFormatting() const;

private:
    QString m_key;
    QString m_name;
    Expression m_expression;
    bool m_isFormatting{false};
};

class ExpressionTreeModel : public TreeModel<ExpressionTreeItem>
{
public:
    explicit ExpressionTreeModel(QObject* parent = nullptr);

    void populate(const ExpressionList& expressions);

    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

private:
    struct ParsedFormatTag
    {
        QString name;
        QString option;
        bool closing{false};
    };

    struct FormatTagMatch
    {
        QString name;
        QString displayText;
        int nextIndex{-1};
        bool closing{false};
    };

    ExpressionTreeItem* insertNode(const QString& key, const QString& name, const Expression& expression,
                                   ExpressionTreeItem* parent, bool isFormatting = false);
    QString generateKey(const QString& parentKey, const QString& name) const;

    [[nodiscard]] static QString parseHtmlAttribute(const QString& attrs, const QString& name);
    [[nodiscard]] static ParsedFormatTag parseFormatTag(QString content);
    [[nodiscard]] static bool isKnownFormatTag(const ParsedFormatTag& tag);

    [[nodiscard]] static std::optional<QString> literalExpressionText(const Expression& expression);
    [[nodiscard]] QString expressionText(const Expression& expression) const;
    [[nodiscard]] QString expressionsText(const ExpressionList& expressions, int startIndex = 0,
                                          int endIndex = -1) const;

    [[nodiscard]] std::optional<FormatTagMatch> matchFormatTag(const ExpressionList& expressions, int startIndex) const;
    int findClosingFormatTag(const ExpressionList& expressions, int index, int endIndex, const QString& tagName) const;
    void iterateExpressions(const ExpressionList& expressions, ExpressionTreeItem* parent, int& index,
                            const QString& closingFormatTag = {}, int endIndex = -1);
    void iterateExpression(const Expression& expression, ExpressionTreeItem* parent);

    std::unordered_map<QString, ExpressionTreeItem> m_nodes;

    QIcon m_iconExpression;
    QIcon m_iconLiteral;
    QIcon m_iconVariable;
    QIcon m_iconFunction;
    QIcon m_iconFormatting;
};
} // namespace Fooyin
