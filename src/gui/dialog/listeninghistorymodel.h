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

#include <core/player/playbackhistorydefs.h>
#include <utils/treeitem.h>
#include <utils/treemodel.h>

#include <array>
#include <optional>
#include <unordered_map>

namespace Fooyin {
constexpr auto ColumnCount = 5;
using ItemColumns          = std::array<QString, ColumnCount>;

class ListeningHistoryItem : public TreeItem<ListeningHistoryItem>
{
public:
    enum class Type : uint8_t
    {
        Root = 0,
        Date,
        Event,
        MetadataPending,
        MetadataLoading,
        Metadata,
        NoMetadata,
    };

    explicit ListeningHistoryItem(Type type = Type::Root);

    Type type() const;
    qint64 key() const;
    const ItemColumns& columns() const;
    QString column(int index) const;

    void setType(Type type);
    void setKey(qint64 key);
    void setColumns(ItemColumns columns);
    void setColumn(int index, QString text);

private:
    Type m_type;
    qint64 m_key{-1};
    ItemColumns m_columns;
};

class ListeningHistoryModel : public TreeModel<ListeningHistoryItem>
{
    Q_OBJECT

public:
    explicit ListeningHistoryModel(QObject* parent = nullptr);
    ~ListeningHistoryModel() override;

    void clear();

    void updateListeningTime(qint64 eventId, uint64_t listeningTimeMs);
    [[nodiscard]] QModelIndex eventIndex(qint64 eventId) const;
    [[nodiscard]] uint64_t eventCount() const;

    void appendEntries(const std::vector<PlaybackHistoryEntry>& entries);
    [[nodiscard]] std::optional<qint64> beginMetadataLoad(const QModelIndex& eventIndex);
    void setMetadataSpans(qint64 eventId, const std::vector<StreamMetadataSpan>& spans);

    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    ListeningHistoryItem* createItem(ListeningHistoryItem::Type type);

    std::vector<std::unique_ptr<ListeningHistoryItem>> m_items;
    std::unordered_map<qint64, ListeningHistoryItem*> m_dates;
    std::unordered_map<qint64, ListeningHistoryItem*> m_events;
    std::unordered_map<qint64, std::pair<qint64, uint64_t>> m_progress;
    std::unordered_map<qint64, std::vector<StreamMetadataSpan>> m_metadata;
};
} // namespace Fooyin
