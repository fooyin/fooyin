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

#include <QFont>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QSharedData>

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
struct HeartBrushes
{
    QBrush filled;
    QBrush faded;
};

[[nodiscard]] HeartBrushes getHeartBrushes(const QPalette& palette, HeartValue::EditMode mode, bool selected,
                                           const QColor& lovedColour, const QColor& unlovedColour)
{
    QBrush filled;

    if(lovedColour.isValid()) {
        filled = lovedColour;
    }
    else if(mode == HeartValue::EditMode::Editable) {
        filled = selected ? palette.highlightedText() : palette.highlight();
    }
    else {
        filled = selected ? palette.highlightedText() : palette.text();
    }

    QBrush faded;
    if(unlovedColour.isValid()) {
        faded = unlovedColour;
    }
    else {
        QColor fadedColour{filled.color()};
        fadedColour.setAlphaF(fadedColour.alphaF() * 0.2);
        faded = fadedColour;
    }

    return {.filled = filled, .faded = faded};
}
} // namespace

class HeartValuePrivate : public QSharedData
{
public:
    HeartValuePrivate(bool loved, int scale, const QColor& lovedColour, const QColor& unlovedColour,
                      std::optional<LoveSymbols> symbols);

    void paint(QPainter* painter, const QRect& rect, const QPalette& palette, HeartValue::EditMode mode,
               Qt::Alignment alignment, bool selected) const;

    QPainterPath m_heart;
    bool m_loved;
    int m_scale;
    QColor m_lovedColour;
    QColor m_unlovedColour;
    std::optional<LoveSymbols> m_symbols;
    QFont m_symbolFont{};
};

HeartValuePrivate::HeartValuePrivate(bool loved, int scale, const QColor& lovedColour, const QColor& unlovedColour,
                                     std::optional<LoveSymbols> symbols)
    : m_loved{loved}
    , m_scale{scale}
    , m_lovedColour{lovedColour}
    , m_unlovedColour{unlovedColour}
    , m_symbols{std::move(symbols)}
{
    m_heart.moveTo(0.5, 1.0);
    m_heart.lineTo(0.11, 0.56);
    m_heart.cubicTo(0.0, 0.44, 0.0, 0.25, 0.11, 0.12);
    m_heart.cubicTo(0.22, 0.0, 0.4, 0.02, 0.5, 0.21);
    m_heart.cubicTo(0.6, 0.02, 0.78, 0.0, 0.89, 0.12);
    m_heart.cubicTo(1.0, 0.25, 1.0, 0.44, 0.89, 0.56);
    m_heart.closeSubpath();
}

void HeartValuePrivate::paint(QPainter* painter, const QRect& rect, const QPalette& palette, HeartValue::EditMode mode,
                              Qt::Alignment alignment, bool selected) const
{
    if(rect.isEmpty() || m_scale <= 0) {
        return;
    }

    auto symbols{m_symbols};
    if(symbols) {
        const auto& defaults = defaultLoveSymbols();
        if(symbols->lovedSymbol.isEmpty()) {
            symbols->lovedSymbol = defaults.lovedSymbol;
        }
        if(symbols->unlovedSymbol.isEmpty()) {
            symbols->unlovedSymbol = defaults.unlovedSymbol;
        }
    }

    const auto brushes = getHeartBrushes(palette, mode, selected, m_lovedColour, m_unlovedColour);
    QFont font{m_symbolFont.resolve(painter->font())};
    font.setPixelSize(m_scale);

    const qreal dpr  = painter->device()->devicePixelRatioF();
    QString cacheKey = u"HeartValue:%1|%2|%3|%4|%5"_s.arg(m_loved)
                           .arg(m_scale)
                           .arg(mode == HeartValue::EditMode::Editable ? 1 : 0)
                           .arg(rect.width())
                           .arg(rect.height())
                     + u"|%1|%2|%3|%4"_s.arg(alignment.toInt())
                           .arg(brushes.filled.color().name(QColor::HexArgb))
                           .arg(brushes.faded.color().name(QColor::HexArgb))
                           .arg(dpr);
    if(symbols) {
        cacheKey += u"|%1:%2|%3:%4|%5"_s.arg(QString::number(symbols->lovedSymbol.size()), symbols->lovedSymbol,
                                             QString::number(symbols->unlovedSymbol.size()), symbols->unlovedSymbol,
                                             font.key());
    }
    else {
        cacheKey += u"|painted"_s;
    }

    QPixmap pixmap;
    if(!QPixmapCache::find(cacheKey, &pixmap)) {
        pixmap = QPixmap{rect.size() * dpr};
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);

        QPainter pixmapPainter{&pixmap};
        pixmapPainter.setRenderHint(QPainter::Antialiasing, true);

        const int yOffset = (rect.height() - m_scale) / 2;
        int xOffset{0};
        if(alignment & Qt::AlignHCenter) {
            xOffset = (rect.width() - m_scale) / 2;
        }
        else if(alignment & Qt::AlignRight) {
            xOffset = rect.width() - m_scale;
        }
        pixmapPainter.translate(xOffset, yOffset);

        const auto brush = m_loved ? brushes.filled : brushes.faded;
        if(symbols) {
            const std::array text{symbols->lovedSymbol, symbols->unlovedSymbol};
            std::array<QRectF, 2> bounds;
            const QFontMetricsF metrics{font, painter->device()};
            qreal symbolSize{1.0};
            for(size_t i{0}; i < text.size(); ++i) {
                bounds[i]  = metrics.tightBoundingRect(text[i]);
                symbolSize = std::max({symbolSize, bounds[i].width(), bounds[i].height()});
            }

            const size_t index = m_loved ? 0 : 1;
            pixmapPainter.setRenderHint(QPainter::TextAntialiasing, true);
            pixmapPainter.setFont(font);
            pixmapPainter.setPen(QPen{brush, 1.0});
            pixmapPainter.translate(m_scale / 2.0, m_scale / 2.0);
            pixmapPainter.scale(m_scale / symbolSize, m_scale / symbolSize);
            pixmapPainter.drawText(-bounds[index].center(), text[index]);
        }
        else {
            pixmapPainter.scale(m_scale, m_scale);
            pixmapPainter.setPen(Qt::NoPen);
            pixmapPainter.setBrush(brush);
            pixmapPainter.drawPath(m_heart);
        }

        pixmapPainter.end();
        QPixmapCache::insert(cacheKey, pixmap);
    }

    painter->drawPixmap(rect.topLeft(), pixmap);
}

HeartValue::HeartValue()
    : HeartValue{false, false}
{ }

HeartValue::HeartValue(bool loved)
    : HeartValue{loved, 15}
{ }

HeartValue::HeartValue(bool loved, int scale)
    : HeartValue{loved, scale, {}}
{ }

HeartValue::HeartValue(bool loved, int scale, const QColor& colour)
    : HeartValue{loved, scale, colour, {}}
{ }

HeartValue::HeartValue(bool loved, int scale, const QColor& lovedColour, const QColor& unlovedColour,
                       std::optional<LoveSymbols> symbols)
    : p{new HeartValuePrivate(loved, scale, lovedColour, unlovedColour, std::move(symbols))}
{ }

HeartValue::~HeartValue()                                      = default;
HeartValue::HeartValue(const HeartValue& other)                = default;
HeartValue& HeartValue::operator=(const HeartValue& other)     = default;
HeartValue::HeartValue(HeartValue&& other) noexcept            = default;
HeartValue& HeartValue::operator=(HeartValue&& other) noexcept = default;

bool HeartValue::loved() const
{
    return p->m_loved;
}

int HeartValue::scale() const
{
    return p->m_scale;
}

void HeartValue::setLoved(bool loved)
{
    p->m_loved = loved;
}

void HeartValue::setScale(int scale)
{
    p->m_scale = scale;
}

void HeartValue::setSymbolFont(const QFont& font)
{
    p->m_symbolFont = font;
}

void HeartValue::paint(QPainter* painter, const QRect& rect, const QPalette& palette, EditMode mode,
                       Qt::Alignment alignment, bool selected) const
{
    p->paint(painter, rect, palette, mode, alignment, selected);
}

QSize HeartValue::sizeHint() const
{
    return p->m_scale * QSize{1, 1};
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
