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

#include <gui/scripting/richtext.h>

#include <QListView>
#include <QMargins>
#include <QPersistentModelIndex>

class QColor;
class QPainter;

namespace Fooyin {
class SettingsManager;

namespace Lyrics {
class LyricsDelegate;
class LyricsModel;

class LyricsView : public QListView
{
    Q_OBJECT

public:
    explicit LyricsView(QWidget* parent = nullptr);

    void setDisplayAlignment(Qt::Alignment alignment);
    void setEdgeFadeEnabled(bool enabled);
    void setEdgeFadeSizePercent(int percent);
    void setDisplayMargins(const QMargins& margins);
    void setDisplayString(const RichText& string);

    [[nodiscard]] bool isDragSeeking() const;

Q_SIGNALS:
    void dragSeekingChanged(bool active);
    void lineClicked(const QModelIndex& index, const QPoint& pos);
    void lineDragSeekRequested(const QModelIndex& index, const QPoint& pos);
    void userScrolling();
    void viewportResized();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void updateGeometries() override;
    void wheelEvent(QWheelEvent* event) override;

private:
    [[nodiscard]] QPoint seekPosition(const QPoint& pos) const;
    [[nodiscard]] QModelIndex seekableIndexAt(const QPoint& pos) const;
    [[nodiscard]] bool isSeekableIndex(const QModelIndex& index) const;
    [[nodiscard]] QColor backgroundColour() const;
    [[nodiscard]] int edgeFadeHeight() const;
    [[nodiscard]] int visiblePaddingHeight(bool top) const;

    void updateScrollSingleStep();
    void paintEdgeFade(QPainter& painter) const;
    void updateDragPreview(const QPoint& pos);
    void clearDragPreview();

    RichText m_displayString;
    Qt::Alignment m_displayAlignment;
    QMargins m_displayMargins;

    QPoint m_pressPos;
    QPoint m_dragPos;
    QPersistentModelIndex m_dragIndex;
    bool m_edgeFadeEnabled;
    int m_edgeFadeSizePercent;
    bool m_leftButtonDown;
    bool m_dragSeeking;
    bool m_suppressContextMenu;
    bool m_suppressNextLeftRelease;
};
} // namespace Lyrics
} // namespace Fooyin
