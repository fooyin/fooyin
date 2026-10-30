/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <luket@pm.me>
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

#include "playlistview.h"

#include "playlistdelegate.h"
#include "playlistmodel.h"

#include <gui/widgets/widgetbackground.h>
#include <utils/heartdelegate.h>
#include <utils/hearteditor.h>
#include <utils/stardelegate.h>
#include <utils/stareditor.h>

#include <QAbstractItemDelegate>
#include <QApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScrollBar>

#include <algorithm>

using namespace Qt::StringLiterals;

constexpr auto MultipleValuesPrefix = "<<multiple values>>"_L1;

namespace {
bool sameRow(const QModelIndex& lhs, const QModelIndex& rhs)
{
    return lhs.isValid() && rhs.isValid() && lhs.row() == rhs.row() && lhs.parent() == rhs.parent();
}

QString normaliseBulkEditorValue(QString value)
{
    if(value.startsWith(QLatin1String{MultipleValuesPrefix})) {
        value.remove(0, QLatin1StringView{MultipleValuesPrefix}.size());
    }

    return value.trimmed();
}
} // namespace

namespace Fooyin {
PlaylistView::PlaylistView(QWidget* parent)
    : ExpandedTreeView{parent}
    , m_playlistLoaded{false}
    , m_stickyHeaders{false}
    , m_stickySubheaders{false}
    , m_stickyArtwork{false}
    , m_snappingScroll{false}
    , m_stickyHeaderPressed{false}
    , m_trackStatsEditable{true}
    , m_starDelegate{new StarDelegate(this)}
    , m_ratingColumn{-1}
    , m_heartDelegate{new HeartDelegate(this)}
    , m_lovedColumn{-1}
    , m_background{new WidgetBackground(viewport(), this)}
    , m_bulkEditor{nullptr}
    , m_inlineEditDelayMs{600}
    , m_bulkEditColumnIndex{-1}
    , m_cancelledPendingEditClick{false}
    , m_cancelledActiveEditorClick{false}
    , m_suppressNextClickEdit{false}
    , m_singleWriteInProgress{false}
    , m_bulkWriteInProgress{false}
{
    setObjectName(u"PlaylistView"_s);

    setSelectionBehavior(SelectRows);
    setSelectionMode(ExtendedSelection);
    setDragEnabled(true);
    setDragDropMode(DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    setDropIndicatorShown(true);
    setTextElideMode(Qt::ElideRight);
    setSelectBeforeDrag(true);
    setUniformHeightRole(PlaylistItem::UniformHeightKey);
    viewport()->setAcceptDrops(true);
    setSelectIgnoreParents(true);
}

void PlaylistView::setInlineEditDelay(int delayMs)
{
    m_inlineEditDelayMs = std::clamp(delayMs, 0, 10000);
}

void PlaylistView::setLoadingText(const QString& text)
{
    m_loadingText = text;
    viewport()->update();
}

void PlaylistView::setEmptyText(const QString& text)
{
    m_emptyText = text;
    viewport()->update();
}

void PlaylistView::setStickyHeaders(bool enabled)
{
    if(std::exchange(m_stickyHeaders, enabled) == enabled) {
        return;
    }

    updateGeometries();
    if(enabled) {
        snapScrollToStickyHeader(0);
    }
    viewport()->update();
}

void PlaylistView::setStickySubheaders(bool enabled)
{
    if(std::exchange(m_stickySubheaders, enabled) == enabled) {
        return;
    }

    updateGeometries();
    snapScrollToStickyHeader(0);
    viewport()->update();
}

void PlaylistView::setStickyArtwork(bool enabled)
{
    if(std::exchange(m_stickyArtwork, enabled) == enabled) {
        return;
    }

    viewport()->update();
}

void PlaylistView::setRatingColumn(int column)
{
    if(m_ratingColumn == column) {
        return;
    }

    m_starDelegate->setHoverIndex({});
    unsetCursor();
    viewport()->update();

    if(m_ratingColumn >= 0) {
        setItemDelegateForColumn(m_ratingColumn, nullptr);
    }

    m_ratingColumn = column;

    if(m_ratingColumn >= 0) {
        setItemDelegateForColumn(m_ratingColumn, m_starDelegate);
    }

    setMouseTracking(m_ratingColumn >= 0 || m_lovedColumn >= 0);
}

void PlaylistView::setLovedColumn(int column)
{
    if(m_lovedColumn == column) {
        return;
    }

    m_heartDelegate->setHoverIndex({});
    unsetCursor();
    viewport()->update();

    if(m_lovedColumn >= 0) {
        setItemDelegateForColumn(m_lovedColumn, nullptr);
    }

    m_lovedColumn = column;

    if(m_lovedColumn >= 0) {
        setItemDelegateForColumn(m_lovedColumn, m_heartDelegate);
    }

    setMouseTracking(m_ratingColumn >= 0 || m_lovedColumn >= 0);
}

void PlaylistView::setRatingEditorOnlyOnHover(bool enabled)
{
    m_starDelegate->setShowEmptyOnlyOnActiveRow(enabled);
    viewport()->update();
}

void PlaylistView::setLoveEditorOnlyOnHover(bool enabled)
{
    m_heartDelegate->setShowEmptyOnlyOnActiveRow(enabled);
    viewport()->update();
}

void PlaylistView::setTrackStatsEditable(bool editable)
{
    if(std::exchange(m_trackStatsEditable, editable) == editable) {
        return;
    }

    if(!editable) {
        ratingHoverOut();
        loveHoverOut();
    }
}

void PlaylistView::setBackgroundConfig(const BackgroundConfig& config)
{
    m_background->setConfig(config);
}

void PlaylistView::setBackgroundPixmap(const QPixmap& pixmap)
{
    m_background->setPixmap(pixmap);
}

void PlaylistView::playlistAboutToBeReset()
{
    m_playlistLoaded = false;
    cancelPendingEditor();
    endBulkEditSession(false);
    viewport()->update();
}

void PlaylistView::playlistReset()
{
    m_playlistLoaded = true;
    viewport()->update();
}

bool PlaylistView::playlistLoaded() const
{
    return m_playlistLoaded;
}

bool PlaylistView::stickyArtworkIntersects(const QRect& rect) const
{
    return m_stickyArtworkRegion.intersects(rect);
}

bool PlaylistView::hasActiveInlineEditor() const
{
    return state() == EditingState || (m_bulkEditor && m_bulkEditor->isVisible());
}

void PlaylistView::setSingleWriteInProgress(bool inProgress)
{
    m_singleWriteInProgress = inProgress;

    if(inProgress) {
        cancelPendingEditor();
    }
}

void PlaylistView::handleSingleWriteFinished()
{
    m_singleWriteInProgress = false;
}

void PlaylistView::setBulkWriteInProgress(bool inProgress)
{
    m_bulkWriteInProgress = inProgress;

    if(inProgress) {
        cancelPendingEditor();
    }
}

void PlaylistView::handleBulkWriteFinished()
{
    m_bulkWriteInProgress = false;
}

void PlaylistView::changeEvent(QEvent* event)
{
    ExpandedTreeView::changeEvent(event);

    switch(event->type()) {
        case QEvent::FontChange:
            Q_EMIT displayChanged();
            break;
        default:
            break;
    }
}

void PlaylistView::keyPressEvent(QKeyEvent* event)
{
    if(m_bulkWriteInProgress) {
        event->accept();
        return;
    }

    if(event->key() == Qt::Key_F2 && event->modifiers() == Qt::NoModifier && startBulkEditSession()) {
        event->accept();
        return;
    }

    ExpandedTreeView::keyPressEvent(event);
}

void PlaylistView::mouseMoveEvent(QMouseEvent* event)
{
    if(m_bulkWriteInProgress || m_stickyHeaderPressed) {
        event->accept();
        return;
    }

    const QModelIndex index = indexAt(event->pos());
    updateHoveredRow(index);

    if(m_starDelegate) {
        if(index.isValid() && index.column() == m_ratingColumn) {
            ratingHoverIn(index, event->pos());
        }
        else if(m_starDelegate->hoveredIndex().isValid()) {
            ratingHoverOut();
        }
    }

    if(m_heartDelegate) {
        if(index.isValid() && index.column() == m_lovedColumn) {
            loveHoverIn(index);
        }
        else if(m_heartDelegate->hoveredIndex().isValid()) {
            loveHoverOut();
        }
    }

    ExpandedTreeView::mouseMoveEvent(event);
}

void PlaylistView::mousePressEvent(QMouseEvent* event)
{
    m_stickyHeaderPressed = false;

    const QModelIndex index = indexAt(event->pos());

    if(m_bulkEditor && m_bulkEditor->isVisible() && !m_bulkEditor->geometry().contains(event->pos())) {
        m_suppressNextClickEdit = event->button() == Qt::LeftButton;
        endBulkEditSession(true);
    }

    m_cancelledActiveEditorClick = m_suppressNextClickEdit;
    m_suppressNextClickEdit      = false;
    m_cancelledPendingEditClick  = m_pendingEditIndex.isValid() && sameRow(index, m_pendingEditIndex);
    cancelPendingEditor();
    m_pressedSelectedIndex = QPersistentModelIndex{};

    if(m_bulkWriteInProgress) {
        event->accept();
        return;
    }

    if(event->button() == Qt::LeftButton) {
        for(const auto& pinnedHeader : stickyHeaderLayout()) {
            if(!stickyHeaderPaintRect(pinnedHeader).contains(event->pos())) {
                continue;
            }

            const QModelIndex group = pinnedHeader.index;
            m_stickyHeaderPressed   = true;

            QModelIndex firstTrack{group};
            while(model()->hasChildren(firstTrack)) {
                firstTrack = model()->index(0, 0, firstTrack);
            }
            if(firstTrack.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
                const QScopedValueRollback snapGuard{m_snappingScroll, true};
                setCurrentIndex(firstTrack);
                scrollTo(group, PositionAtTop);
                if(verticalScrollMode() == ScrollPerPixel) {
                    verticalScrollBar()->setValue(verticalScrollBar()->value() - pinnedHeader.clipRect.top());
                }
            }

            event->accept();
            return;
        }
    }

    if(event->button() != Qt::LeftButton) {
        ExpandedTreeView::mousePressEvent(event);
        return;
    }

    const int column = index.column();

    if(editTriggers() != NoEditTriggers && index.isValid() && column != m_ratingColumn && column != m_lovedColumn
       && (index.flags() & Qt::ItemIsEditable) != 0 && selectionModel() && selectionModel()->isSelected(index)) {
        m_pressedSelectedIndex = index;
    }

    if(!m_trackStatsEditable || !index.isValid() || (column != m_ratingColumn && column != m_lovedColumn)) {
        ExpandedTreeView::mousePressEvent(event);
        return;
    }

    if(column == m_ratingColumn) {
        const auto starRating = index.data().value<StarRating>();
        const auto align      = static_cast<Qt::Alignment>(index.data(Qt::TextAlignmentRole).toInt());
        const auto rating     = StarEditor::ratingAtPosition(
            event->pos(), StarDelegate::contentRect(visualRect(index), viewport()), starRating, align);

        auto modelRating{starRating};
        modelRating.setRating(rating);
        editTrackStat(index, QVariant::fromValue(modelRating), Track::Stat::Rating);
    }
    else if(column == m_lovedColumn) {
        auto heartValue = index.data().value<HeartValue>();
        heartValue.setLoved(!heartValue.loved());
        editTrackStat(index, QVariant::fromValue(heartValue), Track::Stat::Loved);
    }

    ExpandedTreeView::mousePressEvent(event);
}

void PlaylistView::mouseReleaseEvent(QMouseEvent* event)
{
    if(std::exchange(m_stickyHeaderPressed, false)) {
        event->accept();
        return;
    }

    if(m_bulkWriteInProgress) {
        event->accept();
        return;
    }

    const QModelIndex index = indexAt(event->pos());

    ExpandedTreeView::mouseReleaseEvent(event);

    if(event->button() == Qt::LeftButton && !m_cancelledPendingEditClick && !m_cancelledActiveEditorClick
       && m_pressedSelectedIndex.isValid() && index == m_pressedSelectedIndex) {
        queueEditor(index);
    }

    m_pressedSelectedIndex       = QPersistentModelIndex{};
    m_cancelledPendingEditClick  = false;
    m_cancelledActiveEditorClick = false;
}

void PlaylistView::resizeEvent(QResizeEvent* event)
{
    ExpandedTreeView::resizeEvent(event);

    updateBulkEditorGeometry();
}

void PlaylistView::scrollContentsBy(int dx, int dy)
{
    ExpandedTreeView::scrollContentsBy(dx, dy);
    if(dy != 0) {
        snapScrollToStickyHeader(dy);
    }
    updateBulkEditorGeometry();
}

void PlaylistView::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_editTimer.timerId()) {
        m_editTimer.stop();

        const QPersistentModelIndex pendingIndex = m_pendingEditIndex;
        m_pendingEditIndex                       = QPersistentModelIndex{};
        reopenEditor(pendingIndex);
        return;
    }

    ExpandedTreeView::timerEvent(event);
}

void PlaylistView::closeEditor(QWidget* editor, QAbstractItemDelegate::EndEditHint hint)
{
    const bool closingFromLeftClick = (QApplication::mouseButtons() & Qt::LeftButton) != 0;
    m_suppressNextClickEdit         = closingFromLeftClick;

    ExpandedTreeView::closeEditor(editor, hint);
}

void PlaylistView::leaveEvent(QEvent* event)
{
    updateHoveredRow({});

    if(m_starDelegate && m_starDelegate->hoveredIndex().isValid()) {
        ratingHoverOut();
    }
    if(m_heartDelegate && m_heartDelegate->hoveredIndex().isValid()) {
        loveHoverOut();
    }

    ExpandedTreeView::leaveEvent(event);
}

void PlaylistView::paintEvent(QPaintEvent* event)
{
    m_stickyArtworkRegion = QRegion{};

    QPainter painter{viewport()};

    const auto drawCentreText = [this, &painter](const QString& text) {
        drawBackground(painter);

        if(!text.isEmpty()) {
            QRect textRect = painter.fontMetrics().boundingRect(text);
            textRect.moveCenter(viewport()->rect().center());
            painter.drawText(textRect, Qt::AlignCenter, text);
        }
    };

    if(const auto* playlistModel = qobject_cast<PlaylistModel*>(model())) {
        if(playlistModel->haveTracks()) {
            if(playlistModel->shouldShowLoadingText()) {
                drawCentreText(m_loadingText);
            }
            else {
                drawBackground(painter);
                painter.end();
                ExpandedTreeView::paintEvent(event);
                painter.begin(viewport());
                drawStickyHeader(painter);
            }
        }
        else if(!m_playlistLoaded) {
            drawCentreText(m_loadingText);
        }
        else {
            drawCentreText(m_emptyText);
        }
    }
}

bool PlaylistView::eventFilter(QObject* watched, QEvent* event)
{
    if(!m_bulkEditor || watched != m_bulkEditor) {
        return ExpandedTreeView::eventFilter(watched, event);
    }

    if(event->type() == QEvent::KeyPress) {
        const auto* keyEvent = static_cast<QKeyEvent*>(event);

        switch(keyEvent->key()) {
            case Qt::Key_Tab:
                if((keyEvent->modifiers() & ~Qt::ShiftModifier) == Qt::NoModifier) {
                    advanceBulkEditColumn((keyEvent->modifiers() & Qt::ShiftModifier) != 0 ? -1 : 1);
                    return true;
                }
                break;
            case Qt::Key_Backtab:
                advanceBulkEditColumn(-1);
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                endBulkEditSession(true);
                return true;
            case Qt::Key_Escape:
                endBulkEditSession(false);
                return true;
            default:
                break;
        }
    }
    else if(event->type() == QEvent::FocusOut && m_bulkEditor && m_bulkEditor->isVisible()
            && !m_bulkEditor->hasFocus()) {
        endBulkEditSession(true);
    }

    return ExpandedTreeView::eventFilter(watched, event);
}

QAbstractItemView::DropIndicatorPosition PlaylistView::dropPosition(const QPoint& pos, const QRect& rect,
                                                                    const QModelIndex& index)
{
    DropIndicatorPosition dropPos{OnViewport};
    const int midpoint = static_cast<int>(std::round(static_cast<double>(rect.height()) / 2));
    const auto type    = index.data(PlaylistItem::Type).toInt();

    if(type == PlaylistItem::Subheader) {
        dropPos = OnItem;
    }
    else if(pos.y() - rect.top() < midpoint || type == PlaylistItem::Header) {
        dropPos = AboveItem;
    }
    else if(rect.bottom() - pos.y() < midpoint) {
        dropPos = BelowItem;
    }

    return dropPos;
}

QRect PlaylistView::drawSpan(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if(!m_stickyArtwork) {
        return ExpandedTreeView::drawSpan(painter, option, index);
    }

    const auto* playlistModel = qobject_cast<PlaylistModel*>(model());
    if(!playlistModel) {
        return ExpandedTreeView::drawSpan(painter, option, index);
    }

    const QModelIndex first   = playlistModel->firstImageColumnTrackIndex(index);
    const QModelIndex artwork = first.siblingAtColumn(index.column());
    if(!artwork.isValid()) {
        return ExpandedTreeView::drawSpan(painter, option, index);
    }

    const QModelIndex group = playlistModel->imageColumnGroupIndex(first);
    int topInset{0};
    if(extendSpansIntoParents()) {
        QModelIndex topLevel{first};
        while(topLevel.parent().isValid()) {
            topLevel = topLevel.parent();
        }
        QModelIndex firstLeaf{topLevel};
        while(firstLeaf.isValid() && model()->rowCount(firstLeaf) > 0) {
            firstLeaf = model()->index(0, 0, firstLeaf);
        }
        if(sameRow(first, firstLeaf)) {
            for(QModelIndex parent = first.parent(); parent.parent().isValid(); parent = parent.parent()) {
                topInset += visualRect(parent.siblingAtColumn(index.column())).height();
            }
        }
    }

    QStyleOptionViewItem opt{option};
    const int height
        = std::max(spanHeight(artwork), itemDelegateForIndex(artwork)->sizeHint(opt, artwork).height()) + topInset;
    const int naturalTop = visualRect(artwork).top() - topInset;

    int pinnedTop{0};
    QModelIndex headerIndex{first.parent()};
    while(headerIndex.isValid() && headerIndex.data(PlaylistItem::Type).toInt() != PlaylistItem::Header) {
        headerIndex = headerIndex.parent();
    }
    if(m_stickyHeaders) {
        // Keep the artwork below the pinned main header when both options are enabled
        pinnedTop = std::max(0, stickyHeaderRect(headerIndex).bottom() + 1);
    }

    if(naturalTop >= pinnedTop) {
        m_stickyArtworkRegion += option.rect.intersected(viewport()->rect());
        return ExpandedTreeView::drawSpan(painter, option, index);
    }

    QModelIndex last{group};
    while(model()->rowCount(last) > 0) {
        last = model()->index(model()->rowCount(last) - 1, 0, last);
    }

    const int groupBottom = visualRect(last.siblingAtColumn(index.column())).bottom() + 1;
    const int top         = std::max(naturalTop, std::min(pinnedTop, groupBottom - height));
    opt.rect              = {opt.rect.x(), top, opt.rect.width(), height};

    // Spans are painted before rows, so subheaders can use the current artwork bounds when laying out their text.
    m_stickyArtworkRegion += opt.rect.intersected(viewport()->rect());

    painter->save();
    painter->setClipRect(opt.rect.intersected(viewport()->rect()), Qt::IntersectClip);
    itemDelegateForIndex(artwork)->paint(painter, opt, artwork);
    painter->restore();

    return opt.rect;
}

int PlaylistView::spanHeight(const QModelIndex& index) const
{
    const int width = ExpandedTreeView::spanHeight(index);
    if(!index.isValid() || width <= 0) {
        return width;
    }

    const auto* delegate = qobject_cast<PlaylistDelegate*>(itemDelegate());
    if(!delegate) {
        return width;
    }

    QModelIndex track = index.siblingAtColumn(0);
    while(track.isValid() && model()->rowCount(track) > 0) {
        track = model()->index(0, 0, track);
    }
    return delegate->artworkSpanHeight(width, track.siblingAtColumn(index.column()));
}

int PlaylistView::adjustVerticalScrollMaximum(int maximum) const
{
    if(!m_stickyHeaders || !model() || viewMode() != ViewMode::Tree || verticalScrollMode() != ScrollPerPixel
       || maximum <= 0) {
        return maximum;
    }

    const int scrollOffset = maximum - verticalScrollBar()->value();
    const auto headers     = stickyHeaderLayout(scrollOffset);
    if(headers.empty()) {
        return maximum;
    }

    const int boundary = headers.back().rect.bottom() + 1;
    if(boundary >= viewport()->height()) {
        return maximum;
    }

    const QModelIndex index = findIndexAt({0, scrollOffset + boundary}, true, true);
    if(index.data(PlaylistItem::Type).toInt() != PlaylistItem::Track) {
        return maximum;
    }

    int column{0};
    while(column < header()->count() && (header()->isSectionHidden(column) || isSpanning(column))) {
        ++column;
    }
    if(column == header()->count()) {
        return maximum;
    }

    const QRect rowRect = visualRect(index.siblingAtColumn(column)).translated(0, -scrollOffset);
    if(rowRect.top() >= boundary || rowRect.bottom() < boundary) {
        return maximum;
    }

    return maximum + rowRect.bottom() + 1 - boundary;
}

QModelIndexList PlaylistView::selectedTrackRows() const
{
    if(!selectionModel()) {
        return {};
    }

    QModelIndexList rows;

    const auto selected = selectionModel()->selectedRows();
    for(const QModelIndex& index : selected) {
        if(index.isValid() && index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
            rows.push_back(index);
        }
    }

    std::ranges::sort(rows, [](const QModelIndex& lhs, const QModelIndex& rhs) {
        return lhs.data(PlaylistItem::Index).toInt() < rhs.data(PlaylistItem::Index).toInt();
    });

    return rows;
}

void PlaylistView::editTrackStat(const QModelIndex& index, const QVariant& value, Track::Stat stat)
{
    QModelIndexList indexes;

    if(selectedIndexes().contains(index)) {
        const auto selectedRows = selectedTrackRows();
        for(const QModelIndex& selectedIndex : selectedRows) {
            const QModelIndex statIndex = selectedIndex.siblingAtColumn(index.column());
            if(statIndex.isValid()) {
                indexes.push_back(statIndex);
            }
        }
    }
    else {
        indexes.push_back(index);
    }

    if(index.flags().testFlag(Qt::ItemIsEditable)) {
        auto* playlistModel = qobject_cast<PlaylistModel*>(model());
        if(playlistModel && indexes.size() > 1) {
            const auto bulkEdit = playlistModel->setBulkData(indexes, value);
            if(bulkEdit.has_value()) {
                emitTrackStatChanged(bulkEdit->tracks, stat);
                return;
            }
        }

        for(const QModelIndex& statIndex : std::as_const(indexes)) {
            model()->setData(statIndex, value, Qt::EditRole);
        }
        return;
    }

    TrackList tracks;
    for(const QModelIndex& statIndex : std::as_const(indexes)) {
        auto track = statIndex.data(PlaylistItem::PersistentItemData).value<PlaylistTrack>().track;
        if(stat == Track::Stat::Rating) {
            track.setRating(value.value<StarRating>().rating());
        }
        else if(stat == Track::Stat::Loved) {
            track.setLoved(value.value<HeartValue>().loved());
        }
        tracks.push_back(track);
    }
    emitTrackStatChanged(tracks, stat);
}

void PlaylistView::emitTrackStatChanged(const TrackList& tracks, Track::Stat stat)
{
    if(stat == Track::Stat::Rating) {
        Q_EMIT tracksRated(tracks);
    }
    else if(stat == Track::Stat::Loved) {
        Q_EMIT tracksLoved(tracks);
    }
}

std::vector<int> PlaylistView::bulkEditableColumns() const
{
    if(!header()) {
        return {};
    }

    const QModelIndexList rows = selectedTrackRows();
    if(rows.empty()) {
        return {};
    }

    const QModelIndex& anchorRow = rows.front();

    std::vector<int> columns;

    for(int visualIndex{0}; visualIndex < header()->count(); ++visualIndex) {
        const int logicalIndex = header()->logicalIndex(visualIndex);

        if(logicalIndex < 0 || header()->isSectionHidden(logicalIndex) || logicalIndex == m_ratingColumn
           || logicalIndex == m_lovedColumn) {
            continue;
        }

        const QModelIndex probeIndex = anchorRow.siblingAtColumn(logicalIndex);
        if(probeIndex.isValid() && (model()->flags(probeIndex) & Qt::ItemIsEditable) != 0) {
            columns.push_back(logicalIndex);
        }
    }

    return columns;
}

QModelIndex PlaylistView::bulkEditAnchorIndex(int column) const
{
    if(m_bulkEditRows.empty()) {
        return {};
    }

    return m_bulkEditRows.front().siblingAtColumn(column);
}

QRect PlaylistView::bulkEditRect(int column) const
{
    if(m_bulkEditRows.empty() || !header() || header()->isSectionHidden(column)) {
        return {};
    }

    const int x     = header()->sectionViewportPosition(column);
    const int width = header()->sectionSize(column);
    if(x < 0 || width <= 0) {
        return {};
    }

    QRect rowsRect;

    for(const QModelIndex& rowIndex : m_bulkEditRows) {
        const QModelIndex index = rowIndex.siblingAtColumn(column);
        if(!index.isValid()) {
            continue;
        }

        const QRect visualIndexRect = visualRect(index);
        if(!visualIndexRect.isValid()) {
            continue;
        }

        rowsRect = rowsRect.isNull() ? visualIndexRect : rowsRect.united(visualIndexRect);
    }

    if(!rowsRect.isValid()) {
        return {};
    }

    return QRect{x, rowsRect.top(), width, rowsRect.height()}.adjusted(0, 0, -1, -1);
}

QString PlaylistView::bulkEditValueForColumn(int column, bool& mixedValues) const
{
    mixedValues = false;

    QString currentValue;
    QStringList distinctValues;

    for(const QModelIndex& rowIndex : m_bulkEditRows) {
        const QModelIndex index = rowIndex.siblingAtColumn(column);
        if(!index.isValid()) {
            continue;
        }

        const QString value = index.data(Qt::EditRole).toString();
        if(!distinctValues.contains(value)) {
            distinctValues.push_back(value);
        }

        if(distinctValues.size() == 1) {
            currentValue = value;
            continue;
        }

        mixedValues = true;
    }

    if(mixedValues) {
        QStringList nonEmptyValues{distinctValues};
        nonEmptyValues.removeAll(QString{});
        return QString{MultipleValuesPrefix} + u' ' + nonEmptyValues.join("; "_L1);
    }

    return currentValue;
}

bool PlaylistView::startBulkEditSession()
{
    if(m_bulkWriteInProgress || editTriggers() == NoEditTriggers || state() == EditingState) {
        return false;
    }

    const QModelIndexList rows = selectedTrackRows();
    if(rows.size() < 2) {
        return false;
    }

    const std::vector<int> columns = bulkEditableColumns();
    if(columns.empty()) {
        return false;
    }

    cancelPendingEditor();

    if(!m_bulkEditor) {
        m_bulkEditor = new QLineEdit(viewport());
        m_bulkEditor->hide();
        m_bulkEditor->installEventFilter(this);
    }

    m_bulkEditRows        = rows;
    m_bulkEditColumns     = columns;
    m_bulkEditColumnIndex = 0;

    showBulkEditor();
    return true;
}

bool PlaylistView::commitBulkEdit(int columnIndex)
{
    const int resolvedColumnIndex = columnIndex >= 0 ? columnIndex : m_bulkEditColumnIndex;
    if(!m_bulkEditor || !m_bulkEditor->isModified() || m_bulkEditRows.empty() || resolvedColumnIndex < 0
       || std::cmp_greater_equal(resolvedColumnIndex, m_bulkEditColumns.size())) {
        return false;
    }

    auto* playlistModel = qobject_cast<PlaylistModel*>(model());
    if(!playlistModel) {
        return false;
    }

    QModelIndexList indexes;
    const int column = m_bulkEditColumns.at(resolvedColumnIndex);

    for(const QModelIndex& rowIndex : std::as_const(m_bulkEditRows)) {
        const QModelIndex index = rowIndex.siblingAtColumn(column);
        if(index.isValid()) {
            indexes.push_back(index);
        }
    }

    const auto bulkEdit = playlistModel->setBulkData(indexes, normaliseBulkEditorValue(m_bulkEditor->text()));
    if(!bulkEdit.has_value()) {
        return false;
    }

    if(bulkEdit->loveField) {
        Q_EMIT tracksLoved(bulkEdit->tracks);
    }
    else if(bulkEdit->ratingField) {
        Q_EMIT tracksRated(bulkEdit->tracks);
    }
    else {
        Q_EMIT bulkWriteRequested(bulkEdit->tracks);
    }
    return true;
}

void PlaylistView::advanceBulkEditColumn(int offset)
{
    if(m_bulkEditColumns.empty() || m_bulkEditColumnIndex < 0) {
        return;
    }

    const int count           = static_cast<int>(m_bulkEditColumns.size());
    const int nextColumnIndex = (m_bulkEditColumnIndex + offset + count) % count;

    if(!m_bulkEditor || !m_bulkEditor->isModified()) {
        m_bulkEditColumnIndex = nextColumnIndex;
        showBulkEditor();
        return;
    }

    if(m_bulkEditor) {
        m_bulkEditor->hide();
    }

    if(commitBulkEdit(m_bulkEditColumnIndex)) {
        return;
    }

    m_bulkEditColumnIndex = nextColumnIndex;
    showBulkEditor();
}

void PlaylistView::centreRectInView(const QRect& rect)
{
    if(!rect.isValid()) {
        return;
    }

    const QPoint viewportCentre = viewport()->rect().center();

    if(auto* hBar = horizontalScrollBar()) {
        hBar->setValue(hBar->value() + rect.center().x() - viewportCentre.x());
    }

    if(auto* vBar = verticalScrollBar()) {
        vBar->setValue(vBar->value() + rect.center().y() - viewportCentre.y());
    }
}

void PlaylistView::endBulkEditSession(bool commitChanges)
{
    if(m_bulkEditor) {
        m_bulkEditor->hide();
    }

    if(commitChanges) {
        commitBulkEdit();
    }

    if(m_bulkEditor) {
        m_bulkEditor->clear();
        m_bulkEditor->setModified(false);
    }

    m_bulkEditRows.clear();
    m_bulkEditColumns.clear();
    m_bulkEditColumnIndex = -1;
}

void PlaylistView::showBulkEditor()
{
    if(!m_bulkEditor || m_bulkEditRows.empty() || m_bulkEditColumnIndex < 0
       || std::cmp_greater_equal(m_bulkEditColumnIndex, m_bulkEditColumns.size())) {
        return;
    }

    const int column        = m_bulkEditColumns.at(m_bulkEditColumnIndex);
    const QModelIndex index = bulkEditAnchorIndex(column);
    if(!index.isValid()) {
        endBulkEditSession(false);
        return;
    }

    setCurrentIndex(index);

    bool mixedValues{false};
    const QString value = bulkEditValueForColumn(column, mixedValues);

    m_bulkEditor->blockSignals(true);
    m_bulkEditor->setText(value);
    m_bulkEditor->setModified(false);
    m_bulkEditor->blockSignals(false);

    m_bulkEditor->show();
    m_bulkEditor->raise();
    updateBulkEditorGeometry();

    centreRectInView(m_bulkEditor->geometry());
    m_bulkEditor->setFocus();
    m_bulkEditor->selectAll();
}

void PlaylistView::updateBulkEditorGeometry()
{
    if(!m_bulkEditor || !m_bulkEditor->isVisible() || m_bulkEditColumnIndex < 0
       || std::cmp_greater_equal(m_bulkEditColumnIndex, m_bulkEditColumns.size())) {
        return;
    }

    const QModelIndex index = bulkEditAnchorIndex(m_bulkEditColumns.at(m_bulkEditColumnIndex));
    if(!index.isValid()) {
        m_bulkEditor->hide();
        return;
    }

    const QRect editorRect = bulkEditRect(m_bulkEditColumns.at(m_bulkEditColumnIndex));
    if(!editorRect.isValid()) {
        m_bulkEditor->hide();
        return;
    }

    m_bulkEditor->setGeometry(editorRect);
}

void PlaylistView::queueEditor(const QModelIndex& index)
{
    if(m_bulkWriteInProgress || (m_bulkEditor && m_bulkEditor->isVisible()) || editTriggers() == NoEditTriggers
       || !index.isValid() || index.column() == m_ratingColumn || index.column() == m_lovedColumn
       || (model()->flags(index) & Qt::ItemIsEditable) == 0 || state() == EditingState) {
        return;
    }

    m_pendingEditIndex = index;
    m_editTimer.start(m_inlineEditDelayMs, this);
}

void PlaylistView::cancelPendingEditor()
{
    m_editTimer.stop();
    m_pendingEditIndex = QPersistentModelIndex{};
}

void PlaylistView::reopenEditor(const QModelIndex& index)
{
    if(m_bulkWriteInProgress || (m_bulkEditor && m_bulkEditor->isVisible()) || editTriggers() == NoEditTriggers
       || !index.isValid() || index.column() == m_ratingColumn || index.column() == m_lovedColumn
       || (model()->flags(index) & Qt::ItemIsEditable) == 0) {
        return;
    }

    setCurrentIndex(index);

    if(state() != EditingState && currentIndex() == index && selectionModel() && selectionModel()->isSelected(index)) {
        edit(index);
    }
}

void PlaylistView::ratingHoverIn(const QModelIndex& index, const QPoint& pos)
{
    if(!m_trackStatsEditable) {
        return;
    }

    const QModelIndexList selected = selectedIndexes();
    const QModelIndex prevIndex    = m_starDelegate->hoveredIndex();
    m_starDelegate->setHoverIndex(index, pos, selected);
    setCursor(Qt::PointingHandCursor);

    update(prevIndex);
    update(index);

    for(const QModelIndex& selectedIndex : selected) {
        if(selectedIndex.column() == m_ratingColumn) {
            update(selectedIndex);
        }
    }
}

void PlaylistView::ratingHoverOut()
{
    const QModelIndex prevIndex = m_starDelegate->hoveredIndex();
    m_starDelegate->setHoverIndex({});
    setCursor({});

    update(prevIndex);

    const QModelIndexList selected = selectedIndexes();
    for(const QModelIndex& selectedIndex : selected) {
        if(selectedIndex.column() == m_ratingColumn) {
            update(selectedIndex);
        }
    }
}

void PlaylistView::loveHoverIn(const QModelIndex& index)
{
    if(!m_trackStatsEditable) {
        return;
    }

    const QModelIndexList selected = selectedIndexes();
    const QModelIndex prevIndex    = m_heartDelegate->hoveredIndex();
    m_heartDelegate->setHoverIndex(index, selected);
    setCursor(Qt::PointingHandCursor);

    update(prevIndex);
    update(index);

    for(const QModelIndex& selectedIndex : selected) {
        if(selectedIndex.column() == m_lovedColumn) {
            update(selectedIndex);
        }
    }
}

void PlaylistView::loveHoverOut()
{
    const QModelIndex prevIndex = m_heartDelegate->hoveredIndex();
    m_heartDelegate->setHoverIndex({});
    setCursor({});

    update(prevIndex);

    const QModelIndexList selected = selectedIndexes();
    for(const QModelIndex& selectedIndex : selected) {
        if(selectedIndex.column() == m_lovedColumn) {
            update(selectedIndex);
        }
    }
}

void PlaylistView::updateHoveredRow(const QModelIndex& index)
{
    const QModelIndex hoveredRow = index.isValid() ? index.siblingAtColumn(0) : QModelIndex{};
    if(m_hoveredRow == hoveredRow) {
        return;
    }

    const QModelIndex previousRow = std::exchange(m_hoveredRow, hoveredRow);

    m_starDelegate->setHoveredRow(hoveredRow);
    m_heartDelegate->setHoveredRow(hoveredRow);

    const auto updateEditorCells = [this](const QModelIndex& row) {
        if(!row.isValid()) {
            return;
        }
        if(m_ratingColumn >= 0) {
            update(row.siblingAtColumn(m_ratingColumn));
        }
        if(m_lovedColumn >= 0) {
            update(row.siblingAtColumn(m_lovedColumn));
        }
    };

    updateEditorCells(previousRow);
    updateEditorCells(hoveredRow);
}

QModelIndex PlaylistView::stickyHeaderIndex(int scrollOffset) const
{
    if(!m_stickyHeaders || !model() || viewMode() != ViewMode::Tree) {
        return {};
    }

    QModelIndex group = findIndexAt({0, scrollOffset}, true, true);
    while(group.isValid() && group.data(PlaylistItem::Type).toInt() != PlaylistItem::Header) {
        group = group.parent();
    }
    return group.siblingAtColumn(0);
}

QRect PlaylistView::stickyHeaderRect(const QModelIndex& group, int scrollOffset, int top) const
{
    if(!group.isValid()) {
        return {};
    }

    int column{0};
    while(column < header()->count() && header()->isSectionHidden(column)) {
        ++column;
    }
    if(column == header()->count()) {
        return {};
    }

    const QRect rect = visualRect(group.siblingAtColumn(column)).translated(0, -scrollOffset);
    if(rect.height() <= 0 || rect.top() >= top) {
        return {};
    }

    // A subgroup also ends when one of its ancestors gives way to its next sibling.
    QModelIndex ancestor{group};
    while(ancestor.isValid()) {
        const QModelIndex nextGroup = model()->index(ancestor.row() + 1, column, ancestor.parent());
        if(nextGroup.isValid()) {
            const QRect nextRect = visualRect(nextGroup).translated(0, -scrollOffset);
            if(nextRect.isValid()) {
                top = std::min(top, nextRect.top() - rect.height());
            }
            break;
        }
        ancestor = ancestor.parent();
    }

    return {0, top, header()->length(), rect.height()};
}

std::vector<PlaylistView::StickyHeader> PlaylistView::stickyHeaderLayout(int scrollOffset) const
{
    std::vector<StickyHeader> headers;
    QModelIndex group = stickyHeaderIndex(scrollOffset);
    QRect rect        = stickyHeaderRect(group, scrollOffset);
    if(rect.isEmpty()) {
        return headers;
    }

    headers.push_back({group, rect, rect.intersected(viewport()->rect())});

    int top = std::max(0, rect.bottom() + 1);
    while(m_stickySubheaders && top < viewport()->height()) {
        QModelIndex child = findIndexAt({0, scrollOffset + top}, true, true);
        while(child.isValid() && child.parent() != group) {
            child = child.parent();
        }
        if(!child.isValid() || child.data(PlaylistItem::Type).toInt() != PlaylistItem::Subheader) {
            break;
        }

        rect                 = stickyHeaderRect(child, scrollOffset, top);
        const QRect clipRect = rect.intersected({0, top, header()->length(), viewport()->height() - top});
        if(clipRect.isEmpty()) {
            break;
        }

        headers.push_back({child, rect, clipRect});
        group = child;
        top   = rect.bottom() + 1;
    }

    return headers;
}

QRect PlaylistView::stickyHeaderPaintRect(const StickyHeader& header) const
{
    if(header.index.data(PlaylistItem::Type).toInt() != PlaylistItem::Subheader) {
        return header.clipRect;
    }

    QStyleOptionViewItem opt;
    initViewItemOption(&opt);
    opt.rect = header.rect;
    return PlaylistDelegate::subheaderContentRect(opt, header.index).intersected(header.clipRect);
}

void PlaylistView::drawStickyHeader(QPainter& painter) const
{
    for(const auto& pinnedHeader : stickyHeaderLayout()) {
        QStyleOptionViewItem opt;
        initViewItemOption(&opt);
        opt.rect = pinnedHeader.rect;
        opt.state.setFlag(QStyle::State_Selected, selectionModel()->isSelected(pinnedHeader.index));
        opt.state.setFlag(QStyle::State_MouseOver, false);

        painter.save();

        painter.setClipRect(stickyHeaderPaintRect(pinnedHeader), Qt::IntersectClip);
        painter.fillRect(opt.rect, opt.palette.brush(QPalette::Base));
        style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &opt, &painter, this);
        itemDelegate()->paint(&painter, opt, pinnedHeader.index);

        painter.restore();
    }
}

void PlaylistView::snapScrollToStickyHeader(int delta)
{
    if(m_snappingScroll || verticalScrollMode() != ScrollPerPixel) {
        return;
    }

    const auto headers = stickyHeaderLayout();
    if(headers.empty()) {
        return;
    }

    const int boundary = headers.back().rect.bottom() + 1;
    if(boundary >= viewport()->height()) {
        return;
    }

    int column{0};
    while(column < header()->count() && (header()->isSectionHidden(column) || isSpanning(column))) {
        ++column;
    }
    if(column == header()->count()) {
        return;
    }

    const auto indexes = visibleIndexes();
    for(const auto& index : indexes) {
        if(index.data(PlaylistItem::Type).toInt() != PlaylistItem::Track) {
            continue;
        }

        const QRect rowRect = visualRect(index.siblingAtColumn(column));
        if(rowRect.top() >= boundary || rowRect.bottom() < boundary) {
            continue;
        }

        const int above = rowRect.top() - boundary;
        const int below = rowRect.bottom() + 1 - boundary;

        int adjustment = below < -above ? below : above;
        if(delta < 0) {
            adjustment = below;
        }
        else if(delta > 0) {
            adjustment = above;
        }

        auto* scrollbar  = verticalScrollBar();
        const int target = std::max(scrollbar->minimum(), scrollbar->value() + adjustment);

        const QScopedValueRollback snapGuard{m_snappingScroll, true};
        scrollbar->setValue(target);
        return;
    }
}

void PlaylistView::drawBackground(QPainter& painter)
{
    painter.fillRect(viewport()->rect(), palette().brush(QPalette::Base));
    m_background->draw(painter);
}

} // namespace Fooyin

#include "moc_playlistview.cpp"
