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

#include "fygui_export.h"

#include <core/scripting/expression.h>

#include <QCoreApplication>
#include <QDataStream>
#include <QDate>

#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace Fooyin {
struct AutoPlaylistQuery
{
    QString name;
    QString query;
    QString sortQuery;
    bool forceSorted{false};

    friend QDataStream& operator<<(QDataStream& stream, const AutoPlaylistQuery& preset)
    {
        static constexpr int version{2};
        stream << version;
        stream << preset.name;
        stream << preset.query;
        stream << preset.sortQuery;
        stream << preset.forceSorted;

        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, AutoPlaylistQuery& preset)
    {
        int version{0};
        stream >> version;

        stream >> preset.name;
        stream >> preset.query;

        if(version >= 2) {
            stream >> preset.sortQuery;
            stream >> preset.forceSorted;
        }

        return stream;
    }
};

enum class AutoPlaylistFieldType : uint8_t
{
    Text = 0,
    Number,
    Rating,
    Duration,
    Date,
    Boolean,
    Metadata,
};

enum class AutoPlaylistFieldSource : uint8_t
{
    BuiltIn = 0,
    Metadata,
};

struct AutoPlaylistField
{
    QString name;
    QString label;
    AutoPlaylistFieldType type;
};

struct AutoPlaylistRule
{
    QString field{QStringLiteral("artist")};
    AutoPlaylistFieldSource source{AutoPlaylistFieldSource::BuiltIn};
    Expr::Type comparison{Expr::Contains};
    bool negated{false};
    QString value;
    std::vector<QString> values;
    QDate date{QDate::currentDate()};
    QDate endDate{QDate::currentDate()};
    int period{1};
    int unit{3};
    bool relativeDate{false};
};

struct AutoPlaylistRuleGroup
{
    enum Match : uint8_t
    {
        All = 0,
        Any,
        Odd,
    };

    Match match{All};
    using Item = std::variant<AutoPlaylistRule, AutoPlaylistRuleGroup>;
    std::vector<Item> rules;
};

struct AutoPlaylistRules
{
    AutoPlaylistRuleGroup group;
    bool allTracks{false};
    QString sortField;
    AutoPlaylistFieldSource sortSource{AutoPlaylistFieldSource::BuiltIn};
    bool descending{false};
    bool randomSort{false};
    int limit{0};
};

struct AutoPlaylistDateUnit
{
    QString keyword;
    const char* singular;
    const char* plural;
};

class FYGUI_EXPORT AutoPlaylistQueryBuilder
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::AutoPlaylistQuery)

public:
    static std::span<const AutoPlaylistField> fields();
    static std::span<const AutoPlaylistDateUnit> dateUnits();

    static int fieldIndex(const QString& name, AutoPlaylistFieldSource source = AutoPlaylistFieldSource::BuiltIn);
    static AutoPlaylistFieldType fieldType(const QString& name,
                                           AutoPlaylistFieldSource source = AutoPlaylistFieldSource::BuiltIn);

    static std::vector<Expr::Type> comparisons(AutoPlaylistFieldType type);
    static bool isListValueValid(const QString& value, AutoPlaylistFieldType type);
    static std::pair<QString, QString> query(const AutoPlaylistRules& rules);
    static std::optional<AutoPlaylistRules> rules(const QString& query, const QString& sortQuery);
    static QString formatQuery(const QString& query);
    static QString queryError(const QString& query, const QString& sortQuery);
};
} // namespace Fooyin
