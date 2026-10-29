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

#include "filterrows.h"

#include "gui/scripting/richtextlayout.h"

#include <core/player/playbackqueue.h>
#include <core/scripting/scriptcontextservice.h>
#include <gui/scripting/scriptformatter.h>
#include <utils/worker.h>

#include <QJsonObject>

#include <memory>
#include <stop_token>
#include <unordered_map>

namespace Fooyin::Filters {
struct ExpansionScripts
{
    QString header{QStringLiteral("<sized=1><b>%filter_columns%</b></sized>")};
    QString groupBy;
    QString subheader{QStringLiteral("$ifgreater(%disctotal%,1,\n"
                                     "Disc #%disc%\n"
                                     "<hr/>\n"
                                     "<right>%playtime%</right>\n"
                                     ")")};
    QString track{QStringLiteral(
        "$if2(%playingicon%,$if(%tracknumber%,$num(%tracknumber%,2),))  %title%<right>%length%</right>")};
    QString footer;

    bool operator==(const ExpansionScripts& other) const = default;

    void load(const QJsonObject& object);
    [[nodiscard]] QJsonObject toJson() const;
};

enum ExpansionSection : uint8_t
{
    ExpansionHeader     = 1 << 0,
    ExpansionTracks     = 1 << 1,
    ExpansionFooter     = 1 << 2,
    ExpansionGrouping   = 1 << 3,
    ExpansionSubheaders = 1 << 4,

    ExpansionAll = ExpansionHeader | ExpansionTracks | ExpansionFooter | ExpansionGrouping | ExpansionSubheaders,
};

struct ExpansionDependencies
{
    ScriptDependencies header{.unclassified = false};
    ScriptDependencies track{.unclassified = false};
    ScriptDependencies footer{.unclassified = false};
    ScriptDependencies grouping{.unclassified = false};
    ScriptDependencies subheader{.unclassified = false};

    [[nodiscard]] ScriptDependencies combined() const;
};

struct ExpansionRow
{
    QString text;
    RichTextLayout layout;
    ScriptPlayingIcon playingIcon{ScriptPlayingIcon::None};
    int height{0};
};

struct ExpansionSubheader
{
    int firstTrack{0};
    int trackCount{0};
    ExpansionRow content;
};

struct ExpansionRequest
{
    uint64_t generation{0};
    RowKey key;
    ExpansionScripts scripts;
    std::shared_ptr<const TrackList> tracks;
    std::vector<QString> columnValues;
    std::vector<QString> columnScripts;
    QString sortScript;
    ScriptContextData context;
    ScriptEvaluationOptions options;
    LibraryInfoMap libraries;
    QueueTracks queue;
    QFont font;
    bool rebuild{true};
    uint8_t sections{ExpansionAll};
    //! Empty means all rows when ExpansionTracks is requested
    std::vector<int> rows;
};

struct ExpansionResult
{
    uint64_t generation{0};
    RowKey key;
    std::shared_ptr<const TrackList> tracks;
    ScriptContextData context;
    ExpansionDependencies dependencies;
    Track coverTrack;
    RichTextLayout header;
    RichTextLayout footer;
    std::vector<std::pair<int, ExpansionRow>> rows;
    std::vector<ExpansionSubheader> subheaders;
    QStringList errors;
    bool rebuild{true};
    bool cancelled{false};
    uint8_t sections{ExpansionAll};
};

class FilterExpansionPopulator : public Worker
{
    Q_OBJECT

public:
    explicit FilterExpansionPopulator(QObject* parent = nullptr);
    ~FilterExpansionPopulator() override;

    void run(ExpansionRequest request, std::stop_token stop);
    void clearGroups();

Q_SIGNALS:
    void ready(const Fooyin::Filters::ExpansionResult& result);

private:
    ExpansionResult evaluate(ExpansionRequest request, std::stop_token stop);

    void addErrors(const QString& section, const ErrorList& errors);
    RichText formatted(const QString& text, const QString& section);
    [[nodiscard]] ScriptContext groupContext() const;

    void prepareScripts();
    void prepareColumnValues();
    void evaluateHeaderFooter();
    bool updateGroups(std::stop_token stop);
    bool evaluateSubheaders(std::stop_token stop);
    void evaluateTracks(std::stop_token stop);
    void setRowContext(int row);

    struct Group
    {
        int firstTrack{0};
        std::shared_ptr<const TrackList> tracks;
    };

    ScriptParser m_parser;
    ScriptFormatter m_formatter;

    ExpansionScripts m_scripts;
    ParsedScript m_header;
    ParsedScript m_track;
    ParsedScript m_footer;
    ParsedScript m_grouping;
    ParsedScript m_subheader;

    std::shared_ptr<const TrackList> m_groupedTracks;
    QString m_groupingScript;
    std::vector<Group> m_groups;
    ExpansionDependencies m_dependencies;
    bool m_parsed{false};

    std::unordered_map<int, std::vector<int>> m_queueIndexes;
    ExpansionRequest m_request;
    ExpansionResult m_result;
};
} // namespace Fooyin::Filters

Q_DECLARE_METATYPE(Fooyin::Filters::ExpansionResult)
