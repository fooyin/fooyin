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

#include "radioguideconfig.h"

#include <utils/treeitem.h>
#include <utils/treemodel.h>

#include <QByteArray>

#include <memory>
#include <optional>
#include <vector>

class QMimeData;

namespace Fooyin::RadioBrowser {
enum class RadioGuideConfigItemKind : uint8_t
{
    Section = 0,
    Preset,
};

struct RadioGuideConfigItem : TreeItem<RadioGuideConfigItem>
{
    using TreeItem::TreeItem;

    RadioGuideConfigItemKind kind{RadioGuideConfigItemKind::Section};
    QString name;
    QString tag;
};

class RadioGuideConfigModel : public TreeModel<RadioGuideConfigItem>
{
    Q_OBJECT

public:
    enum Role : uint16_t
    {
        ItemKindRole = Qt::UserRole,
    };

    using TreeModel::TreeModel;

    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    [[nodiscard]] QStringList mimeTypes() const override;
    [[nodiscard]] QMimeData* mimeData(const QModelIndexList& indexes) const override;
    [[nodiscard]] bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                       const QModelIndex& parent) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                      const QModelIndex& parent) override;
    [[nodiscard]] Qt::DropActions supportedDragActions() const override;
    [[nodiscard]] Qt::DropActions supportedDropActions() const override;

    void setSections(const RadioGuideTagSections& sections);
    [[nodiscard]] RadioGuideTagSections sections() const;
    [[nodiscard]] QModelIndex addSection(const QString& name, int row = -1);
    [[nodiscard]] QModelIndex addPreset(const QModelIndex& sectionIndex, const RadioGuideTag& tag, int row = -1);
    bool removeIndex(const QModelIndex& index);
    [[nodiscard]] QModelIndex moveIndex(const QModelIndex& index, int offset);

private:
    struct DragItem
    {
        int parentRow{-1};
        int row{-1};
    };

    [[nodiscard]] RadioGuideConfigItem* createNode(RadioGuideConfigItem* parent = nullptr);
    void clearChildren(RadioGuideConfigItem* parent);
    void collectDescendants(RadioGuideConfigItem* parent, std::vector<RadioGuideConfigItem*>& descendants) const;
    [[nodiscard]] RadioGuideConfigItem* sectionForIndex(const QModelIndex& index) const;
    [[nodiscard]] QModelIndex indexForDragItem(const DragItem& item) const;
    static std::optional<DragItem> dragItemFromData(const QMimeData* data);

    std::vector<std::unique_ptr<RadioGuideConfigItem>> m_nodes;
};
} // namespace Fooyin::RadioBrowser
