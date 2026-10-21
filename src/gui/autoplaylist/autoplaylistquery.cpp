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

#include "autoplaylistquery.h"

#include <core/scripting/scriptparser.h>
#include <core/scripting/scriptregistry.h>
#include <core/scripting/scriptscanner.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <ranges>
#include <type_traits>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
QString quoted(QString value)
{
    value.replace(uR"(\)"_s, uR"(\\)"_s);
    value.replace(uR"(")"_s, uR"(\")"_s);
    return uR"("%1")"_s.arg(value);
}

bool validNumericValue(const QString& value, AutoPlaylistFieldType type, bool canonical)
{
    bool valid{false};
    if(type == AutoPlaylistFieldType::Rating) {
        const double rating = value.toDouble(&valid);
        return valid && std::isfinite(rating) && rating >= 0 && rating <= 5 && std::floor(rating * 2) == rating * 2
            && (!canonical || value == QString::number(rating));
    }

    const auto number = value.toLongLong(&valid);
    return valid && number >= 0 && number <= std::numeric_limits<int>::max()
        && (!canonical || value == QString::number(number));
}

int varFieldIndex(const Expression& expression)
{
    if(expression.type != Expr::Variable || !std::holds_alternative<QString>(expression.value)) {
        return -1;
    }
    return AutoPlaylistQueryBuilder::fieldIndex(std::get<QString>(expression.value));
}

std::optional<QString> customField(const Expression& expression, Expr::Type comparison)
{
    const bool isPresence = comparison == Expr::Present || comparison == Expr::Missing;

    if(expression.type == Expr::Variable || (isPresence && expression.type == Expr::QuotedLiteral)) {
        const auto& name = std::get<QString>(expression.value);
        if(expression.type == Expr::Variable && ScriptRegistry::instance().isVariable(name)) {
            return {};
        }
        return name;
    }

    const auto* function = std::get_if<FuncValue>(&expression.value);
    if(!isPresence && expression.type == Expr::Function && function && function->name == "meta"_L1
       && function->args.size() == 1 && function->args.front().type == Expr::FunctionArg) {
        const auto* args = std::get_if<ExpressionList>(&function->args.front().value);
        if(args && args->size() == 1
           && (args->front().type == Expr::QuotedLiteral || args->front().type == Expr::Literal)) {
            return std::get<QString>(args->front().value);
        }
    }
    return {};
}

bool readAbsoluteDate(const QString& operand, QDate& date)
{
    QString dateText = operand.trimmed();
    if(dateText.startsWith(u'"') && dateText.endsWith(u'"')) {
        dateText = dateText.mid(1, dateText.size() - 2);
    }
    date = QDate::fromString(dateText, Qt::ISODate);

    return date.isValid() && dateText == date.toString(Qt::ISODate);
}

bool readDate(const QString& operand, AutoPlaylistRule& rule)
{
    ScriptScanner scanner;
    scanner.setWhitespaceMode(ScriptScanner::WhitespaceMode::IgnoreAll);
    scanner.setup(operand);

    auto token = scanner.next();
    if(token.type == ScriptScanner::TokLast) {
        rule.relativeDate = true;
        token             = scanner.next();
        if(token.type == ScriptScanner::TokLiteral) {
            bool valid{false};
            rule.period = token.value.toInt(&valid);
            if(!valid || rule.period <= 0 || rule.period == std::numeric_limits<int>::max()) {
                return false;
            }
            token = scanner.next();
        }

        const auto units = AutoPlaylistQueryBuilder::dateUnits();
        for(size_t i{0}; i < units.size(); ++i) {
            const auto keyword = units[i].keyword;
            if(token.value.compare(keyword, Qt::CaseInsensitive) == 0
               || token.value.compare(keyword.chopped(1), Qt::CaseInsensitive) == 0) {
                rule.unit = static_cast<int>(i);
                return scanner.next().type == ScriptScanner::TokEos;
            }
        }
        return false;
    }

    return readAbsoluteDate(operand, rule.date);
}

bool readRule(const Expression& expression, AutoPlaylistRule& rule)
{
    if(expression.type == Expr::Group) {
        const auto* args = std::get_if<ExpressionList>(&expression.value);
        return args && args->size() == 1 && readRule(args->front(), rule);
    }
    if(expression.type == Expr::Not) {
        const auto* args = std::get_if<ExpressionList>(&expression.value);
        if(!args || args->size() != 1 || !readRule(args->front(), rule)) {
            return false;
        }
        rule.negated = !rule.negated;
        return true;
    }

    const auto* date = std::get_if<DateValue>(&expression.value);
    const auto* args = date ? &date->args : std::get_if<ExpressionList>(&expression.value);
    if(!args || args->empty()) {
        return false;
    }

    const int field = varFieldIndex(args->front());
    if(field >= 0) {
        rule.field = AutoPlaylistQueryBuilder::fields()[field].name;
        if((expression.type == Expr::Present || expression.type == Expr::Missing)
           && std::get<QString>(args->front().value).compare(rule.field, Qt::CaseInsensitive) != 0) {
            return false;
        }
    }
    else if(const auto name = customField(args->front(), expression.type);
            name && !name->isEmpty() && *name == name->trimmed()) {
        rule.field  = *name;
        rule.source = AutoPlaylistFieldSource::Metadata;
    }
    else {
        return false;
    }

    const auto type        = AutoPlaylistQueryBuilder::fieldType(rule.field, rule.source);
    const auto comparisons = AutoPlaylistQueryBuilder::comparisons(type);
    if(std::ranges::find(comparisons, expression.type) == comparisons.end()) {
        return false;
    }

    rule.comparison = expression.type;
    if(rule.comparison == Expr::Present || rule.comparison == Expr::Missing) {
        return args->size() == 1;
    }
    if(type == AutoPlaylistFieldType::Date) {
        if(rule.comparison == Expr::Between) {
            return date && args->size() == 3 && readAbsoluteDate(date->operand, rule.date)
                && readAbsoluteDate(date->endOperand, rule.endDate) && rule.date <= rule.endDate;
        }
        return date && args->size() == (rule.comparison == Expr::During ? 3 : 2) && readDate(date->operand, rule)
            && (!rule.relativeDate || rule.comparison == Expr::During);
    }
    if(rule.comparison == Expr::In || rule.comparison == Expr::ContainsAny) {
        for(const auto& value : *args | std::views::drop(1)) {
            if(value.type != Expr::Literal && value.type != Expr::QuotedLiteral) {
                return false;
            }
            const auto& text = std::get<QString>(value.value);
            if(!AutoPlaylistQueryBuilder::isListValueValid(text, type)) {
                return false;
            }
            rule.values.push_back(text);
        }
        return !rule.values.empty();
    }
    if(args->size() != 2 || (args->at(1).type != Expr::Literal && args->at(1).type != Expr::QuotedLiteral)) {
        return false;
    }

    rule.value = std::get<QString>(args->at(1).value);
    if(type == AutoPlaylistFieldType::Text || type == AutoPlaylistFieldType::Metadata) {
        // The rule editor validates unfinished rules, so accept empty text
        return true;
    }
    if(type == AutoPlaylistFieldType::Boolean) {
        return rule.value == "1"_L1;
    }

    return validNumericValue(rule.value, type, rule.comparison == Expr::Equals);
}

std::optional<AutoPlaylistRuleGroup::Match> groupMatch(Expr::Type type)
{
    switch(type) {
        case Expr::And:
            return AutoPlaylistRuleGroup::All;
        case Expr::Or:
            return AutoPlaylistRuleGroup::Any;
        case Expr::XOr:
            return AutoPlaylistRuleGroup::Odd;
        default:
            return {};
    }
}

bool collectRules(const Expression& expression, AutoPlaylistRuleGroup& group)
{
    const auto* args = std::get_if<ExpressionList>(&expression.value);
    if(expression.type == Expr::Group) {
        if(!args || args->size() != 1) {
            return false;
        }

        const auto* child = &args->front();
        while(child->type == Expr::Group) {
            const auto* children = std::get_if<ExpressionList>(&child->value);
            if(!children || children->size() != 1) {
                return false;
            }
            child = &children->front();
        }

        if(const auto match = groupMatch(child->type)) {
            AutoPlaylistRuleGroup nested;
            nested.match = *match;
            if(!collectRules(*child, nested)) {
                return false;
            }
            group.rules.emplace_back(std::move(nested));
            return true;
        }
        return collectRules(*child, group);
    }

    if(const auto match = groupMatch(expression.type)) {
        if(!args || args->size() != 2) {
            return false;
        }
        if(*match != group.match) {
            AutoPlaylistRuleGroup nested;
            nested.match = *match;
            if(!collectRules(expression, nested)) {
                return false;
            }
            group.rules.emplace_back(std::move(nested));
            return true;
        }
        return collectRules(args->front(), group) && collectRules(args->back(), group);
    }

    AutoPlaylistRule rule;
    if(!readRule(expression, rule)) {
        return false;
    }

    if(rule.negated && (rule.comparison == Expr::Missing || rule.comparison == Expr::Present)) {
        rule.comparison = rule.comparison == Expr::Missing ? Expr::Present : Expr::Missing;
        rule.negated    = false;
    }
    if(rule.negated && rule.comparison != Expr::Contains && rule.comparison != Expr::StartsWith
       && rule.comparison != Expr::EndsWith && rule.comparison != Expr::Equals && rule.comparison != Expr::In
       && rule.comparison != Expr::ContainsAny && rule.comparison != Expr::ContainsWord
       && AutoPlaylistQueryBuilder::fieldType(rule.field, rule.source) != AutoPlaylistFieldType::Date) {
        return false;
    }

    group.rules.emplace_back(std::move(rule));
    return true;
}

QString ruleQuery(const AutoPlaylistRule& rule)
{
    QString comparison;

    switch(rule.comparison) {
        case Expr::Contains:
            comparison = u"HAS"_s;
            break;
        case Expr::ContainsAny:
            comparison = u"HAS ANY"_s;
            break;
        case Expr::ContainsWord:
            comparison = u"HAS WORD"_s;
            break;
        case Expr::StartsWith:
            comparison = u"STARTS WITH"_s;
            break;
        case Expr::EndsWith:
            comparison = u"ENDS WITH"_s;
            break;
        case Expr::Equals:
            comparison = u"IS"_s;
            break;
        case Expr::In:
            comparison = u"IN"_s;
            break;
        case Expr::Greater:
            comparison = u">"_s;
            break;
        case Expr::GreaterEqual:
            comparison = u">="_s;
            break;
        case Expr::Less:
            comparison = u"<"_s;
            break;
        case Expr::LessEqual:
            comparison = u"<="_s;
            break;
        case Expr::Present:
            comparison = u"PRESENT"_s;
            break;
        case Expr::Missing:
            comparison = u"MISSING"_s;
            break;
        case Expr::Before:
            comparison = u"BEFORE"_s;
            break;
        case Expr::After:
            comparison = u"AFTER"_s;
            break;
        case Expr::Since:
            comparison = u"SINCE"_s;
            break;
        case Expr::During:
            comparison = u"DURING"_s;
            break;
        case Expr::Between:
            comparison = u"BETWEEN"_s;
            break;
        default:
            break;
    }

    const bool presence = rule.comparison == Expr::Present || rule.comparison == Expr::Missing;
    QString field{rule.field};
    if(AutoPlaylistQueryBuilder::fieldIndex(field, rule.source) < 0) {
        field = presence ? quoted(field) : u"$meta(%1)"_s.arg(quoted(field));
    }

    QString term = field + u' ';
    if(rule.negated) {
        term += u"NOT "_s;
    }
    term += comparison;

    if(rule.comparison != Expr::Present && rule.comparison != Expr::Missing) {
        if(rule.comparison == Expr::In || rule.comparison == Expr::ContainsAny) {
            QStringList values;
            for(const auto& value : rule.values) {
                values.push_back(quoted(value));
            }
            term += u" (%1)"_s.arg(values.join(u", "_s));
        }
        else if(AutoPlaylistQueryBuilder::fieldType(rule.field, rule.source) == AutoPlaylistFieldType::Date) {
            term += rule.relativeDate && rule.comparison != Expr::Between
                      ? u" LAST %1 %2"_s.arg(rule.period).arg(AutoPlaylistQueryBuilder::dateUnits()[rule.unit].keyword)
                      : u' ' + quoted(rule.date.toString(Qt::ISODate));
            if(rule.comparison == Expr::Between) {
                term += u" AND "_s + quoted(rule.endDate.toString(Qt::ISODate));
            }
        }
        else {
            term += u' ' + quoted(rule.value);
        }
    }

    return term;
}

QString groupQuery(const AutoPlaylistRuleGroup& group)
{
    QStringList terms;

    for(const auto& item : group.rules) {
        const QString term = std::visit(
            [](const auto& value) -> QString {
                if constexpr(std::is_same_v<std::decay_t<decltype(value)>, AutoPlaylistRule>) {
                    return ruleQuery(value);
                }
                else {
                    return groupQuery(value);
                }
            },
            item);
        terms.push_back(u"(%1)"_s.arg(term));
    }

    switch(group.match) {
        case AutoPlaylistRuleGroup::All:
            return terms.join(u" AND "_s);
        case AutoPlaylistRuleGroup::Any:
            return terms.join(u" OR "_s);
        case AutoPlaylistRuleGroup::Odd:
            return terms.join(u" XOR "_s);
    }

    return {};
}

bool validExpression(const Expression& expression)
{
    const auto* date = std::get_if<DateValue>(&expression.value);
    const auto* args = date ? &date->args : std::get_if<ExpressionList>(&expression.value);

    switch(expression.type) {
        case Expr::Not:
        case Expr::Missing:
        case Expr::Present:
            if(!args || args->size() != 1) {
                return false;
            }
            break;
        case Expr::And:
        case Expr::Or:
        case Expr::XOr:
        case Expr::Equals:
        case Expr::Contains:
        case Expr::ContainsWord:
        case Expr::StartsWith:
        case Expr::EndsWith:
        case Expr::Greater:
        case Expr::GreaterEqual:
        case Expr::Less:
        case Expr::LessEqual:
        case Expr::Before:
        case Expr::After:
        case Expr::Since:
            if(!args || args->size() != 2) {
                return false;
            }
            break;
        case Expr::In:
        case Expr::ContainsAny:
            if(!args || args->size() < 2) {
                return false;
            }
            break;
        case Expr::During:
        case Expr::Between:
            if(!args || args->size() != 3) {
                return false;
            }
            break;
        case Expr::Group:
            if(!args || args->empty()) {
                return false;
            }
            break;
        case Expr::SortAscending:
        case Expr::SortDescending: {
            ScriptParser parser;
            return parser.parse(std::get<QString>(expression.value)).isValid();
        }
        case Expr::Limit: {
            bool valid{false};
            const int limit = std::get<QString>(expression.value).toInt(&valid);
            return valid && limit >= 0;
        }
        default:
            break;
    }

    return !args || std::ranges::all_of(*args, validExpression);
}
} // namespace

std::span<const AutoPlaylistField> AutoPlaylistQueryBuilder::fields()
{
    static const std::array fields{
        AutoPlaylistField{.name = u"artist"_s, .label = tr("Artist"), .type = AutoPlaylistFieldType::Text},
        AutoPlaylistField{.name = u"albumartist"_s, .label = tr("Album artist"), .type = AutoPlaylistFieldType::Text},
        AutoPlaylistField{.name = u"album"_s, .label = tr("Album"), .type = AutoPlaylistFieldType::Text},
        AutoPlaylistField{.name = u"title"_s, .label = tr("Title"), .type = AutoPlaylistFieldType::Text},
        AutoPlaylistField{.name = u"genre"_s, .label = tr("Genre"), .type = AutoPlaylistFieldType::Text},
        AutoPlaylistField{.name = u"composer"_s, .label = tr("Composer"), .type = AutoPlaylistFieldType::Text},
        AutoPlaylistField{.name = u"performer"_s, .label = tr("Performer"), .type = AutoPlaylistFieldType::Text},
        AutoPlaylistField{.name = u"comment"_s, .label = tr("Comment"), .type = AutoPlaylistFieldType::Text},
        AutoPlaylistField{.name = u"lyrics"_s, .label = tr("Lyrics"), .type = AutoPlaylistFieldType::Text},
        AutoPlaylistField{.name = u"date"_s, .label = tr("Date"), .type = AutoPlaylistFieldType::Date},
        AutoPlaylistField{.name = u"year"_s, .label = tr("Year"), .type = AutoPlaylistFieldType::Number},
        AutoPlaylistField{.name = u"duration_s"_s, .label = tr("Duration"), .type = AutoPlaylistFieldType::Duration},
        AutoPlaylistField{.name = u"rating"_s, .label = tr("Rating"), .type = AutoPlaylistFieldType::Rating},
        AutoPlaylistField{.name = u"loved"_s, .label = tr("Loved"), .type = AutoPlaylistFieldType::Boolean},
        AutoPlaylistField{.name = u"playcount"_s, .label = tr("Playcount"), .type = AutoPlaylistFieldType::Number},
        AutoPlaylistField{.name = u"addedtime"_s, .label = tr("Date added"), .type = AutoPlaylistFieldType::Date},
        AutoPlaylistField{.name = u"firstplayed"_s, .label = tr("First played"), .type = AutoPlaylistFieldType::Date},
        AutoPlaylistField{.name = u"lastplayed"_s, .label = tr("Last played"), .type = AutoPlaylistFieldType::Date},
        AutoPlaylistField{.name = u"codec"_s, .label = tr("Codec"), .type = AutoPlaylistFieldType::Text},
        AutoPlaylistField{.name = u"path"_s, .label = tr("Folder path"), .type = AutoPlaylistFieldType::Text},
        AutoPlaylistField{.name = u"filepath"_s, .label = tr("File path"), .type = AutoPlaylistFieldType::Text},
    };
    return fields;
}

std::span<const AutoPlaylistDateUnit> AutoPlaylistQueryBuilder::dateUnits()
{
    static const std::array units{
        AutoPlaylistDateUnit{
            .keyword = u"SECONDS"_s,
            //: Time unit in ‘in the last [number] [unit]’. The number is displayed separately; translate only the unit.
            .singular = QT_TRANSLATE_N_NOOP("Fooyin::AutoPlaylistRulesWidget", "second"),
            //: Time unit in ‘in the last [number] [unit]’. The number is displayed separately; translate only the unit.
            .plural = QT_TRANSLATE_N_NOOP("Fooyin::AutoPlaylistRulesWidget", "seconds")},
        AutoPlaylistDateUnit{
            .keyword = u"MINUTES"_s,
            //: Time unit in ‘in the last [number] [unit]’. The number is displayed separately; translate only the unit.
            .singular = QT_TRANSLATE_N_NOOP("Fooyin::AutoPlaylistRulesWidget", "minute"),
            //: Time unit in ‘in the last [number] [unit]’. The number is displayed separately; translate only the unit.
            .plural = QT_TRANSLATE_N_NOOP("Fooyin::AutoPlaylistRulesWidget", "minutes")},
        AutoPlaylistDateUnit{
            .keyword = u"HOURS"_s,
            //: Time unit in ‘in the last [number] [unit]’. The number is displayed separately; translate only the unit.
            .singular = QT_TRANSLATE_N_NOOP("Fooyin::AutoPlaylistRulesWidget", "hour"),
            //: Time unit in ‘in the last [number] [unit]’. The number is displayed separately; translate only the unit.
            .plural = QT_TRANSLATE_N_NOOP("Fooyin::AutoPlaylistRulesWidget", "hours")},
        AutoPlaylistDateUnit{
            .keyword = u"DAYS"_s,
            //: Time unit in ‘in the last [number] [unit]’. The number is displayed separately; translate only the unit.
            .singular = QT_TRANSLATE_N_NOOP("Fooyin::AutoPlaylistRulesWidget", "day"),
            //: Time unit in ‘in the last [number] [unit]’. The number is displayed separately; translate only the unit.
            .plural = QT_TRANSLATE_N_NOOP("Fooyin::AutoPlaylistRulesWidget", "days")},
        AutoPlaylistDateUnit{
            .keyword = u"WEEKS"_s,
            //: Time unit in ‘in the last [number] [unit]’. The number is displayed separately; translate only the unit.
            .singular = QT_TRANSLATE_N_NOOP("Fooyin::AutoPlaylistRulesWidget", "week"),
            //: Time unit in ‘in the last [number] [unit]’. The number is displayed separately; translate only the unit.
            .plural = QT_TRANSLATE_N_NOOP("Fooyin::AutoPlaylistRulesWidget", "weeks")},
        AutoPlaylistDateUnit{
            .keyword = u"MONTHS"_s,
            //: Time unit in ‘in the last [number] [unit]’. The number is displayed separately; translate only the unit.
            .singular = QT_TRANSLATE_N_NOOP("Fooyin::AutoPlaylistRulesWidget", "month"),
            //: Time unit in ‘in the last [number] [unit]’. The number is displayed separately; translate only the unit.
            .plural = QT_TRANSLATE_N_NOOP("Fooyin::AutoPlaylistRulesWidget", "months")},
        AutoPlaylistDateUnit{
            .keyword = u"YEARS"_s,
            //: Time unit in ‘in the last [number] [unit]’. The number is displayed separately; translate only the unit.
            .singular = QT_TRANSLATE_N_NOOP("Fooyin::AutoPlaylistRulesWidget", "year"),
            //: Time unit in ‘in the last [number] [unit]’. The number is displayed separately; translate only the unit.
            .plural = QT_TRANSLATE_N_NOOP("Fooyin::AutoPlaylistRulesWidget", "years")},
    };
    return units;
}

int AutoPlaylistQueryBuilder::fieldIndex(const QString& name, AutoPlaylistFieldSource source)
{
    if(source == AutoPlaylistFieldSource::Metadata) {
        return -1;
    }

    const auto kind   = ScriptRegistry::instance().resolveVariable(name);
    const auto fields = AutoPlaylistQueryBuilder::fields();

    for(size_t i{0}; i < fields.size(); ++i) {
        if(name.compare(fields[i].name, Qt::CaseInsensitive) == 0
           || (kind != VariableKind::Generic && kind == ScriptRegistry::instance().resolveVariable(fields[i].name))) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

AutoPlaylistFieldType AutoPlaylistQueryBuilder::fieldType(const QString& name, AutoPlaylistFieldSource source)
{
    const int index = fieldIndex(name, source);
    return index >= 0 ? fields()[index].type : AutoPlaylistFieldType::Metadata;
}

std::vector<Expr::Type> AutoPlaylistQueryBuilder::comparisons(AutoPlaylistFieldType type)
{
    if(type == AutoPlaylistFieldType::Metadata) {
        return {Expr::Equals,     Expr::In,       Expr::Contains, Expr::ContainsAny,  Expr::ContainsWord,
                Expr::StartsWith, Expr::EndsWith, Expr::Greater,  Expr::GreaterEqual, Expr::Less,
                Expr::LessEqual,  Expr::Present,  Expr::Missing};
    }
    if(type == AutoPlaylistFieldType::Boolean) {
        return {Expr::Equals};
    }
    if(type == AutoPlaylistFieldType::Text) {
        return {Expr::Equals,     Expr::In,       Expr::Contains, Expr::ContainsAny, Expr::ContainsWord,
                Expr::StartsWith, Expr::EndsWith, Expr::Present,  Expr::Missing};
    }
    if(type == AutoPlaylistFieldType::Date) {
        return {Expr::Before, Expr::After, Expr::Since, Expr::During, Expr::Between, Expr::Present, Expr::Missing};
    }
    return {Expr::Equals, Expr::In, Expr::Greater, Expr::GreaterEqual, Expr::Less, Expr::LessEqual};
}

bool AutoPlaylistQueryBuilder::isListValueValid(const QString& value, AutoPlaylistFieldType type)
{
    if(value.isEmpty() || value.contains(u'\n') || value.contains(u'\r') || value.contains(QChar::LineSeparator)
       || value.contains(QChar::ParagraphSeparator)) {
        return false;
    }
    if(type == AutoPlaylistFieldType::Text || type == AutoPlaylistFieldType::Metadata) {
        return true;
    }

    if(type == AutoPlaylistFieldType::Rating || type == AutoPlaylistFieldType::Number
       || type == AutoPlaylistFieldType::Duration) {
        return validNumericValue(value, type, false);
    }
    return false;
}

std::pair<QString, QString> AutoPlaylistQueryBuilder::query(const AutoPlaylistRules& rules)
{
    QString query = rules.allTracks ? u"ALL"_s : groupQuery(rules.group);
    if(rules.limit > 0) {
        query += u" LIMIT %1"_s.arg(rules.limit);
    }

    QString sortQuery;
    if(rules.randomSort) {
        sortQuery = u"SORT%1 $rand()"_s.arg(QChar{rules.descending ? u'-' : u'+'});
    }
    else if(!rules.sortField.isEmpty()) {
        const int index     = fieldIndex(rules.sortField, rules.sortSource);
        const QString field = index >= 0 ? fields()[index].name : u"$meta(%1)"_s.arg(quoted(rules.sortField));
        sortQuery           = u"SORT%1 %2"_s.arg(QChar{rules.descending ? u'-' : u'+'}).arg(field);
    }

    return {query, sortQuery};
}

std::optional<AutoPlaylistRules> AutoPlaylistQueryBuilder::rules(const QString& query, const QString& sortQuery)
{
    ScriptParser parser;
    const auto parsed = parser.parseQuery(sortQuery.isEmpty() ? query : query + u' ' + sortQuery);
    if(!parsed.isValid()) {
        return {};
    }

    AutoPlaylistRules rules;

    std::vector<const Expression*> conditions;
    for(bool hasLimit{false}; const auto& expression : parsed.expressions) {
        if(expression.type == Expr::SortAscending || expression.type == Expr::SortDescending) {
            if(rules.randomSort || !rules.sortField.isEmpty()) {
                return {};
            }

            const auto sort = parser.parse(std::get<QString>(expression.value));
            if(!sort.isValid() || sort.expressions.size() != 1) {
                return {};
            }

            auto field = sort.expressions.front();
            if(field.type == Expr::Literal) {
                field.type = Expr::Variable;
            }

            const int index      = varFieldIndex(field);
            const auto* function = std::get_if<FuncValue>(&field.value);
            if(field.type == Expr::Function && function && function->name == "rand"_L1 && function->args.empty()) {
                rules.randomSort = true;
            }
            else if(index >= 0) {
                rules.sortField = fields()[index].name;
            }
            else if(const auto name = customField(field, Expr::Equals);
                    name && !name->isEmpty() && *name == name->trimmed()) {
                rules.sortField  = *name;
                rules.sortSource = AutoPlaylistFieldSource::Metadata;
            }
            else {
                return {};
            }

            rules.descending = expression.type == Expr::SortDescending;
        }
        else if(expression.type == Expr::Limit) {
            bool valid{false};
            rules.limit = std::get<QString>(expression.value).toInt(&valid);
            if(hasLimit || !valid || rules.limit <= 0) {
                return {};
            }
            hasLimit = true;
        }
        else {
            conditions.push_back(&expression);
        }
    }

    if(conditions.empty()) {
        return {};
    }

    const auto* root = conditions.front();
    while(root->type == Expr::Group) {
        const auto* args = std::get_if<ExpressionList>(&root->value);
        if(!args || args->size() != 1) {
            return {};
        }
        root = &args->front();
    }

    if(conditions.size() == 1 && root->type == Expr::All) {
        rules.allTracks = true;
        return rules;
    }

    if(conditions.size() == 1) {
        rules.group.match = groupMatch(root->type).value_or(AutoPlaylistRuleGroup::All);
        return collectRules(*root, rules.group) ? std::make_optional(std::move(rules)) : std::nullopt;
    }

    for(const auto* condition : conditions) {
        if(!collectRules(*condition, rules.group)) {
            return {};
        }
    }

    return rules;
}

QString AutoPlaylistQueryBuilder::formatQuery(const QString& query)
{
    if(!queryError(query, {}).isEmpty()) {
        return query;
    }

    struct LayoutToken
    {
        ScriptScanner::Token token;
        bool multiline{false};
    };

    ScriptScanner scanner;
    scanner.setWhitespaceMode(ScriptScanner::WhitespaceMode::IgnoreAll);
    scanner.setup(query);

    std::vector<LayoutToken> layout;
    std::vector<size_t> groups;
    bool insideQuote{false};
    bool insideVariable{false};
    bool escapeNext{false};
    bool functionPending{false};
    bool insideSort{false};
    bool betweenPending{false};
    int scriptDepth{0};

    for(auto token = scanner.next(); token.type != ScriptScanner::TokEos; token = scanner.next()) {
        if(escapeNext) {
            escapeNext = false;
            continue;
        }
        if(token.type == ScriptScanner::TokEscape && !insideVariable) {
            escapeNext = true;
            continue;
        }
        if(token.type == ScriptScanner::TokQuote && !insideVariable) {
            insideQuote = !insideQuote;
            continue;
        }
        if(insideQuote) {
            continue;
        }
        if(token.type == ScriptScanner::TokVar) {
            insideVariable = !insideVariable;
            continue;
        }
        if(insideVariable) {
            continue;
        }
        if(insideSort && token.type != ScriptScanner::TokLimit) {
            continue;
        }
        if(token.type == ScriptScanner::TokFunc) {
            functionPending = true;
        }
        else if(token.type == ScriptScanner::TokLeftSquare
                || (token.type == ScriptScanner::TokLeftParen && (functionPending || scriptDepth > 0))) {
            ++scriptDepth;
            functionPending = false;
        }
        else if(scriptDepth > 0
                && (token.type == ScriptScanner::TokRightSquare || token.type == ScriptScanner::TokRightParen)) {
            --scriptDepth;
            continue;
        }
        if(functionPending || scriptDepth > 0) {
            continue;
        }

        switch(token.type) {
            case ScriptScanner::TokBetween: {
                int delta{1};
                auto next = scanner.peekNext(delta);
                while(next.type == ScriptScanner::TokComment) {
                    next = scanner.peekNext(++delta);
                }
                betweenPending = next.type == ScriptScanner::TokLiteral || next.type == ScriptScanner::TokQuote;
                break;
            }
            case ScriptScanner::TokLeftParen:
                groups.emplace_back(layout.size());
                layout.emplace_back(token);
                break;
            case ScriptScanner::TokRightParen:
                if(!groups.empty()) {
                    const bool multiline = layout[groups.back()].multiline;
                    groups.pop_back();
                    layout.emplace_back(token, multiline);
                    if(multiline && !groups.empty()) {
                        layout[groups.back()].multiline = true;
                    }
                }
                break;
            case ScriptScanner::TokAnd:
                if(betweenPending) {
                    betweenPending = false;
                    break;
                }
                [[fallthrough]];
            case ScriptScanner::TokOr:
            case ScriptScanner::TokXOr:
                if(!groups.empty()) {
                    layout[groups.back()].multiline = true;
                }
                layout.emplace_back(token);
                break;
            case ScriptScanner::TokSort:
                insideSort = true;
                layout.emplace_back(token);
                break;
            case ScriptScanner::TokLimit:
                insideSort = false;
                layout.emplace_back(token);
                break;
            case ScriptScanner::TokComment:
                layout.emplace_back(token);
                break;
            default:
                break;
        }
    }

    QString formatted;
    int depth{0};
    int indentation{0};
    qsizetype position{0};

    const auto append = [&](QStringView text) {
        if(formatted.isEmpty() || formatted.endsWith(u'\n')) {
            while(!text.isEmpty() && text.front().isSpace()) {
                text = text.sliced(1);
            }
            if(!text.isEmpty()) {
                formatted += QString{static_cast<qsizetype>(indentation * 4), u' '};
            }
        }
        formatted += text;
    };

    const auto lineBreak = [&](int level) {
        while(!formatted.isEmpty() && formatted.back().isSpace()) {
            formatted.chop(1);
        }
        if(!formatted.isEmpty()) {
            formatted += u'\n';
        }
        indentation = level;
    };

    auto previousType{ScriptScanner::TokEos};
    for(const auto& [token, multiline] : layout) {
        const auto text = QStringView{query}.sliced(position, token.position - position);
        append(text);

        if(token.type == ScriptScanner::TokRightParen && multiline) {
            lineBreak(--depth);
        }
        else if(token.type == ScriptScanner::TokLeftParen && multiline && text.trimmed().isEmpty()
                && (previousType == ScriptScanner::TokAnd || previousType == ScriptScanner::TokOr
                    || previousType == ScriptScanner::TokXOr)) {
            lineBreak(depth);
        }
        else if(token.type != ScriptScanner::TokLeftParen && token.type != ScriptScanner::TokRightParen) {
            lineBreak(token.type == ScriptScanner::TokComment ? 0 : depth);
        }
        append(token.value);
        if(token.type == ScriptScanner::TokLeftParen && multiline) {
            lineBreak(++depth);
        }
        else if(token.type == ScriptScanner::TokComment) {
            lineBreak(depth);
        }
        position     = token.position + token.value.size();
        previousType = token.type;
    }
    append(QStringView{query}.sliced(position));

    return queryError(formatted, {}).isEmpty() ? formatted : query;
}

QString AutoPlaylistQueryBuilder::queryError(const QString& query, const QString& sortQuery)
{
    if(query.trimmed().isEmpty()) {
        return tr("Enter a query.");
    }

    ScriptParser parser;
    const auto parsed = parser.parseQuery(sortQuery.isEmpty() ? query : query + u' ' + sortQuery);
    if(!parsed.errors.empty()) {
        return parsed.errors.front().message;
    }

    if(!ScriptParser::canEvaluateAsQuery(parsed) || !std::ranges::all_of(parsed.expressions, validExpression)) {
        return tr("The query contains an incomplete or invalid expression.");
    }

    return {};
}
} // namespace Fooyin
