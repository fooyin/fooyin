/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <luket@pm.me>
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

#include <gui/fylayout.h>
#include <utils/treemodel.h>
#include <utils/treestatusitem.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QMargins>

#include <memory>
#include <vector>

namespace Fooyin {
class WidgetProvider;

class LayoutItem : public TreeStatusItem<LayoutItem>
{
public:
    enum Role
    {
        IsContainer = Qt::UserRole,
        WidgetKey
    };

    LayoutItem();
    explicit LayoutItem(QString key, QJsonObject data, LayoutItem* parent);

    [[nodiscard]] QString name() const;
    [[nodiscard]] QString key() const;
    [[nodiscard]] QJsonObject data() const;
    void setData(QJsonObject data);

    [[nodiscard]] bool isContainer() const;

private:
    QString m_key;
    QJsonObject m_data;
};

class LayoutTreeModel : public TreeModel<LayoutItem>
{
    Q_OBJECT

public:
    explicit LayoutTreeModel(WidgetProvider* widgetProvider, QObject* parent = nullptr);

    void populate(const FyLayout& layout);

    void remove(const QModelIndex& index);
    void moveUp(const QModelIndex& index);
    void moveDown(const QModelIndex& index);
    [[nodiscard]] bool canMoveUp(const QModelIndex& index) const;
    [[nodiscard]] bool canMoveDown(const QModelIndex& index) const;
    [[nodiscard]] bool canRemove(const QModelIndex& index) const;
    [[nodiscard]] bool canCut(const QModelIndex& index) const;
    [[nodiscard]] bool canCopy(const QModelIndex& index) const;
    [[nodiscard]] bool canPasteAfter(const QModelIndex& index) const;
    [[nodiscard]] bool isDummy(const QModelIndex& index) const;
    [[nodiscard]] QJsonObject copyItem(const QModelIndex& index) const;
    void pasteAfter(const QModelIndex& index, const QJsonObject& item);

    [[nodiscard]] FyLayout layout() const;
    [[nodiscard]] bool hasConfigurableMargins(const QModelIndex& index) const;
    [[nodiscard]] bool hasCustomMargins(const QModelIndex& index) const;
    [[nodiscard]] QMargins margins(const QModelIndex& index) const;
    void setMargins(const QModelIndex& index, const QMargins& margins);
    void clearMargins(const QModelIndex& index);
    [[nodiscard]] bool hasConfigurableSplitterSpacing(const QModelIndex& index) const;
    [[nodiscard]] bool hasCustomSplitterSpacing(const QModelIndex& index) const;
    [[nodiscard]] int splitterSpacing(const QModelIndex& index) const;
    void setSplitterSpacing(const QModelIndex& index, int spacing);
    void clearSplitterSpacing(const QModelIndex& index);

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

private:
    LayoutItem* appendItem(LayoutItem* parent, const QString& key, const QJsonObject& data);
    LayoutItem* insertItem(LayoutItem* parent, int row, const QString& key, const QJsonObject& data);
    void populateChildren(LayoutItem* parent, const QJsonArray& children);
    void populateChildren(LayoutItem* parent, const QJsonArray& children, LayoutItem::ItemStatus status);

    QJsonObject m_layoutJson;
    QString m_layoutName;
    WidgetProvider* m_widgetProvider;
    std::vector<std::unique_ptr<LayoutItem>> m_nodes;
};
} // namespace Fooyin
