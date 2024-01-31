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

#pragma once

#include "fyutils_export.h"

#include <QTableView>

namespace Fooyin {
class ActionManager;
class Context;

class FYUTILS_EXPORT ExtendableTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit ExtendableTableModel(QObject* parent = nullptr)
        : QAbstractTableModel{parent}
    { }

    virtual void addPendingRow()    = 0;
    virtual void removePendingRow() = 0;

signals:
    void pendingRowCancelled();
};

class FYUTILS_EXPORT ExtendableTableView : public QTableView
{
    Q_OBJECT

public:
    enum Tool
    {
        None = 0x0,
        Move = 0x1,
    };
    Q_DECLARE_FLAGS(Tools, Tool)

    explicit ExtendableTableView(ActionManager* actionManager, QWidget* parent = nullptr);
    ExtendableTableView(ActionManager* actionManager, const Tools& tools, QWidget* parent = nullptr);
    ~ExtendableTableView() override;

    void setTools(const Tools& tools);
    void setToolButtonStyle(Qt::ToolButtonStyle style);
    void addCustomTool(QWidget* widget);

    void setExtendableModel(ExtendableTableModel* model);
    void setExtendableColumn(int column);

    [[nodiscard]] QAction* addAction() const;
    [[nodiscard]] QAction* removeAction() const;
    [[nodiscard]] QAction* moveUpAction() const;
    [[nodiscard]] QAction* moveDownAction() const;

    Context context() const;
    virtual void addContextActions(QMenu* menu);

    void scrollTo(const QModelIndex& index, ScrollHint hint = EnsureVisible) override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
