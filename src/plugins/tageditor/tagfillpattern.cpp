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

#include "tagfillpattern.h"

#include <core/constants.h>
#include <core/scripting/scriptbinder.h>
#include <core/track.h>

#include <ranges>
#include <set>

using namespace Qt::StringLiterals;

namespace Fooyin::TagEditor {
namespace {
QString normaliseField(const QString& field)
{
    return field.trimmed().toUpper();
}

bool isWritableBuiltInField(const VariableKind kind)
{
    switch(kind) {
        case VariableKind::Title:
        case VariableKind::Artist:
        case VariableKind::Album:
        case VariableKind::AlbumArtist:
        case VariableKind::Track:
        case VariableKind::TrackTotal:
        case VariableKind::Disc:
        case VariableKind::DiscTotal:
        case VariableKind::Genre:
        case VariableKind::Genres:
        case VariableKind::Composer:
        case VariableKind::Performer:
        case VariableKind::Comment:
        case VariableKind::Date:
        case VariableKind::Rating:
        case VariableKind::RatingNormalized:
        case VariableKind::Stars:
            return true;
        default:
            return false;
    }
}

bool hasAdjacentPatternFields(const std::vector<FillPattern::Token>& tokens)
{
    FillPattern::Token::Type previousType = FillPattern::Token::Type::Literal;

    for(const auto& token : tokens) {
        if(token.type == FillPattern::Token::Type::Literal) {
            previousType = token.type;
            continue;
        }

        if(previousType != FillPattern::Token::Type::Literal) {
            return true;
        }

        previousType = token.type;
    }

    return false;
}
} // namespace

FillPattern FillPattern::parse(const QString& pattern)
{
    FillPattern parsed;
    parsed.m_pattern = pattern;

    QString literal;
    int literalStart{-1};

    const auto flushLiteral = [&parsed, &literal, &literalStart]() {
        if(literal.isEmpty()) {
            return;
        }

        parsed.m_tokens.push_back({.type = Token::Type::Literal, .position = literalStart, .value = literal});
        literal.clear();
        literalStart = -1;
    };

    std::set<QString> fields;

    for(int i{0}; i < pattern.size(); ++i) {
        if(pattern.at(i) != '%'_L1) {
            if(literalStart < 0) {
                literalStart = i;
            }
            literal.append(pattern.at(i));
            continue;
        }

        const auto closing = static_cast<int>(pattern.indexOf('%'_L1, i + 1));
        if(closing < 0) {
            parsed.m_errors.push_back({.position = i, .message = u"Unclosed field marker"_s});
            break;
        }

        flushLiteral();

        const QString field = normaliseField(pattern.mid(i + 1, closing - i - 1));
        if(field.isEmpty()) {
            parsed.m_tokens.push_back({.type = Token::Type::Ignore, .position = i, .value = {}});
            i = closing;
            continue;
        }

        if(!isWritableFillField(field)) {
            parsed.m_errors.push_back({.position = i, .message = u"Field is not writable: %1"_s.arg(field)});
        }
        else if(!fields.emplace(field).second) {
            parsed.m_errors.push_back({.position = i, .message = u"Field is used more than once: %1"_s.arg(field)});
        }

        parsed.m_tokens.push_back({.type = Token::Type::Capture, .position = i, .value = field});
        i = closing;
    }

    flushLiteral();

    if(parsed.m_tokens.empty()) {
        parsed.m_errors.push_back({.position = 0, .message = u"Pattern cannot be empty"_s});
    }

    if(hasAdjacentPatternFields(parsed.m_tokens)) {
        parsed.m_errors.push_back({.position = 0, .message = u"Adjacent fields must be separated by literal text"_s});
    }

    if(!parsed.m_errors.empty()) {
        return parsed;
    }

    for(const auto& token : parsed.m_tokens) {
        if(token.type == Token::Type::Capture) {
            parsed.m_captureFields.push_back(token.value);
        }
    }

    return parsed;
}

bool FillPattern::isValid() const
{
    return m_errors.empty();
}

const std::vector<FillPatternError>& FillPattern::errors() const
{
    return m_errors;
}

const std::vector<QString>& FillPattern::captureFields() const
{
    return m_captureFields;
}

std::optional<FillPattern::Match> FillPattern::match(const QString& source) const
{
    if(!isValid()) {
        return {};
    }

    Match matchResult;
    matchResult.values.reserve(m_captureFields.size());

    qsizetype sourcePos{0};
    bool consumedPattern{false};

    const auto finishPartialMatch = [&consumedPattern, &matchResult]() -> std::optional<Match> {
        return consumedPattern ? std::optional<Match>{matchResult} : std::nullopt;
    };

    for(size_t i{0}; i < m_tokens.size(); ++i) {
        const auto& token = m_tokens.at(i);

        if(token.type == Token::Type::Literal) {
            if(token.value.isEmpty()) {
                continue;
            }

            if(!source.sliced(sourcePos).startsWith(token.value)) {
                return finishPartialMatch();
            }

            sourcePos += token.value.size();
            consumedPattern = true;
            continue;
        }

        const auto nextLiteralIt = std::ranges::find_if(
            m_tokens | std::views::drop(static_cast<qsizetype>(i + 1)),
            [](const Token& current) { return current.type == Token::Type::Literal && !current.value.isEmpty(); });

        QString capturedValue;

        if(nextLiteralIt == m_tokens.end()) {
            capturedValue   = source.sliced(sourcePos);
            sourcePos       = source.size();
            consumedPattern = true;
        }
        else {
            const qsizetype nextLiteralPos = source.indexOf(nextLiteralIt->value, sourcePos);
            if(nextLiteralPos < 0) {
                capturedValue = source.sliced(sourcePos);
                if(token.type == Token::Type::Capture) {
                    matchResult.values.emplace_back(token.value, capturedValue.trimmed());
                }
                consumedPattern = true;
                return finishPartialMatch();
            }

            capturedValue   = source.mid(sourcePos, nextLiteralPos - sourcePos);
            sourcePos       = nextLiteralPos;
            consumedPattern = true;
        }

        if(token.type == Token::Type::Capture) {
            matchResult.values.emplace_back(token.value, capturedValue.trimmed());
        }
    }

    return finishPartialMatch();
}

bool isWritableFillField(const QString& field)
{
    const QString tag = normaliseField(field);
    if(tag.isEmpty()) {
        return false;
    }

    const VariableKind kind = resolveBuiltInVariableKind(tag);
    return kind == VariableKind::Generic || isWritableBuiltInField(kind);
}

bool isMultiValueFillField(const QString& field)
{
    const QString tag = normaliseField(field);
    return Track::isMultiValueTag(tag) || resolveBuiltInVariableKind(tag) == VariableKind::Genres;
}

ScriptFieldValue fillFieldValue(const QString& field, const QString& value)
{
    QString trimmedValue = value.trimmed();

    if(isMultiValueFillField(field)) {
        QStringList values = trimmedValue.split(';'_L1, Qt::SkipEmptyParts);
        std::ranges::transform(values, values.begin(), [](const QString& current) { return current.trimmed(); });
        return values;
    }

    return trimmedValue;
}
} // namespace Fooyin::TagEditor
