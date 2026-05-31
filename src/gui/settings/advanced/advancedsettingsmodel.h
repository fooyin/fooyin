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

#include <utils/settings/advancedsettingsregistry.h>
#include <utils/treeitem.h>
#include <utils/treemodel.h>

namespace Fooyin {
enum class AdvancedItemType : uint8_t
{
    Category = 0,
    Setting,
    RadioOption,
};

struct AdvancedNode : TreeItem<AdvancedNode>
{
    using TreeItem::TreeItem;

    AdvancedItemType type{AdvancedItemType::Category};
    QString label;
    QString description;
    AdvancedSettingDescriptor descriptor;
    QVariant value;
};

class AdvancedSettingsModel : public TreeModel<AdvancedNode>
{
    Q_OBJECT

public:
    enum AdvancedItemRole
    {
        ItemType = Qt::UserRole,
        IsSpinBoxEditor,
        MinimumValue,
        MaximumValue,
        SingleStep,
        Suffix,
        SpecialValueText,
        StableKey,
    };

    explicit AdvancedSettingsModel(AdvancedSettingsRegistry* registry, QObject* parent = nullptr);

    void load();
    void apply();
    void reset();
    [[nodiscard]] QString validationError() const;

    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

private:
    [[nodiscard]] AdvancedNode* appendNode(AdvancedNode* parent, AdvancedItemType type, QString label);
    [[nodiscard]] AdvancedNode* categoryNode(const QStringList& path);

    void addDescriptor(const AdvancedSettingDescriptor& descriptor);
    void applyNode(const AdvancedNode* node);
    void resetNode(AdvancedNode* node);

    [[nodiscard]] QString validationErrorForNode(const AdvancedNode* node) const;
    static QVariant normaliseValue(const AdvancedSettingDescriptor& descriptor, const QVariant& value);

    void emitRadioSiblingsChanged(const AdvancedNode* node);

    AdvancedSettingsRegistry* m_registry;
    std::vector<std::unique_ptr<AdvancedNode>> m_nodes;
};
} // namespace Fooyin
