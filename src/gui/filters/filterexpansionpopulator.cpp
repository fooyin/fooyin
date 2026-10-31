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

#include "filterexpansionpopulator.h"

#include <core/constants.h>
#include <core/library/tracksort.h>

#include <QFontMetrics>
#include <QThread>

using namespace Qt::StringLiterals;

namespace Fooyin::Filters {
void ExpansionScripts::load(const QJsonObject& object)
{
    header    = object.value("Header"_L1).toString(header);
    track     = object.value("Track"_L1).toString(track);
    footer    = object.value("Footer"_L1).toString(footer);
    groupBy   = object.value("GroupBy"_L1).toString(groupBy);
    subheader = object.value("Subheader"_L1).toString(subheader);
}

QJsonObject ExpansionScripts::toJson() const
{
    return {{u"Header"_s, header},
            {u"Track"_s, track},
            {u"Footer"_s, footer},
            {u"GroupBy"_s, groupBy},
            {u"Subheader"_s, subheader}};
}

ScriptDependencies ExpansionDependencies::combined() const
{
    auto result{header};
    result.merge(track);
    result.merge(footer);
    result.merge(grouping);
    result.merge(subheader);
    return result;
}

FilterExpansionPopulator::FilterExpansionPopulator(QObject* parent)
    : Worker{parent}
{ }

FilterExpansionPopulator::~FilterExpansionPopulator() = default;

void FilterExpansionPopulator::run(ExpansionRequest request, std::stop_token stop)
{
    Q_ASSERT(QThread::currentThread() == thread());

    if(startWork(stop)) {
        const auto result = evaluate(std::move(request), stop);
        m_request         = {};
        m_queueIndexes.clear();
        setState(Idle);
        if(!result.cancelled && !stop.stop_requested()) {
            Q_EMIT ready(result);
        }
    }

    Q_EMIT finished();
}

void FilterExpansionPopulator::clearGroups()
{
    Q_ASSERT(QThread::currentThread() == thread());

    m_groups.clear();
    m_groupedTracks.reset();
    m_groupingScript.clear();
}

ExpansionResult FilterExpansionPopulator::evaluate(ExpansionRequest request, std::stop_token stop)
{
    m_request           = std::move(request);
    m_result            = {};
    m_result.generation = m_request.generation;
    m_result.key        = m_request.key;
    m_result.tracks     = m_request.tracks;
    m_result.context    = m_request.context;
    m_result.rebuild    = m_request.rebuild;
    m_result.sections   = m_request.sections;

    if(m_request.tracks && !m_request.tracks->empty()) {
        m_result.coverTrack = m_request.tracks->front();
    }

    prepareScripts();

    if(stop.stop_requested() || !m_result.errors.empty()) {
        m_result.cancelled = stop.stop_requested();
        return std::move(m_result);
    }

    if(m_request.rebuild && !m_request.sortScript.isEmpty()) {
        TrackSorter sorter{m_request.libraries};
        m_request.tracks
            = std::make_shared<const TrackList>(sorter.calcSortTracks(m_request.sortScript, *m_request.tracks));
    }

    if(stop.stop_requested()) {
        m_result.cancelled = true;
        return std::move(m_result);
    }

    m_result.tracks                           = m_request.tracks;
    m_request.context.ownedTracks             = m_request.tracks;
    m_request.context.playlistData.trackCount = static_cast<int>(m_request.tracks->size());

    m_formatter.setBaseFont(m_request.font);
    prepareColumnValues();
    evaluateHeaderFooter();

    if(!updateGroups(stop) || !evaluateSubheaders(stop)) {
        return std::move(m_result);
    }
    evaluateTracks(stop);

    m_result.cancelled = stop.stop_requested();
    return std::move(m_result);
}

void FilterExpansionPopulator::addErrors(const QString& section, const ErrorList& errors)
{
    for(const auto& error : errors) {
        const QString message = section + u": "_s + error.message;
        if(!m_result.errors.contains(message)) {
            m_result.errors.push_back(message);
        }
    }
}

RichText FilterExpansionPopulator::formatted(const QString& text, const QString& section)
{
    auto rich = m_formatter.evaluate(text);
    addErrors(section, m_formatter.errors());
    return rich;
}

ScriptContext FilterExpansionPopulator::groupContext() const
{
    auto context      = m_request.context.context();
    context.libraries = &m_request.libraries;
    return context;
}

void FilterExpansionPopulator::prepareScripts()
{
    const bool groupingEnabled = !m_request.scripts.subheader.isEmpty();
    const auto& groupScript
        = m_request.scripts.groupBy.isEmpty() ? m_request.scripts.subheader : m_request.scripts.groupBy;

    if(!m_parsed || m_scripts != m_request.scripts) {
        m_scripts   = m_request.scripts;
        m_header    = m_parser.parse(m_request.scripts.header);
        m_track     = m_parser.parse(m_request.scripts.track);
        m_footer    = m_parser.parse(m_request.scripts.footer);
        m_grouping  = m_parser.parse(groupScript);
        m_subheader = m_parser.parse(m_request.scripts.subheader);

        m_dependencies.header = m_parser.dependencies(m_header);
        m_dependencies.track  = m_parser.dependencies(m_track);
        m_dependencies.footer = m_parser.dependencies(m_footer);
        m_dependencies.grouping
            = !groupingEnabled ? ScriptDependencies{.unclassified = false} : m_parser.dependencies(m_grouping);
        m_dependencies.subheader
            = !groupingEnabled ? ScriptDependencies{.unclassified = false} : m_parser.dependencies(m_subheader);

        m_parsed = true;
    }

    m_result.dependencies = m_dependencies;
    m_result.dependencies.track.merge(
        {.sources = {ScriptSources::PlaybackTrack, ScriptSources::PlaybackState}, .unclassified = false});

    addErrors(tr("Header"), m_header.errors);
    addErrors(tr("Track"), m_track.errors);
    addErrors(tr("Footer"), m_footer.errors);

    if(groupingEnabled && !m_request.scripts.groupBy.isEmpty()) {
        addErrors(tr("Group by"), m_grouping.errors);
    }
    if(groupingEnabled) {
        addErrors(tr("Subheader"), m_subheader.errors);
    }
}

void FilterExpansionPopulator::prepareColumnValues()
{
    ScriptDependencies columnDependencies{.unclassified = false};

    for(const auto& script : m_request.columnScripts) {
        const auto parsed = m_parser.parse(script);
        addErrors(tr("Filter columns"), parsed.errors);
        columnDependencies.merge(m_parser.dependencies(parsed));

        const auto text = m_parser.evaluate(parsed, *m_request.tracks, groupContext(), m_request.options);
        m_request.columnValues.push_back(
            formatted(text.section(QLatin1String{Constants::UnitSeparator}, 0, 0), tr("Filter columns")).joinedText());
    }

    m_result.dependencies.header.merge(columnDependencies);
    m_result.dependencies.track.merge(columnDependencies);
    m_result.dependencies.footer.merge(columnDependencies);

    if(!m_request.scripts.subheader.isEmpty()) {
        m_result.dependencies.grouping.merge(columnDependencies);
        m_result.dependencies.subheader.merge(columnDependencies);
    }

    const QStringList columnValues{m_request.columnValues.cbegin(), m_request.columnValues.cend()};
    const bool hasColumnValues
        = std::ranges::any_of(m_request.columnValues, [](const QString& value) { return !value.isEmpty(); });

    m_request.context.values.push_back({.name   = u"FILTER_COLUMNS"_s,
                                        .result = {.value = columnValues.join(u'\n'), .cond = hasColumnValues},
                                        .track  = {}});
    m_request.context.values.push_back(
        {.name   = u"FILTER_COLUMN_COUNT"_s,
         .result = {.value = QString::number(m_request.columnValues.size()), .cond = !m_request.columnValues.empty()},
         .track  = {}});

    for(size_t i{0}; i < m_request.columnValues.size(); ++i) {
        m_request.context.values.push_back(
            {.name   = u"_FOOYIN_FILTER_COLUMN_%1"_s.arg(i + 1),
             .result = {.value = m_request.columnValues[i], .cond = !m_request.columnValues[i].isEmpty()},
             .track  = {}});
    }

    m_result.context = m_request.context;
}

void FilterExpansionPopulator::evaluateHeaderFooter()
{
    if(m_request.sections & ExpansionHeader) {
        const auto text = m_parser.evaluate(m_header, *m_request.tracks, groupContext(), m_request.options);
        m_result.header = prepareRichTextLayout(formatted(text, tr("Header")), m_request.font);
    }
    if(m_request.sections & ExpansionFooter) {
        const auto text = m_parser.evaluate(m_footer, *m_request.tracks, groupContext(), m_request.options);
        m_result.footer = prepareRichTextLayout(formatted(text, tr("Footer")), m_request.font);
    }
}

bool FilterExpansionPopulator::updateGroups(std::stop_token stop)
{
    const bool groupingEnabled = !m_request.scripts.subheader.isEmpty();
    const auto& groupScript
        = m_request.scripts.groupBy.isEmpty() ? m_request.scripts.subheader : m_request.scripts.groupBy;
    const bool regroup = groupingEnabled
                      && (m_groupedTracks != m_request.tracks || m_groupingScript != groupScript
                          || (m_request.sections & ExpansionGrouping));

    m_queueIndexes.clear();
    if(regroup || (m_request.sections & ExpansionTracks)) {
        for(int i{0}; const auto& queued : m_request.queue) {
            m_queueIndexes[queued.track.id()].push_back(i++);
        }
    }

    if(regroup) {
        auto options{m_request.options};
        if(!m_request.scripts.groupBy.isEmpty()) {
            options.escapeRichText = false;
        }

        std::vector<std::pair<int, int>> ranges;
        QString previousKey;

        for(int row{0}; std::cmp_less(row, m_request.tracks->size()); ++row) {
            if(stop.stop_requested()) {
                m_result.cancelled = true;
                return false;
            }

            setRowContext(row);

            const auto key = m_parser.evaluate(m_grouping, m_request.tracks->at(row), groupContext(), options);
            if(ranges.empty() || key != previousKey) {
                ranges.emplace_back(row, 0);
                previousKey = key;
            }
            ++ranges.back().second;
        }

        m_groups.clear();

        for(const auto& [first, count] : ranges) {
            m_groups.emplace_back(first, std::make_shared<const TrackList>(m_request.tracks->cbegin() + first,
                                                                           m_request.tracks->cbegin() + first + count));
        }

        m_groupedTracks  = m_request.tracks;
        m_groupingScript = groupScript;
    }
    else if(!groupingEnabled) {
        clearGroups();
    }

    return true;
}

bool FilterExpansionPopulator::evaluateSubheaders(std::stop_token stop)
{
    if(m_request.sections & (ExpansionGrouping | ExpansionSubheaders)) {
        m_result.sections |= ExpansionSubheaders;

        for(const auto& group : m_groups) {
            if(stop.stop_requested()) {
                m_result.cancelled = true;
                return false;
            }

            ScriptPlaylistData data;
            data.trackCount = static_cast<int>(group.tracks->size());
            data.depth      = 1;

            auto context{groupContext()};
            context.tracks       = group.tracks.get();
            context.playlistData = &data;

            const auto text = m_parser.evaluate(m_subheader, *group.tracks, context, m_request.options);
            const auto rich = formatted(text, tr("Subheader"));

            ExpansionSubheader header;
            header.firstTrack     = group.firstTrack;
            header.trackCount     = data.trackCount;
            header.content.layout = prepareRichTextLayout(rich, m_request.font);
            header.content.text   = richTextLayoutPlainText(header.content.layout);
            header.content.height = header.content.layout.height > 0 ? header.content.layout.height + 10 : 0;

            m_result.subheaders.push_back(std::move(header));
        }
    }

    return true;
}

void FilterExpansionPopulator::evaluateTracks(std::stop_token stop)
{
    if(m_request.sections & ExpansionTracks) {
        const auto evaluateRow = [&](int row) {
            const Track& track = m_request.tracks->at(row);
            setRowContext(row);

            const auto& data   = m_request.context.playlistData;
            const bool playing = data.playingIcon != ScriptPlayingIcon::None;
            const auto& font   = playing && m_request.playingRowFont ? *m_request.playingRowFont : m_request.font;
            m_formatter.setBaseFont(font);

            const auto text = m_parser.evaluate(m_track, track, groupContext(), m_request.options);
            const auto rich = formatted(text, tr("Track"));

            ExpansionRow resultRow;
            resultRow.layout      = prepareRichTextLayout(rich, font);
            resultRow.text        = richTextLayoutPlainText(resultRow.layout);
            resultRow.height      = std::max(QFontMetrics{font}.height(), resultRow.layout.height) + 5;
            resultRow.playingIcon = data.playingIcon;
            if(playing) {
                resultRow.font       = m_request.playingRowFont;
                resultRow.background = m_request.playingRowColour;
            }

            m_result.rows.emplace_back(row, std::move(resultRow));
        };

        if(m_request.rows.empty()) {
            m_result.rows.reserve(m_request.tracks->size());
            for(int row{0}; std::cmp_less(row, m_request.tracks->size()) && !stop.stop_requested(); ++row) {
                evaluateRow(row);
            }
        }
        else {
            for(const int row : m_request.rows) {
                if(stop.stop_requested()) {
                    break;
                }
                if(row >= 0 && std::cmp_less(row, m_request.tracks->size())) {
                    evaluateRow(row);
                }
            }
        }
    }
}

void FilterExpansionPopulator::setRowContext(int row)
{
    const Track& track = m_request.tracks->at(row);
    auto& data         = m_request.context.playlistData;
    data.trackIndex    = row;
    data.queueTotal    = static_cast<int>(m_request.queue.size());
    data.queueIndexes.clear();

    if(const auto found = m_queueIndexes.find(track.id()); found != m_queueIndexes.end()) {
        for(const int index : found->second) {
            if(m_request.queue[index].track.sameIdentityAs(track)) {
                data.queueIndexes.push_back(index);
            }
        }
    }

    data.playingIcon = ScriptPlayingIcon::None;

    if(track.sameIdentityAs(m_request.context.playback.track)) {
        data.playingTrackIndex = row;
        data.playingTrackId    = track.id();
        if(m_request.context.playback.playState == Player::PlayState::Playing) {
            data.playingIcon = ScriptPlayingIcon::Play;
        }
        else if(m_request.context.playback.playState == Player::PlayState::Paused) {
            data.playingIcon = ScriptPlayingIcon::Pause;
        }
    }
    else {
        data.playingTrackIndex = -1;
        data.playingTrackId    = -1;
    }
}
} // namespace Fooyin::Filters

#include "moc_filterexpansionpopulator.cpp"
