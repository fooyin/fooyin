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

#include <gui/guiconstants.h>
#include <gui/scripting/richtext.h>
#include <gui/scripting/scriptformatterregistry.h>
#include <utils/crypto.h>
#include <utils/utils.h>

#include <QIcon>
#include <QRegularExpression>
#include <utility>

using namespace Qt::StringLiterals;

namespace Fooyin {
ExpressionTreeItem::ExpressionTreeItem()
    : ExpressionTreeItem{{}, {}, {}}
{ }

ExpressionTreeItem::ExpressionTreeItem(QString key, QString name, Expression expression, const bool isFormatting)
    : m_key{std::move(key)}
    , m_name{std::move(name)}
    , m_expression{std::move(expression)}
    , m_isFormatting{isFormatting}
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

bool ExpressionTreeItem::isFormatting() const
{
    return m_isFormatting;
}

ExpressionTreeModel::ExpressionTreeModel(QObject* parent)
    : TreeModel{parent}
    , m_iconExpression{Utils::iconFromTheme(Constants::Icons::ScriptExpression)}
    , m_iconLiteral{Utils::iconFromTheme(Constants::Icons::ScriptLiteral)}
    , m_iconVariable{Utils::iconFromTheme(Constants::Icons::ScriptVariable)}
    , m_iconFunction{Utils::iconFromTheme(Constants::Icons::ScriptFunction)}
    , m_iconFormatting{Utils::iconFromTheme(Constants::Icons::ScriptFormatting)}
{ }

void ExpressionTreeModel::populate(const ExpressionList& expressions)
{
    beginResetModel();

    resetRoot();
    m_nodes.clear();

    ExpressionTreeItem* parent = rootItem();

    if(expressions.size() > 1) {
        Expression const fullExpression{Expr::FunctionArg, expressions};
        parent = insertNode(generateKey(parent->key(), u" … "_s), u" … "_s, fullExpression, parent);
    }

    int index{0};
    iterateExpressions(expressions, parent, index);
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

    if(item->isFormatting()) {
        return m_iconFormatting;
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
        case(Expr::Equals):
        case(Expr::Contains):
        default:
            break;
    }

    return {};
}

ExpressionTreeItem* ExpressionTreeModel::insertNode(const QString& key, const QString& name,
                                                    const Expression& expression, ExpressionTreeItem* parent,
                                                    const bool isFormatting)
{
    if(!m_nodes.contains(key)) {
        auto* item = m_nodes.contains(key)
                       ? &m_nodes.at(key)
                       : &m_nodes.emplace(key, ExpressionTreeItem{key, name, expression, isFormatting}).first->second;
        parent->appendChild(item);
    }
    return &m_nodes.at(key);
}

QString ExpressionTreeModel::generateKey(const QString& parentKey, const QString& name) const
{
    return Utils::generateHash(parentKey, name, QString::number(m_nodes.size()));
}

QString ExpressionTreeModel::parseHtmlAttribute(const QString& attrs, const QString& name)
{
    static const QRegularExpression attrRegex{uR"(?:^|\s)%1\s*=\s*(?:"([^"]*)\"|'([^']*)'|([^\s"'>]+))"_s.arg(name)};
    const QRegularExpressionMatch match = attrRegex.match(attrs);

    if(!match.hasMatch()) {
        return {};
    }

    for(int i{1}; i <= 3; ++i) {
        if(!match.captured(i).isEmpty()) {
            return match.captured(i);
        }
    }

    return {};
}

ExpressionTreeModel::ParsedFormatTag ExpressionTreeModel::parseFormatTag(QString content)
{
    content = content.trimmed();

    if(content.isEmpty()) {
        return {};
    }

    ParsedFormatTag tag;

    if(content.startsWith('/'_L1)) {
        tag.closing = true;
        content     = content.mid(1).trimmed();
    }

    if(content.isEmpty()) {
        return {};
    }

    const auto firstSpace  = content.indexOf(' '_L1);
    const auto firstEquals = content.indexOf('='_L1);

    if(firstEquals >= 0 && (firstSpace < 0 || firstEquals < firstSpace)) {
        tag.name   = content.left(firstEquals).trimmed().toLower();
        tag.option = content.mid(firstEquals + 1).trimmed();
        return tag;
    }

    tag.name = (firstSpace < 0 ? content : content.left(firstSpace)).trimmed().toLower();

    if(firstSpace >= 0 && tag.name == "a"_L1 && !tag.closing) {
        tag.option = parseHtmlAttribute(content.mid(firstSpace + 1), u"href"_s);
    }

    return tag;
}

bool ExpressionTreeModel::isKnownFormatTag(const ParsedFormatTag& tag)
{
    if(tag.name.isEmpty()) {
        return false;
    }

    RichFormatting formatting;
    const ScriptFormatterRegistry registry;

    if(tag.closing) {
        const QString option = tag.name == "a"_L1 ? u"https://example.com"_s : QString{};
        return registry.format(formatting, tag.name, option);
    }

    return registry.format(formatting, tag.name, tag.option);
}

std::optional<QString> ExpressionTreeModel::literalExpressionText(const Expression& expression)
{
    if(!std::holds_alternative<QString>(expression.value)) {
        return {};
    }

    const auto& value = std::get<QString>(expression.value);

    switch(expression.type) {
        case Expr::Literal:
            return value;
        case Expr::QuotedLiteral:
            return u"\"%1\""_s.arg(value);
        default:
            break;
    }

    return {};
}

QString ExpressionTreeModel::expressionsText(const ExpressionList& expressions, int startIndex, int endIndex) const
{
    if(endIndex < 0 || std::cmp_greater(endIndex, expressions.size())) {
        endIndex = static_cast<int>(expressions.size());
    }

    QString text;

    for(int i{startIndex}; i < endIndex; ++i) {
        text += expressionText(expressions.at(i));
    }

    return text;
}

QString ExpressionTreeModel::expressionText(const Expression& expression) const
{
    if(const auto literalText = literalExpressionText(expression)) {
        return *literalText;
    }

    if(const auto* value = std::get_if<QString>(&expression.value)) {
        switch(expression.type) {
            case Expr::Variable:
                return u"%1%2%1"_s.arg(u'%', *value);
            case Expr::VariableList:
                return u"%<%1>"_s.arg(*value);
            default:
                return *value;
        }
    }

    if(const auto* funcValue = std::get_if<FuncValue>(&expression.value)) {
        QStringList args;
        for(const auto& arg : funcValue->args) {
            args.emplace_back(expressionText(arg));
        }

        return u"$%1(%2)"_s.arg(funcValue->name, args.join(u","_s));
    }

    if(const auto* listValue = std::get_if<ExpressionList>(&expression.value)) {
        QString listText = expressionsText(*listValue);

        switch(expression.type) {
            case Expr::Conditional:
                return u"[%1]"_s.arg(listText);
            case Expr::FunctionArg:
                return listText;
            case Expr::Group:
                return u"(%1)"_s.arg(listText);
            default:
                return listText;
        }
    }

    return {};
}

std::optional<ExpressionTreeModel::FormatTagMatch>
ExpressionTreeModel::matchFormatTag(const ExpressionList& expressions, const int startIndex) const
{
    if(startIndex < 0 || std::cmp_greater_equal(startIndex, expressions.size())) {
        return {};
    }

    const auto open = literalExpressionText(expressions.at(startIndex));
    if(!open || *open != "<"_L1) {
        return {};
    }

    QString content;
    int endIndex{startIndex + 1};

    while(std::cmp_less(endIndex, expressions.size())) {
        const auto part = literalExpressionText(expressions.at(endIndex));
        if(!part) {
            return {};
        }

        if(*part == ">"_L1) {
            break;
        }

        content += *part;
        ++endIndex;
    }

    if(std::cmp_greater_equal(endIndex, expressions.size())) {
        return {};
    }

    const ParsedFormatTag parsedTag = parseFormatTag(content);
    if(!isKnownFormatTag(parsedTag)) {
        return {};
    }

    return FormatTagMatch{
        .name        = parsedTag.name,
        .displayText = expressionsText(expressions, startIndex, endIndex + 1),
        .nextIndex   = endIndex + 1,
        .closing     = parsedTag.closing,
    };
}

int ExpressionTreeModel::findClosingFormatTag(const ExpressionList& expressions, int index, int endIndex,
                                              const QString& tagName) const
{
    while(index < endIndex) {
        if(const auto tag = matchFormatTag(expressions, index)) {
            if(tag->closing) {
                if(tag->name == tagName) {
                    return tag->nextIndex;
                }
            }
            else {
                if(const int nestedEnd = findClosingFormatTag(expressions, tag->nextIndex, endIndex, tag->name);
                   nestedEnd > tag->nextIndex) {
                    index = nestedEnd;
                    continue;
                }
            }
        }

        ++index;
    }

    return -1;
}

void ExpressionTreeModel::iterateExpressions(const ExpressionList& expressions, ExpressionTreeItem* parent, int& index,
                                             const QString& closingFormatTag, int endIndex)
{
    if(endIndex < 0 || std::cmp_greater(endIndex, expressions.size())) {
        endIndex = static_cast<int>(expressions.size());
    }

    while(index < endIndex) {
        if(const auto tag = matchFormatTag(expressions, index)) {
            if(tag->closing) {
                if(!closingFormatTag.isEmpty() && tag->name == closingFormatTag) {
                    index = tag->nextIndex;
                    return;
                }
            }
            else {
                const int closingIndex = findClosingFormatTag(expressions, tag->nextIndex, endIndex, tag->name);
                const int groupEnd     = closingIndex > tag->nextIndex ? closingIndex : endIndex;

                if(groupEnd >= tag->nextIndex) {
                    const QString fullTagText = expressionsText(expressions, index, groupEnd);
                    const Expression formatExpression{Expr::Literal, fullTagText};
                    auto* node = insertNode(generateKey(parent->key(), tag->displayText), tag->displayText,
                                            formatExpression, parent, true);

                    int childIndex = tag->nextIndex;
                    iterateExpressions(expressions, node, childIndex,
                                       closingIndex > tag->nextIndex ? tag->name : QString{}, groupEnd);

                    index = groupEnd;
                    continue;
                }
            }
        }

        iterateExpression(expressions.at(index), parent);
        ++index;
    }
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
            name   = u"[ … ]"_s;
            parent = insertNode(generateKey(parent->key(), name), name, expression, parent);
        }

        int index{0};
        iterateExpressions(*listVal, parent, index);
    }
}
} // namespace Fooyin
