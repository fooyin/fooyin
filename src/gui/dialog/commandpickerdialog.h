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
 */

#pragma once

#include <QDialog>

class QIcon;
class QLineEdit;
class QSortFilterProxyModel;
class QStandardItemModel;
class QTreeView;

namespace Fooyin {
class ActionManager;

class CommandPickerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CommandPickerDialog(ActionManager* actionManager, QWidget* parent = nullptr);

    [[nodiscard]] QString commandId() const;
    void setCommandId(const QString& commandId);

    static QString displayText(ActionManager* actionManager, const QString& commandId);
    static QIcon displayIcon(ActionManager* actionManager, const QString& commandId);

private:
    enum Role
    {
        CommandIdRole = Qt::UserRole,
        SearchRole,
    };

    void populate();
    void restoreSelection();
    void currentChanged(const QModelIndex& current);
    void activateCurrent(const QModelIndex& index);

    ActionManager* m_actionManager;

    QLineEdit* m_filter;
    QTreeView* m_commands;
    QLineEdit* m_commandId;
    QStandardItemModel* m_model;
    QSortFilterProxyModel* m_proxy;
};
} // namespace Fooyin
