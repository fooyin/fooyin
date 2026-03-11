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

#include "filterpopulator.h"

#include <core/constants.h>
#include <core/coresettings.h>
#include <gui/scripting/richtextutils.h>
#include <utils/crypto.h>
#include <utils/settings/settingsmanager.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Filters {
namespace {
struct ColumnData
{
    QStringList plainColumns;
    std::vector<RichText> richColumns;
};

ColumnData buildColumnData(const QStringList& columns, ScriptFormatter& formatter)
{
    ColumnData data;
    data.plainColumns.reserve(columns.size());
    data.richColumns.reserve(columns.size());

    for(const QString& column : columns) {
        RichText richColumn = trimRichText(formatter.evaluate(column));
        QString plainColumn = richColumn.joinedText();

        if(plainColumn.isEmpty() && !column.isEmpty()) {
            RichText placeholder = trimRichText(formatter.evaluate(column + u"?"_s));
            if(!placeholder.empty()) {
                richColumn  = std::move(placeholder);
                plainColumn = richColumn.joinedText();
            }
        }

        data.richColumns.push_back(richColumn);
        data.plainColumns.push_back(plainColumn);
    }

    return data;
}
} // namespace

FilterPopulator::FilterPopulator(LibraryManager* libraryManager, QObject* parent)
    : Worker{parent}
    , m_scriptEnvironment{libraryManager}
{ }

void FilterPopulator::setFont(const QFont& font)
{
    m_formatter.setBaseFont(font);
}

void FilterPopulator::run(const QStringList& columns, const TrackList& tracks, bool useVarious)
{
    setState(Running);

    m_data.clear();

    m_scriptEnvironment.setEvaluationPolicy(TrackListContextPolicy::Unresolved, {}, false, useVarious);

    const QString newColumns = columns.join("\036"_L1);
    if(std::exchange(m_currentColumns, newColumns) != newColumns) {
        m_script = m_parser.parse(m_currentColumns);
    }

    const bool success = runBatch(tracks);

    setState(Idle);

    if(success) {
        emit finished();
    }
}

FilterItem* FilterPopulator::getOrInsertItem(const QStringList& columns, const std::vector<RichText>& richColumns)
{
    const auto key = Utils::generateMd5Hash(columns.join(QString{}));
    if(!m_data.items.contains(key)) {
        auto [it, _] = m_data.items.emplace(key, FilterItem{key, columns, &m_root});
        it->second.setRichColumns(richColumns);
    }
    return &m_data.items.at(key);
}

std::vector<FilterItem*> FilterPopulator::getOrInsertItems(const QList<QStringList>& columnSet)
{
    std::vector<FilterItem*> items;
    items.reserve(columnSet.size());
    for(const QStringList& columns : columnSet) {
        const auto columnData = buildColumnData(columns, m_formatter);
        auto* filterItem      = getOrInsertItem(columnData.plainColumns, columnData.richColumns);
        items.emplace_back(filterItem);
    }
    return items;
}

void FilterPopulator::addTrackToNode(const Track& track, FilterItem* node)
{
    node->addTrack(track);
    m_data.trackParents[track.id()].push_back(node->key());
}

bool FilterPopulator::runBatch(const TrackList& tracks)
{
    ScriptContext context;
    context.environment = &m_scriptEnvironment;

    for(const Track& track : tracks) {
        if(!mayRun()) {
            return false;
        }

        if(track.isInLibrary()) {
            const QString columns = m_parser.evaluate(m_script, track, context);

            if(columns.contains(QLatin1String{Constants::UnitSeparator})) {
                const QStringList values = columns.split(QLatin1String{Constants::UnitSeparator});

                QList<QStringList> colValues;
                std::ranges::transform(values, std::back_inserter(colValues), [](const QString& col) {
                    return col.split(QLatin1String{Constants::RecordSeparator});
                });

                const auto nodes = getOrInsertItems(colValues);
                for(FilterItem* node : nodes) {
                    addTrackToNode(track, node);
                }
            }
            else {
                const auto columnData
                    = buildColumnData(columns.split(QLatin1String{Constants::RecordSeparator}), m_formatter);
                FilterItem* node = getOrInsertItem(columnData.plainColumns, columnData.richColumns);
                addTrackToNode(track, node);
            }
        }
    }

    if(!mayRun()) {
        return false;
    }

    emit populated(std::make_shared<PendingTreeData>(std::move(m_data)));
    m_data = {};

    return true;
}
} // namespace Fooyin::Filters

#include "moc_filterpopulator.cpp"
