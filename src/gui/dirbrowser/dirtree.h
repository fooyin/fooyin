/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <QByteArray>
#include <QTreeView>

namespace Fooyin {
class AutoHeaderView;

class DirTree : public QTreeView
{
    Q_OBJECT

public:
    explicit DirTree(QWidget* parent = nullptr);

    void setModel(QAbstractItemModel* model) override;

    void initialiseHeader();
    void resizeView();

    [[nodiscard]] bool showHeader() const;
    void setShowHeader(bool show);

    [[nodiscard]] QByteArray saveHeaderState() const;
    void restoreHeaderState(const QByteArray& state);
    void preserveHeaderState();

Q_SIGNALS:
    void backClicked();
    void forwardClicked();
    void headerVisibilityChanged(bool visible);
    void middleClicked();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void restoreHeaderAfterModelReset();
    void setColumnVisible(int column, bool visible);
    void showHeaderContextMenu(const QPoint& pos);

    AutoHeaderView* m_header;
    QByteArray m_pendingHeaderState;
};
} // namespace Fooyin
