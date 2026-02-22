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

#include <core/engine/enginedefs.h>
#include <utils/id.h>

#include <QAbstractTableModel>

namespace Fooyin {
class DspModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Role : uint16_t
    {
        Id = Qt::UserRole,
        HasSettings,
        Dsp
    };

    explicit DspModel(QObject* parent = nullptr);

    void setAllowInternalMoves(bool allow);

    void setup(const Engine::DspChain& dsps);
    [[nodiscard]] Engine::DspChain dsps() const;

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QStringList mimeTypes() const override;
    [[nodiscard]] QMimeData* mimeData(const QModelIndexList& indexes) const override;
    [[nodiscard]] bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                       const QModelIndex& parent) const override;
    [[nodiscard]] Qt::DropActions supportedDropActions() const override;
    [[nodiscard]] Qt::DropActions supportedDragActions() const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                      const QModelIndex& parent) override;
    bool removeRows(int row, int count, const QModelIndex& parent) override;

private:
    [[nodiscard]] bool isInternalDrag(const QMimeData* data) const;
    [[nodiscard]] std::optional<Engine::DspDefinition> dspForIndex(const QModelIndex& index) const;

    UId m_id;
    bool m_allowInternalMoves;

    Engine::DspChain m_dsps;
};
} // namespace Fooyin
