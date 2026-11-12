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

#include "listeninghistorymodel.h"

#include <utils/stringutils.h>

#include <QDateTime>
#include <QFont>
#include <QLocale>

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
QString sourceName(const PlaybackSource& source)
{
    return source.displayName.isEmpty() ? source.originalUri : source.displayName;
}

QString streamMetadataText(const StreamMetadataSpan& span)
{
    QString metadata = span.rawStreamTitle;
    if(metadata.isEmpty()) {
        metadata = span.artists.join(u", "_s);
        if(!metadata.isEmpty() && !span.title.isEmpty()) {
            metadata += u" — "_s;
        }
        metadata += span.title;
    }
    return metadata.isEmpty() ? ListeningHistoryModel::tr("Untitled stream metadata") : metadata;
}
} // namespace

ListeningHistoryItem::ListeningHistoryItem(Type type)
    : m_type{type}
{ }

ListeningHistoryItem::Type ListeningHistoryItem::type() const
{
    return m_type;
}

qint64 ListeningHistoryItem::key() const
{
    return m_key;
}

const ItemColumns& ListeningHistoryItem::columns() const
{
    return m_columns;
}

QString ListeningHistoryItem::column(int index) const
{
    if(index < 0 || index >= ColumnCount) {
        return {};
    }
    return m_columns.at(index);
}

void ListeningHistoryItem::setType(Type type)
{
    m_type = type;
}

void ListeningHistoryItem::setKey(qint64 key)
{
    m_key = key;
}

void ListeningHistoryItem::setColumns(ItemColumns columns)
{
    m_columns = std::move(columns);
}

void ListeningHistoryItem::setColumn(int index, QString text)
{
    if(index < 0 || index >= ColumnCount) {
        return;
    }
    m_columns[index] = std::move(text);
}

ListeningHistoryModel::ListeningHistoryModel(QObject* parent)
    : TreeModel{parent}
{ }

ListeningHistoryModel::~ListeningHistoryModel() = default;

void ListeningHistoryModel::clear()
{
    beginResetModel();
    resetRoot();
    m_items.clear();
    m_dates.clear();
    m_events.clear();
    m_progress.clear();
    m_metadata.clear();
    endResetModel();
}

QModelIndex ListeningHistoryModel::eventIndex(qint64 eventId) const
{
    const auto it = m_events.find(eventId);
    return it == m_events.cend() ? QModelIndex{} : indexOfItem(it->second);
}

uint64_t ListeningHistoryModel::eventCount() const
{
    return m_events.size();
}

void ListeningHistoryModel::updateListeningTime(qint64 eventId, uint64_t listeningTimeMs)
{
    const auto it = m_events.find(eventId);
    if(it == m_events.cend()) {
        return;
    }

    auto* event{it->second};
    auto& progress  = m_progress.at(eventId).second;
    progress        = std::max(progress, listeningTimeMs);
    const auto text = Utils::msToString(progress);
    if(event->column(4) != text) {
        event->setColumn(4, text);
        const auto changed = indexOfItem(event).siblingAtColumn(4);
        Q_EMIT dataChanged(changed, changed, {Qt::DisplayRole});
    }

    if(const auto spans = m_metadata.find(eventId); spans != m_metadata.cend() && !spans->second.empty()) {
        const auto& span = spans->second.back();
        if(!span.endedOffsetMs && event->childCount() == static_cast<int>(spans->second.size())
           && progress >= span.startedOffsetMs) {
            auto* child = event->child(event->childCount() - 1);
            child->setColumn(4, Utils::msToString(progress - span.startedOffsetMs));
            const auto changed = indexOfItem(child).siblingAtColumn(4);
            Q_EMIT dataChanged(changed, changed, {Qt::DisplayRole});
        }
    }
}

void ListeningHistoryModel::appendEntries(const std::vector<PlaybackHistoryEntry>& entries)
{
    for(const auto& entry : entries) {
        const QDateTime started = QDateTime::fromMSecsSinceEpoch(entry.event.startedAt);
        const qint64 dateKey    = started.date().toJulianDay();

        ListeningHistoryItem* event{nullptr};
        if(const auto existing = m_events.find(entry.event.id); existing != m_events.cend()) {
            event = existing->second;
        }
        else {
            ListeningHistoryItem* dateNode{nullptr};
            if(const auto dateIt = m_dates.find(dateKey); dateIt != m_dates.cend()) {
                dateNode = dateIt->second;
            }
            else {
                int row = 0;
                while(row < rootItem()->childCount() && rootItem()->child(row)->key() > dateKey) {
                    ++row;
                }
                beginInsertRows({}, row, row);
                dateNode = createItem(ListeningHistoryItem::Type::Date);
                dateNode->setKey(dateKey);
                dateNode->setColumns({QLocale{}.toString(started.date(), QLocale::LongFormat), {}, {}, {}, {}});
                rootItem()->insertChild(row, dateNode);
                m_dates.emplace(dateKey, dateNode);
                endInsertRows();
            }

            const auto order = std::pair{entry.event.startedAt, entry.event.id};
            int row{0};
            while(row < dateNode->childCount()) {
                const auto* child = dateNode->child(row);
                if(std::pair{m_progress.at(child->key()).first, child->key()} < order) {
                    break;
                }
                ++row;
            }

            beginInsertRows(indexOfItem(dateNode), row, row);
            event = createItem(ListeningHistoryItem::Type::Event);
            event->setKey(entry.event.id);
            dateNode->insertChild(row, event);
            m_events.emplace(entry.event.id, event);
            m_progress.emplace(entry.event.id, std::pair{entry.event.startedAt, uint64_t{0}});
            endInsertRows();
        }

        QString title{entry.event.title};
        if(title.isEmpty() || entry.source.kind == PlaybackSourceKind::RemoteStream) {
            title = sourceName(entry.source);
        }
        event->setColumns({QLocale{}.toString(started.time(), QLocale::ShortFormat), title,
                           entry.event.artists.join(u", "_s), entry.event.album, event->column(4)});
        updateListeningTime(entry.event.id, entry.event.listeningTimeMs);

        const auto changed = indexOfItem(event);
        Q_EMIT dataChanged(changed, changed.siblingAtColumn(4), {Qt::DisplayRole});
        if(entry.metadataSpanCount > 0 && event->childCount() == 0) {
            beginInsertRows(changed, 0, 0);
            auto* placeholder = createItem(ListeningHistoryItem::Type::MetadataPending);
            placeholder->setColumn(0, tr("Expand to load stream metadata"));
            event->appendChild(placeholder);
            endInsertRows();
        }
    }
}

std::optional<qint64> ListeningHistoryModel::beginMetadataLoad(const QModelIndex& eventIndex)
{
    if(!eventIndex.isValid()) {
        return {};
    }

    const auto* event = itemForIndex(eventIndex);
    auto* placeholder = event->child(0);
    if(event->type() != ListeningHistoryItem::Type::Event || event->childCount() != 1 || !placeholder
       || placeholder->type() != ListeningHistoryItem::Type::MetadataPending) {
        return {};
    }

    placeholder->setType(ListeningHistoryItem::Type::MetadataLoading);
    placeholder->setColumn(0, tr("Loading stream metadata…"));

    const QModelIndex changedIndex = index(0, 0, indexOfItem(event));
    Q_EMIT dataChanged(changedIndex, changedIndex, {Qt::DisplayRole});
    return event->key();
}

void ListeningHistoryModel::setMetadataSpans(qint64 eventId, const std::vector<StreamMetadataSpan>& spans)
{
    const auto eventIt = m_events.find(eventId);
    if(eventIt == m_events.cend()) {
        return;
    }

    ListeningHistoryItem* event  = eventIt->second;
    const QModelIndex eventIndex = indexOfItem(event);

    auto merged{spans};
    if(const auto previous = m_metadata.find(eventId); previous != m_metadata.cend()) {
        if(previous->second.size() > merged.size()) {
            return;
        }
        for(size_t row{0}; row < previous->second.size(); ++row) {
            if(previous->second[row].id == merged[row].id && previous->second[row].endedOffsetMs) {
                merged[row].endedOffsetMs = previous->second[row].endedOffsetMs;
            }
        }
    }
    m_metadata[eventId] = std::move(merged);

    if(event->childCount() > 0 && event->child(0)->type() != ListeningHistoryItem::Type::Metadata) {
        const auto removedItems = event->children();
        beginRemoveRows(eventIndex, 0, event->childCount() - 1);
        event->clearChildren();
        std::erase_if(m_items, [&removedItems](const auto& item) {
            return std::ranges::find(removedItems, item.get()) != removedItems.cend();
        });
        endRemoveRows();
    }

    for(size_t row{0}; row < m_metadata.at(eventId).size(); ++row) {
        const auto& span = m_metadata.at(eventId)[row];
        auto* child      = event->child(static_cast<int>(row));
        if(!child) {
            beginInsertRows(eventIndex, static_cast<int>(row), static_cast<int>(row));
            child = createItem(ListeningHistoryItem::Type::Metadata);
            event->appendChild(child);
            endInsertRows();
        }
        child->setColumns(
            {u"+%1"_s.arg(Utils::msToString(span.startedOffsetMs)), streamMetadataText(span),
             span.artists.join(u", "_s), span.album,
             span.endedOffsetMs ? Utils::msToString(*span.endedOffsetMs - span.startedOffsetMs) : QString{}});
        const auto changed = indexOfItem(child);
        Q_EMIT dataChanged(changed, changed.siblingAtColumn(4), {Qt::DisplayRole});
    }

    if(spans.empty() && event->childCount() == 0) {
        beginInsertRows(eventIndex, 0, 0);
        auto* child = createItem(ListeningHistoryItem::Type::NoMetadata);
        child->setColumn(0, tr("No timed stream metadata"));
        event->appendChild(child);
        endInsertRows();
    }

    updateListeningTime(eventId, m_progress.at(eventId).second);
}

int ListeningHistoryModel::columnCount(const QModelIndex& /*parent*/) const
{
    return ColumnCount;
}

QVariant ListeningHistoryModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const auto* item = itemForIndex(index);

    if(role == Qt::DisplayRole || role == Qt::ToolTipRole) {
        return item->column(index.column());
    }
    if(role == Qt::TextAlignmentRole && index.column() == 4) {
        return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
    }
    if(role == Qt::FontRole && item->type() == ListeningHistoryItem::Type::Date) {
        QFont font;
        font.setBold(true);
        return font;
    }

    return {};
}

QVariant ListeningHistoryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    static const std::array headers{tr("Time"), tr("Track"), tr("Artist"), tr("Album"), tr("Listened")};
    return section >= 0 && section < ColumnCount ? headers.at(section) : QVariant{};
}

ListeningHistoryItem* ListeningHistoryModel::createItem(ListeningHistoryItem::Type type)
{
    auto item   = std::make_unique<ListeningHistoryItem>(type);
    auto* value = item.get();
    m_items.push_back(std::move(item));
    return value;
}
} // namespace Fooyin

#include "moc_listeninghistorymodel.cpp"
