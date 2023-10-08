/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/track.h>

#include <utils/treeitem.h>
#include <utils/treemodel.h>

namespace Fy {
namespace Core::Player {
class PlayerManager;
}

namespace Gui::Widgets::Info {
class InfoItem : public Utils::TreeItem<InfoItem>
{
public:
    enum ItemType
    {
        Header = 0,
        Entry,
    };

    enum ValueType
    {
        Concat = 0,
        Average,
        Total,
        Max
    };

    enum Role
    {
        Type = Qt::UserRole,
        Value
    };

    using FormatFunc = std::function<QString(uint64_t)>;

    InfoItem();
    InfoItem(ItemType type, QString name, InfoItem* parent, ValueType valueType);
    InfoItem(ItemType type, QString name, InfoItem* parent, ValueType valueType, FormatFunc numFunc);

    [[nodiscard]] ItemType type() const;
    [[nodiscard]] QString name() const;
    [[nodiscard]] QVariant value() const;

    void addTrackValue(uint64_t value);
    void addTrackValue(int value);
    void addTrackValue(const QString& value);
    void addTrackValue(const QStringList& values);

private:
    ItemType m_type;
    ValueType m_valueType;

    QString m_name;
    std::vector<uint64_t> m_numValues;
    mutable uint64_t m_numValue;
    QStringList m_values;
    mutable QString m_value;

    FormatFunc m_formatNum;
};

class InfoModel : public Utils::TreeModel<InfoItem>
{
    Q_OBJECT

public:
    enum class ItemParent
    {
        Root,
        Metadata,
        Location,
        General
    };
    Q_ENUM(ItemParent)

    explicit InfoModel(Core::Player::PlayerManager* playerManager, QObject* parent = nullptr);
    ~InfoModel() override;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

    void resetModel(const Core::TrackList& tracks);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Gui::Widgets::Info
} // namespace Fy
