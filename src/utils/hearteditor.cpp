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

#include <utils/hearteditor.h>

#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
struct HeartBrushes
{
    QBrush filled;
    QBrush faded;
};

[[nodiscard]] HeartBrushes getHeartBrushes(const QPalette& palette, HeartValue::EditMode mode, bool selected)
{
    QBrush filled;

    if(mode == HeartValue::EditMode::Editable) {
        filled = selected ? palette.highlightedText() : palette.highlight();
    }
    else {
        filled = selected ? palette.highlightedText() : palette.text();
    }

    QBrush faded{filled};
    QColor fadedColour{filled.color()};
    fadedColour.setAlphaF(fadedColour.alphaF() * 0.2);
    faded.setColor(fadedColour);

    return {.filled = filled, .faded = faded};
}
} // namespace

HeartValue::HeartValue()
    : HeartValue{false, false}
{ }

HeartValue::HeartValue(bool loved)
    : HeartValue{loved, 15}
{ }

HeartValue::HeartValue(bool loved, int scale)
    : m_loved{loved}
    , m_scale{scale}
{
    m_heart.moveTo(0.5, 1.0);
    m_heart.lineTo(0.11, 0.56);
    m_heart.cubicTo(0.0, 0.44, 0.0, 0.25, 0.11, 0.12);
    m_heart.cubicTo(0.22, 0.0, 0.4, 0.02, 0.5, 0.21);
    m_heart.cubicTo(0.6, 0.02, 0.78, 0.0, 0.89, 0.12);
    m_heart.cubicTo(1.0, 0.25, 1.0, 0.44, 0.89, 0.56);
    m_heart.closeSubpath();
}

bool HeartValue::loved() const
{
    return m_loved;
}

int HeartValue::scale() const
{
    return m_scale;
}

void HeartValue::setLoved(bool loved)
{
    m_loved = loved;
}

void HeartValue::setScale(int scale)
{
    m_scale = scale;
}

void HeartValue::paint(QPainter* painter, const QRect& rect, const QPalette& palette, EditMode mode,
                       Qt::Alignment alignment, bool selected) const
{
    const auto brushes     = getHeartBrushes(palette, mode, selected);
    const qreal dpr        = painter->device()->devicePixelRatioF();
    const QString cacheKey = u"HeartValue:%1|%2|%3|%4|%5"_s.arg(m_loved)
                                 .arg(m_scale)
                                 .arg(mode == EditMode::Editable ? 1 : 0)
                                 .arg(rect.width())
                                 .arg(rect.height())
                           + u"|%1|%2|%3|%4"_s.arg(alignment.toInt())
                                 .arg(brushes.filled.color().name(QColor::HexArgb))
                                 .arg(brushes.faded.color().name(QColor::HexArgb))
                                 .arg(dpr);

    QPixmap pixmap;
    if(!QPixmapCache::find(cacheKey, &pixmap)) {
        pixmap = QPixmap{rect.size() * dpr};
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);

        QPainter pixmapPainter(&pixmap);
        pixmapPainter.setRenderHint(QPainter::Antialiasing, true);

        const int yOffset = (rect.height() - m_scale) / 2;

        int xOffset{0};
        const int totalWidth = 1 * m_scale;
        if(alignment & Qt::AlignHCenter) {
            xOffset = (rect.width() - totalWidth) / 2;
        }
        else if(alignment & Qt::AlignRight) {
            xOffset = rect.width() - totalWidth;
        }

        pixmapPainter.translate(xOffset, yOffset);
        pixmapPainter.scale(m_scale, m_scale);

        pixmapPainter.setPen(Qt::NoPen);
        pixmapPainter.setBrush(m_loved ? brushes.filled : brushes.faded);
        pixmapPainter.drawPath(m_heart);

        pixmapPainter.end();

        QPixmapCache::insert(cacheKey, pixmap);
    }

    painter->drawPixmap(rect.topLeft(), pixmap);
}

QSize HeartValue::sizeHint() const
{
    return m_scale * QSize{1, 1};
}

HeartEditor::HeartEditor(Qt::Alignment align, QWidget* parent)
    : QWidget{parent}
    , m_align{align}
{
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(true);
    setMouseTracking(true);
}

HeartValue HeartEditor::loved() const
{
    return m_loved;
}

void HeartEditor::setValue(const HeartValue& loved)
{
    m_loved        = loved;
    m_originaLoved = loved.loved();
}

QSize HeartEditor::sizeHint() const
{
    return m_loved.sizeHint();
}

void HeartEditor::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};
    m_loved.paint(&painter, rect(), palette(), HeartValue::EditMode::Editable);
}

void HeartEditor::contextMenuEvent(QContextMenuEvent* event)
{
    // Prevent showing parent widget context menu
    event->accept();
}

void HeartEditor::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::RightButton) {
        m_loved.setLoved(m_originaLoved);
        update();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void HeartEditor::mouseReleaseEvent(QMouseEvent* /*event*/)
{
    Q_EMIT editingFinished();
}

void HeartEditor::keyPressEvent(QKeyEvent* event)
{
    const int key = event->key();

    if(key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Escape) {
        Q_EMIT editingFinished();
    }
    if(key == Qt::Key_Space) {
        m_loved.setLoved(!m_loved.loved());
        event->accept();
    }
    else {
        QWidget::keyPressEvent(event);
    }
}
} // namespace Fooyin
