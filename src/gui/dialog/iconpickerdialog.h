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
#include <QIcon>
#include <QStringList>

class QLabel;
class QLineEdit;
class QSortFilterProxyModel;
class QStandardItemModel;

namespace Fooyin {
class ExpandedTreeView;

struct IconSelection
{
    QString themeName;
    QString customPath;
};

class IconPickerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IconPickerDialog(const QIcon& commandIcon, QWidget* parent = nullptr);

    [[nodiscard]] IconSelection selection() const;
    void setSelection(const IconSelection& selection);

private:
    enum Role
    {
        ThemeNameRole = Qt::UserRole,
        DefaultIconRole,
    };

    static QString imageFilter();

    void currentChanged(const QModelIndex& current);
    void activateCurrent(const QModelIndex& index);
    void browseForImage();
    void updateCurrentLabel();

    ExpandedTreeView* m_icons;
    QLineEdit* m_filter;
    QLabel* m_currentLabel;
    QStandardItemModel* m_model;
    QSortFilterProxyModel* m_proxy;
    IconSelection m_selection;
};
} // namespace Fooyin
