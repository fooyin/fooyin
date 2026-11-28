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

#include "appearanceproperty.h"

#include <utils/treeitem.h>
#include <utils/treemodel.h>

#include <QJsonValue>
#include <QPalette>

#include <deque>

namespace Fooyin {
struct AppearancePropertyNode : TreeItem<AppearancePropertyNode>
{
    AppearanceProperty property;
    AppearancePropertyNode* page{nullptr};
};

class AppearancePropertyModel : public TreeModel<AppearancePropertyNode>
{
    Q_OBJECT

public:
    enum Role
    {
        ResetRole = Qt::UserRole,
        InheritedRole,
        CanInheritRole,
        PinRole,
        ColourPaletteRole,
        OverrideValueRole,
        PropertyRole,
    };

    explicit AppearancePropertyModel(QObject* parent);

    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    [[nodiscard]] const AppearanceProperty& property(const QModelIndex& index) const;
    QModelIndex addPage();
    AppearanceProperty* addProperty(const QModelIndex& page, AppearanceProperty property);

    void setOverrides(const QModelIndex& page, const QJsonObject& overrides);
    void resetOverrides(const QModelIndex& page);

    void setColourPalette(const QPalette& palette);

Q_SIGNALS:
    void propertiesEdited(const QModelIndex& page, const std::vector<Fooyin::AppearancePropertyEdit>& edits);

private:
    QString summary(const AppearancePropertyNode* item) const;

    void clearOverrides(AppearancePropertyNode* item);
    void updateAncestors(AppearancePropertyNode* item);

    void notifyChanges(AppearancePropertyNode* parent = nullptr);

    QPalette m_colourPalette;
    // Stable addresses keep property builders and persistent view indices valid as rows are added.
    std::deque<AppearancePropertyNode> m_nodes;
};
} // namespace Fooyin
