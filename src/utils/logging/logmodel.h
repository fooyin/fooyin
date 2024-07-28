/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <QAbstractListModel>
#include <QDateTime>

#include <deque>

namespace Fooyin {
struct ConsoleEntry
{
    QDateTime time;
    QtMsgType type;
    QString message;
};

class LogModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Column : int
    {
        Level = 0,
        Time,
        Message
    };
    Q_ENUM(Column)

    explicit LogModel(QObject* parent = nullptr);

    void addEntry(ConsoleEntry entry);
    void clear();

    [[nodiscard]] std::deque<ConsoleEntry> entries();

    void setMaxEntries(int maxEntries);
    [[nodiscard]] int getMaxEntries() const;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;

private:
    std::deque<ConsoleEntry> m_items;
    int m_maxEntries;
};
} // namespace Fooyin
