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

#include "artworkviewerdialog.h"

#include <algorithm>

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDialog>
#include <QEvent>
#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPixmap>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace Fooyin {
class ArtworkView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit ArtworkView(QWidget* parent = nullptr);

    void setPixmap(const QPixmap& pixmap);

    void setFitToWindowEnabled(bool enabled);
    void fitToWindow();
    void actualSize();
    void zoomIn();
    void zoomOut();

signals:
    void fitToWindowChanged(bool enabled);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    [[nodiscard]] double fitScaleFactor() const;
    void updateFitToWindowState();
    void applyManualScale(qreal factor);
    void applyFitToWindow();

    QGraphicsScene m_scene;
    QGraphicsPixmapItem* m_item;
    bool m_fitToWindowEnabled{true};
};

ArtworkView::ArtworkView(QWidget* parent)
    : QGraphicsView{parent}
    , m_item(new QGraphicsPixmapItem())
{
    setScene(&m_scene);
    m_scene.addItem(m_item);

    setDragMode(ScrollHandDrag);
    setTransformationAnchor(AnchorUnderMouse);
    setResizeAnchor(AnchorViewCenter);
    setAlignment(Qt::AlignCenter);
    setFrameShape(NoFrame);
    setBackgroundRole(QPalette::Base);
    setRenderHint(QPainter::SmoothPixmapTransform, true);
}

void ArtworkView::setPixmap(const QPixmap& pixmap)
{
    m_item->setPixmap(pixmap);

    m_scene.setSceneRect(m_item->boundingRect());

    resetTransform();
    fitToWindow();
}

void ArtworkView::setFitToWindowEnabled(const bool enabled)
{
    m_fitToWindowEnabled = enabled;

    if(m_fitToWindowEnabled) {
        applyFitToWindow();
        return;
    }

    updateFitToWindowState();
}

void ArtworkView::fitToWindow()
{
    if(m_item->pixmap().isNull()) {
        return;
    }

    m_fitToWindowEnabled = true;
    applyFitToWindow();
}

void ArtworkView::actualSize()
{
    m_fitToWindowEnabled = false;

    resetTransform();

    updateFitToWindowState();
}

void ArtworkView::zoomIn()
{
    applyManualScale(1.25);
}

void ArtworkView::zoomOut()
{
    applyManualScale(0.8);
}

void ArtworkView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);

    if(m_fitToWindowEnabled) {
        applyFitToWindow();
    }
}

void ArtworkView::wheelEvent(QWheelEvent* event)
{
    if(!(event->modifiers() & Qt::ControlModifier)) {
        QGraphicsView::wheelEvent(event);
        return;
    }

    static constexpr double ZoomFactor = 1.25;

    const double factor = event->angleDelta().y() > 0 ? ZoomFactor : (1.0 / ZoomFactor);

    applyManualScale(factor);
    event->accept();
}

void ArtworkView::applyManualScale(const qreal factor)
{
    m_fitToWindowEnabled = false;
    scale(factor, factor);

    updateFitToWindowState();
}

void ArtworkView::applyFitToWindow()
{
    if(m_item->pixmap().isNull()) {
        return;
    }

    const auto oldHorizontalPolicy = horizontalScrollBarPolicy();
    const auto oldVerticalPolicy   = verticalScrollBarPolicy();

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    resetTransform();
    setSceneRect(m_item->boundingRect());
    fitInView(m_item, Qt::KeepAspectRatio);

    setHorizontalScrollBarPolicy(oldHorizontalPolicy);
    setVerticalScrollBarPolicy(oldVerticalPolicy);

    updateFitToWindowState();
}

double ArtworkView::fitScaleFactor() const
{
    if(m_item->pixmap().isNull()) {
        return 1.0;
    }

    const QSize itemSize = m_item->boundingRect().size().toSize();
    QSize viewportSize   = viewport()->size();

    if(horizontalScrollBar()->isVisible()) {
        viewportSize.rheight() += horizontalScrollBar()->sizeHint().height();
    }
    if(verticalScrollBar()->isVisible()) {
        viewportSize.rwidth() += verticalScrollBar()->sizeHint().width();
    }

    if(!itemSize.isValid() || !viewportSize.isValid() || itemSize.width() == 0 || itemSize.height() == 0) {
        return 1.0;
    }

    const auto widthScale  = static_cast<double>(viewportSize.width()) / static_cast<double>(itemSize.width());
    const auto heightScale = static_cast<double>(viewportSize.height()) / static_cast<double>(itemSize.height());

    return std::min(widthScale, heightScale);
}

void ArtworkView::updateFitToWindowState()
{
    const double currentScale = transform().m11();
    const double fitScale     = fitScaleFactor();
    const double tolerance    = 0.001 * std::max({1.0, std::abs(currentScale), std::abs(fitScale)});

    m_fitToWindowEnabled = std::abs(currentScale - fitScale) <= tolerance;

    emit fitToWindowChanged(m_fitToWindowEnabled);
}

ArtworkViewerDialog::ArtworkViewerDialog(const Track& track, const QPixmap& cover, QWidget* parent)
    : QDialog(parent)
    , m_view(new ArtworkView(this))
    , m_fitToWindowAction(new QAction(tr("Fit to window"), this))
    , m_actualSizeAction(new QAction(tr("Actual size"), this))
    , m_zoomInAction(new QAction(tr("Zoom in"), this))
    , m_zoomOutAction(new QAction(tr("Zoom out"), this))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(track.isValid() ? tr("Artwork: %1").arg(track.effectiveTitle()) : tr("Artwork"));

    m_fitToWindowAction->setCheckable(true);
    m_fitToWindowAction->setChecked(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    QObject::connect(m_fitToWindowAction, &QAction::toggled, m_view, &ArtworkView::setFitToWindowEnabled);
    QObject::connect(m_actualSizeAction, &QAction::triggered, m_view, &ArtworkView::actualSize);
    QObject::connect(m_zoomInAction, &QAction::triggered, m_view, &ArtworkView::zoomIn);
    QObject::connect(m_zoomOutAction, &QAction::triggered, m_view, &ArtworkView::zoomOut);

    QObject::connect(m_view, &ArtworkView::fitToWindowChanged, this, [this](bool enabled) {
        const QSignalBlocker blocker{m_fitToWindowAction};
        m_fitToWindowAction->setChecked(enabled);
    });

    setCover(cover);

    resize(ArtworkViewerDialog::sizeHint());
}

QSize ArtworkViewerDialog::sizeHint() const
{
    static constexpr double MaxScreenFraction = 0.8;

    if(m_cover.isNull()) {
        return {600, 600};
    }

    const QSize coverSize    = m_cover.size();
    const auto* dialogScreen = screen() ? screen() : QApplication::primaryScreen();

    if(!dialogScreen) {
        return coverSize;
    }

    return coverSize.boundedTo(dialogScreen->availableGeometry().size() * MaxScreenFraction);
}

void ArtworkViewerDialog::setCover(const QPixmap& cover)
{
    if(cover.isNull()) {
        return;
    }

    m_cover = cover;
    m_view->setPixmap(cover);
}

void ArtworkViewerDialog::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(m_fitToWindowAction);
    menu->addAction(m_actualSizeAction);
    menu->addSeparator();
    menu->addAction(m_zoomInAction);
    menu->addAction(m_zoomOutAction);

    menu->popup(event->globalPos());
}
} // namespace Fooyin

#include "artworkviewerdialog.moc"
#include "moc_artworkviewerdialog.cpp"
