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

#include "playlistview.h"

#include "playlistmodel.h"
#include "widgets/pixmapfadecontroller.h"

#include <utils/stardelegate.h>
#include <utils/stareditor.h>

#include <QApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>

#include <algorithm>

using namespace Qt::StringLiterals;

constexpr auto EditorDelay          = 600;
constexpr auto MultipleValuesPrefix = "<<multiple values>>"_L1;

QT_BEGIN_NAMESPACE
// Exported by qimageeffects.cpp
void qt_blurImage(QImage& blurImage, qreal radius, bool quality, int transposed = 0);
QT_END_NAMESPACE

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

QPixmap blurredPixmap(const QPixmap& pixmap, qreal radius)
{
    if(pixmap.isNull() || radius <= 0) {
        return pixmap;
    }

    QImage blurred = pixmap.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    qt_blurImage(blurred, radius, true, false);
    return QPixmap::fromImage(blurred);
}
} // namespace

namespace Fooyin {
PlaylistView::PlaylistView(QWidget* parent)
    : ExpandedTreeView{parent}
    , m_playlistLoaded{false}
    , m_starDelegate{nullptr}
    , m_ratingColumn{-1}
    , m_bgFadeController{new PixmapFadeController(this)}
    , m_bulkEditor{nullptr}
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

    m_bgFadeController->setUpdateCallback([this] { viewport()->update(); });
}

void PlaylistView::setEmptyText(const QString& text)
{
    m_emptyText = text;
    viewport()->update();
}

void PlaylistView::setLoadingText(const QString& text)
{
    m_loadingText = text;
    viewport()->update();
}

void PlaylistView::setupRatingDelegate()
{
    const int columnCount = header()->count();
    for(int column{0}; column < columnCount; ++column) {
        if(auto* starDelegate = qobject_cast<StarDelegate*>(itemDelegateForColumn(column))) {
            m_starDelegate = starDelegate;
            m_ratingColumn = column;
            setMouseTracking(true);
            return;
        }
    }

    m_starDelegate = nullptr;
    m_ratingColumn = -1;
    setMouseTracking(false);
}

void PlaylistView::setBackgroundOptions(const BackgroundOptions& options)
{
    m_bgOptions = options;
    m_bgFadeController->setDurationMs(m_bgOptions.fadeDurationMs);
    m_bgFadeController->setEnabled(m_bgOptions.fadeChanges);
    updateBackgroundTransparency();

    invalidateScaledBackground();
    viewport()->update();
}

void PlaylistView::setBackgroundPixmap(const QPixmap& pixmap)
{
    m_bgFadeController->setPixmap(pixmap);
    updateBackgroundTransparency();
    invalidateScaledBackground();
    viewport()->update();
}

void PlaylistView::playlistAboutToBeReset()
{
    m_playlistLoaded = false;
    cancelPendingEditor();
    endBulkEditSession(false);
}

void PlaylistView::playlistReset()
{
    m_playlistLoaded = true;
}

bool PlaylistView::playlistLoaded() const
{
    return m_playlistLoaded;
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
        case QEvent::StyleChange:
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
    if(m_bulkWriteInProgress) {
        event->accept();
        return;
    }

    if(m_starDelegate) {
        const QModelIndex index = indexAt(event->pos());
        if(index.isValid() && index.column() == m_ratingColumn) {
            ratingHoverIn(index, event->pos());
        }
        else if(m_starDelegate->hoveredIndex().isValid()) {
            ratingHoverOut();
        }
    }

    ExpandedTreeView::mouseMoveEvent(event);
}

void PlaylistView::mousePressEvent(QMouseEvent* event)
{
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

    if(editTriggers() & NoEditTriggers || event->button() != Qt::LeftButton) {
        ExpandedTreeView::mousePressEvent(event);
        return;
    }

    if(index.isValid() && index.column() != m_ratingColumn && (index.flags() & Qt::ItemIsEditable) != 0
       && selectionModel() && selectionModel()->isSelected(index)) {
        m_pressedSelectedIndex = index;
    }

    if(!index.isValid() || index.column() != m_ratingColumn) {
        ExpandedTreeView::mousePressEvent(event);
        return;
    }

    const auto starRating = index.data().value<StarRating>();
    const auto align      = static_cast<Qt::Alignment>(index.data(Qt::TextAlignmentRole).toInt());
    const auto rating     = StarEditor::ratingAtPosition(event->pos(), visualRect(index), starRating, align);

    if(index.flags().testFlag(Qt::ItemIsEditable)) {
        const auto setRating = [this, rating](const QModelIndex& modelIndex) {
            auto modelRating = modelIndex.data().value<StarRating>();
            modelRating.setRating(rating);
            model()->setData(modelIndex, QVariant::fromValue(modelRating), Qt::EditRole);
        };

        if(selectedIndexes().contains(index)) {
            const QModelIndexList selected = selectionModel()->selectedRows();
            QModelIndexList ratingIndexes;

            for(const QModelIndex& selectedIndex : selected) {
                if(selectedIndex.data(PlaylistItem::Type).toInt() != PlaylistItem::Track) {
                    continue;
                }

                const QModelIndex ratingIndex = selectedIndex.siblingAtColumn(m_ratingColumn);
                if(ratingIndex.isValid()) {
                    ratingIndexes.push_back(ratingIndex);
                }
            }

            auto* playlistModel = qobject_cast<PlaylistModel*>(model());
            if(playlistModel && ratingIndexes.size() > 1) {
                auto modelRating = index.data().value<StarRating>();
                modelRating.setRating(rating);

                const auto bulkEdit = playlistModel->setBulkData(ratingIndexes, QVariant::fromValue(modelRating));
                if(bulkEdit.has_value()) {
                    Q_EMIT bulkWriteRequested(*bulkEdit);
                }
                else {
                    for(const QModelIndex& ratingIndex : std::as_const(ratingIndexes)) {
                        setRating(ratingIndex);
                    }
                }
            }
            else {
                for(const QModelIndex& ratingIndex : std::as_const(ratingIndexes)) {
                    setRating(ratingIndex);
                }
            }
        }
        else {
            setRating(index);
        }
    }
    else if(selectedIndexes().contains(index)) {
        TrackList tracks;
        const QModelIndexList selected = selectionModel()->selectedRows();
        for(const QModelIndex& selectedIndex : selected) {
            if(selectedIndex.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
                auto track = selectedIndex.data(PlaylistItem::ItemData).value<PlaylistTrack>().track;
                track.setRating(rating);
                tracks.push_back(track);
            }
        }
        Q_EMIT tracksRated(tracks);
    }
    else {
        auto track = index.data(PlaylistItem::ItemData).value<PlaylistTrack>().track;
        track.setRating(rating);
        Q_EMIT tracksRated({track});
    }

    ExpandedTreeView::mousePressEvent(event);
}

void PlaylistView::mouseReleaseEvent(QMouseEvent* event)
{
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
    invalidateScaledBackground();
    updateBulkEditorGeometry();
}

void PlaylistView::scrollContentsBy(int dx, int dy)
{
    ExpandedTreeView::scrollContentsBy(dx, dy);
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
    if(m_starDelegate && m_starDelegate->hoveredIndex().isValid()) {
        ratingHoverOut();
    }

    ExpandedTreeView::leaveEvent(event);
}

void PlaylistView::paintEvent(QPaintEvent* event)
{
    QPainter painter{viewport()};

    const auto drawCentreText = [this, &painter](const QString& text) {
        drawBackground(painter);

        if(!text.isEmpty()) {
            QRect textRect = painter.fontMetrics().boundingRect(text);
            textRect.moveCenter(viewport()->rect().center());
            painter.drawText(textRect, Qt::AlignCenter, text);
        }
    };

    if(auto* playlistModel = qobject_cast<PlaylistModel*>(model())) {
        if(playlistModel->haveTracks()) {
            if(playlistModel->shouldShowLoadingText()) {
                drawCentreText(m_loadingText);
            }
            else {
                drawBackground(painter);
                ExpandedTreeView::paintEvent(event);
            }
        }
        else {
            drawCentreText(m_emptyText);
        }
    }
}

void PlaylistView::drawBackground(QPainter& painter)
{
    painter.fillRect(viewport()->rect(), palette().brush(QPalette::Base));

    const QPixmap& currentPixmap = m_bgFadeController->pixmap();
    if(m_bgOptions.imageMode == PlaylistBgImage::None || currentPixmap.isNull() || m_bgOptions.opacity <= 0) {
        return;
    }

    const QPixmap pixmap = preparedBackgroundPixmap(currentPixmap, m_cachedBg);
    if(pixmap.isNull()) {
        return;
    }

    painter.save();

    const double opacity = std::clamp(m_bgOptions.opacity, 0, 100) / 100.0;

    if(m_bgFadeController->progress() < 1.0) {
        const QPixmap& prevPixmap = m_bgFadeController->previousPixmap();
        const QPixmap prev        = preparedBackgroundPixmap(prevPixmap, m_cachedPreviousBg);

        if(!prev.isNull()) {
            painter.setOpacity(opacity * (1.0 - m_bgFadeController->progress()));
            drawBackgroundPixmap(painter, prev);
        }
        painter.setOpacity(opacity * m_bgFadeController->progress());
    }
    else {
        painter.setOpacity(opacity);
    }

    drawBackgroundPixmap(painter, pixmap);

    painter.restore();
}

void PlaylistView::updateBackgroundTransparency()
{
    const bool transparentRows = m_bgOptions.imageMode != PlaylistBgImage::None && m_bgOptions.opacity > 0
                              && !m_bgFadeController->pixmap().isNull();
    setProperty("transparent_base_rows", transparentRows);
}

void PlaylistView::invalidateScaledBackground()
{
    m_cachedBg         = {};
    m_cachedPreviousBg = {};
}

QPixmap PlaylistView::preparedBackgroundPixmap(const QPixmap& source, BackgroundPixmapCache& cache) const
{
    const QSize viewportSize = viewport()->size();
    if(viewportSize.isEmpty() || source.isNull()) {
        return {};
    }

    if(cache.sourceKey == source.cacheKey() && cache.viewportSize == viewportSize && !cache.pixmap.isNull()) {
        return cache.pixmap;
    }

    QSize targetSize = source.size();

    switch(m_bgOptions.scaling) {
        case PlaylistBgScaling::ScaledAndCropped:
            targetSize.scale(viewportSize, Qt::KeepAspectRatioByExpanding);
            break;
        case PlaylistBgScaling::Scaled:
            targetSize = viewportSize;
            break;
        case PlaylistBgScaling::ScaledKeepProportions:
            targetSize.scale(viewportSize, Qt::KeepAspectRatio);
            break;
        case PlaylistBgScaling::OriginalSize:
            if(m_bgOptions.maxSize > 0) {
                const QSize maxSize{m_bgOptions.maxSize, m_bgOptions.maxSize};
                if(targetSize.width() > maxSize.width() || targetSize.height() > maxSize.height()) {
                    targetSize.scale(maxSize, Qt::KeepAspectRatio);
                }
            }
            break;
    }

    if(targetSize.isEmpty()) {
        return {};
    }

    cache.sourceKey    = source.cacheKey();
    cache.viewportSize = viewportSize;
    cache.pixmap       = source.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    cache.pixmap       = blurredPixmap(cache.pixmap, m_bgOptions.blur);

    return cache.pixmap;
}

QPoint PlaylistView::backgroundPixmapPosition(const QSize& pixmapSize) const
{
    const QSize viewportSize = viewport()->size();

    if(m_bgOptions.scaling != PlaylistBgScaling::OriginalSize) {
        return {(viewportSize.width() - pixmapSize.width()) / 2, (viewportSize.height() - pixmapSize.height()) / 2};
    }

    int x{0};
    int y{0};

    switch(m_bgOptions.position) {
        case PlaylistBgImagePosition::TopLeft:
        case PlaylistBgImagePosition::Left:
        case PlaylistBgImagePosition::BottomLeft:
            x = 0;
            break;
        case PlaylistBgImagePosition::Top:
        case PlaylistBgImagePosition::Middle:
        case PlaylistBgImagePosition::Bottom:
            x = (viewportSize.width() - pixmapSize.width()) / 2;
            break;
        case PlaylistBgImagePosition::TopRight:
        case PlaylistBgImagePosition::Right:
        case PlaylistBgImagePosition::BottomRight:
            x = viewportSize.width() - pixmapSize.width();
            break;
    }

    switch(m_bgOptions.position) {
        case PlaylistBgImagePosition::TopLeft:
        case PlaylistBgImagePosition::Top:
        case PlaylistBgImagePosition::TopRight:
            y = 0;
            break;
        case PlaylistBgImagePosition::Left:
        case PlaylistBgImagePosition::Middle:
        case PlaylistBgImagePosition::Right:
            y = (viewportSize.height() - pixmapSize.height()) / 2;
            break;
        case PlaylistBgImagePosition::BottomLeft:
        case PlaylistBgImagePosition::Bottom:
        case PlaylistBgImagePosition::BottomRight:
            y = viewportSize.height() - pixmapSize.height();
            break;
    }

    return {x, y};
}

void PlaylistView::drawBackgroundPixmap(QPainter& painter, const QPixmap& pixmap)
{
    painter.drawPixmap(backgroundPixmapPosition(pixmap.size()), pixmap);
}

QAbstractItemView::DropIndicatorPosition PlaylistView::dropPosition(const QPoint& pos, const QRect& rect,
                                                                    const QModelIndex& index)
{
    DropIndicatorPosition dropPos{OnViewport};
    const int midpoint = static_cast<int>(std::round(static_cast<double>((rect.height())) / 2));
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

bool PlaylistView::eventFilter(QObject* watched, QEvent* event)
{
    if(!m_bulkEditor || watched != m_bulkEditor) {
        return ExpandedTreeView::eventFilter(watched, event);
    }

    if(event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);

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
    else if(event->type() == QEvent::FocusOut && m_bulkEditor->isVisible()) {
        if(m_bulkEditor && m_bulkEditor->isVisible() && !m_bulkEditor->hasFocus()) {
            endBulkEditSession(true);
        }
    }

    return ExpandedTreeView::eventFilter(watched, event);
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

        if(logicalIndex < 0 || header()->isSectionHidden(logicalIndex) || logicalIndex == m_ratingColumn) {
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

    Q_EMIT bulkWriteRequested(*bulkEdit);
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
       || !index.isValid() || index.column() == m_ratingColumn || (model()->flags(index) & Qt::ItemIsEditable) == 0
       || state() == EditingState) {
        return;
    }

    m_pendingEditIndex = index;
    m_editTimer.start(EditorDelay, this);
}

void PlaylistView::cancelPendingEditor()
{
    m_editTimer.stop();
    m_pendingEditIndex = QPersistentModelIndex{};
}

void PlaylistView::reopenEditor(const QModelIndex& index)
{
    if(m_bulkWriteInProgress || (m_bulkEditor && m_bulkEditor->isVisible()) || editTriggers() == NoEditTriggers
       || !index.isValid() || index.column() == m_ratingColumn || (model()->flags(index) & Qt::ItemIsEditable) == 0) {
        return;
    }

    setCurrentIndex(index);

    if(state() != EditingState && currentIndex() == index && selectionModel() && selectionModel()->isSelected(index)) {
        edit(index);
    }
}

void PlaylistView::ratingHoverIn(const QModelIndex& index, const QPoint& pos)
{
    if(editTriggers() & NoEditTriggers) {
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
    if(editTriggers() & NoEditTriggers) {
        return;
    }

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
} // namespace Fooyin

#include "moc_playlistview.cpp"
