/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include <core/library/musiclibrary.h>
#include <core/track.h>
#include <gui/widgets/expandedtreeview.h>

#include <QBasicTimer>

class QKeyEvent;
class QLineEdit;
class QResizeEvent;
class QTimerEvent;
class QWidget;

namespace Fooyin {
class StarDelegate;

class PlaylistView : public ExpandedTreeView
{
    Q_OBJECT

public:
    explicit PlaylistView(QWidget* parent = nullptr);

    void setLoadingText(const QString& text);
    void setEmptyText(const QString& text);
    void setupRatingDelegate();

    void playlistAboutToBeReset();
    void playlistReset();
    [[nodiscard]] bool playlistLoaded() const;

    [[nodiscard]] bool hasActiveInlineEditor() const;
    void setSingleWriteInProgress(bool inProgress);
    void handleSingleWriteFinished();
    void setBulkWriteInProgress(bool inProgress);
    void handleBulkWriteFinished();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void timerEvent(QTimerEvent* event) override;
    void closeEditor(QWidget* editor, QAbstractItemDelegate::EndEditHint hint) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    DropIndicatorPosition dropPosition(const QPoint& pos, const QRect& rect, const QModelIndex& index) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void bulkWriteRequested(const Fooyin::TrackList& tracks);
    void tracksRated(const Fooyin::TrackList& tracks);

private:
    [[nodiscard]] QModelIndexList selectedTrackRows() const;
    [[nodiscard]] std::vector<int> bulkEditableColumns() const;
    [[nodiscard]] QModelIndex bulkEditAnchorIndex(int column) const;
    [[nodiscard]] QRect bulkEditRect(int column) const;
    [[nodiscard]] QString bulkEditValueForColumn(int column, bool& mixedValues) const;

    bool startBulkEditSession();
    bool commitBulkEdit(int columnIndex = -1);
    void advanceBulkEditColumn(int offset);
    void centreRectInView(const QRect& rect);
    void endBulkEditSession(bool commitChanges);
    void showBulkEditor();
    void updateBulkEditorGeometry();

    void queueEditor(const QModelIndex& index);
    void cancelPendingEditor();
    void reopenEditor(const QModelIndex& index);

    void ratingHoverIn(const QModelIndex& index, const QPoint& pos);
    void ratingHoverOut();

    QString m_emptyText;
    QString m_loadingText;
    bool m_playlistLoaded;

    StarDelegate* m_starDelegate;
    int m_ratingColumn;

    QPersistentModelIndex m_pressedSelectedIndex;
    QPersistentModelIndex m_pendingEditIndex;
    QLineEdit* m_bulkEditor;
    QBasicTimer m_editTimer;
    QModelIndexList m_bulkEditRows;
    std::vector<int> m_bulkEditColumns;
    int m_bulkEditColumnIndex;
    bool m_cancelledPendingEditClick;
    bool m_cancelledActiveEditorClick;
    bool m_suppressNextClickEdit;
    bool m_singleWriteInProgress;
    bool m_bulkWriteInProgress;
};
} // namespace Fooyin
