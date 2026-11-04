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

#include <gui/widgets/expandedtreeview.h>

namespace Fooyin {
class ExpansionTrackDelegate;
struct InlineTrackEditorHit;

namespace Filters {
class ExpansionTrackView : public ExpandedTreeView
{
    Q_OBJECT

public:
    explicit ExpansionTrackView(QWidget* parent = nullptr);

    void setPreviewMode();
    void reset() override;

Q_SIGNALS:
    void closeRequested();
    void trackActivated(const QModelIndex& index);
    void trackMiddleClicked(const QModelIndex& index);
    void selectionUpdated();
    void trackFocused(const QRect& rect);
    void requestContextMenu(const QPoint& pos, bool hasSelection);
    void editorClicked(const QModelIndex& index, const Fooyin::InlineTrackEditorHit& hit, const QPoint& pos);

protected:
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    bool viewportEvent(QEvent* event) override;

private:
    [[nodiscard]] InlineTrackEditorHit editorAt(const QModelIndex& index, const QPoint& pos) const;
    void activateTrack(const QModelIndex& index);
    void clearEditorHover();

    ExpansionTrackDelegate* m_delegate;
    bool m_previewMode;
};
} // namespace Filters
} // namespace Fooyin
