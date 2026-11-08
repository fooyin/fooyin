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

#include "statscharts.h"

#include <QCoreApplication>
#include <QEvent>
#include <QFontMetricsF>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Qt::StringLiterals;

namespace Fooyin::Stats {
namespace {
constexpr auto ValueLabelHeight = 16;
constexpr auto AxisLabelHeight  = 18;

//! A rectangle with its top corners rounded and its bottom corners square so it sits flat on a baseline.
QPainterPath roundedTop(const QRectF& rect, qreal radius)
{
    const qreal r = std::min({radius, rect.width() / 2, rect.height()});

    QPainterPath path;
    path.moveTo(rect.left(), rect.bottom());
    path.lineTo(rect.left(), rect.top() + r);
    path.quadTo(rect.left(), rect.top(), rect.left() + r, rect.top());
    path.lineTo(rect.right() - r, rect.top());
    path.quadTo(rect.right(), rect.top(), rect.right(), rect.top() + r);
    path.lineTo(rect.right(), rect.bottom());
    path.closeSubpath();
    return path;
}

//! A rectangle with its right corners rounded and its left corners square.
QPainterPath roundedRight(const QRectF& rect, qreal radius)
{
    const qreal r = std::min({radius, rect.height() / 2, rect.width()});

    QPainterPath path;
    path.moveTo(rect.left(), rect.top());
    path.lineTo(rect.right() - r, rect.top());
    path.quadTo(rect.right(), rect.top(), rect.right(), rect.top() + r);
    path.lineTo(rect.right(), rect.bottom() - r);
    path.quadTo(rect.right(), rect.bottom(), rect.right() - r, rect.bottom());
    path.lineTo(rect.left(), rect.bottom());
    path.closeSubpath();
    return path;
}

QColor withAlpha(QColor colour, int alpha)
{
    colour.setAlpha(alpha);
    return colour;
}

//! Rounds @p rough up to 1, 2, 2.5, 5 or 10 times a power of ten.
double niceStep(double rough)
{
    if(rough <= 1.0) {
        return 1.0;
    }

    const double magnitude = std::pow(10.0, std::floor(std::log10(rough)));
    const double fraction  = rough / magnitude;

    if(fraction <= 1.0) {
        return magnitude;
    }
    if(fraction <= 2.0) {
        return 2.0 * magnitude;
    }
    if(fraction <= 2.5) {
        return 2.5 * magnitude;
    }
    if(fraction <= 5.0) {
        return 5.0 * magnitude;
    }
    return 10.0 * magnitude;
}

//! Angle in degrees clockwise from twelve o'clock of @p point relative to @p centre.
double clockAngle(const QPointF& centre, const QPointF& point)
{
    const double angle = std::atan2(point.x() - centre.x(), centre.y() - point.y()) * 180.0 / std::numbers::pi;
    return angle < 0 ? angle + 360.0 : angle;
}
} // namespace

QString listensText(qint64 count)
{
    const QString number = QLocale().toString(count);
    return count == 1 ? QCoreApplication::translate("Fooyin::Stats::Charts", "%1 listen").arg(number)
                      : QCoreApplication::translate("Fooyin::Stats::Charts", "%1 listens").arg(number);
}

QString compactNumber(qint64 value)
{
    const QLocale locale;
    const double absolute = std::abs(static_cast<double>(value));

    if(absolute < 1000) {
        return locale.toString(value);
    }

    const auto format = [&locale](double number, const QString& suffix) -> QString {
        // Only keep a decimal when it adds information, e.g. 1.3K but 12K
        const int precision = std::abs(number) < 10 ? 1 : 0;
        QString text        = locale.toString(number, 'f', precision);
        if(precision == 1 && text.endsWith(locale.decimalPoint() + u"0"_s)) {
            text.chop(2);
        }
        return text + suffix;
    };

    if(absolute < 1'000'000) {
        return format(static_cast<double>(value) / 1000.0, u"K"_s);
    }
    return format(static_cast<double>(value) / 1'000'000.0, u"M"_s);
}

ChartWidget::ChartWidget(QWidget* parent)
    : QWidget{parent}
    , m_colours{Colours::fromPalette(palette())}
    , m_revealValue{1.0}
    , m_revealPending{false}
    , m_hovered{-1}
    , m_clickableItems{false}
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_reveal.setStartValue(0.0);
    m_reveal.setEndValue(1.0);
    m_reveal.setDuration(700);
    m_reveal.setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(&m_reveal, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_revealValue = value.toDouble();
        update();
    });
}

void ChartWidget::skipReveal()
{
    m_reveal.stop();
    m_revealPending = false;
    m_revealValue   = 1.0;
    update();
}

const Colours& ChartWidget::colours() const
{
    return m_colours;
}

double ChartWidget::reveal() const
{
    return m_revealValue;
}

void ChartWidget::restartReveal()
{
    m_reveal.stop();
    m_revealValue = 0.0;

    if(isVisible()) {
        m_reveal.start();
    }
    else {
        m_revealPending = true;
    }
    update();
}

QFont ChartWidget::smallFont() const
{
    QFont small = font();
    if(small.pointSizeF() > 0) {
        small.setPointSizeF(small.pointSizeF() * 0.85);
    }
    return small;
}

int ChartWidget::hitTest(const QPoint& /*pos*/) const
{
    return -1;
}

QString ChartWidget::tipFor(int /*item*/, const QPoint& /*pos*/) const
{
    return {};
}

int ChartWidget::hovered() const
{
    return m_hovered;
}

void ChartWidget::setHovered(int item)
{
    if(std::exchange(m_hovered, item) == item) {
        return;
    }

    if(m_clickableItems) {
        setCursor(item >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
    update();
}

void ChartWidget::setClickableItems(bool clickable)
{
    m_clickableItems = clickable;
}

bool ChartWidget::clickableItems() const
{
    return m_clickableItems;
}

void ChartWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    if(std::exchange(m_revealPending, false)) {
        m_reveal.start();
    }
}

void ChartWidget::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint pos = event->position().toPoint();
    const int item   = hitTest(pos);
    setHovered(item);

    if(item >= 0) {
        const QString tip = tipFor(item, pos);
        if(!tip.isEmpty()) {
            QToolTip::showText(event->globalPosition().toPoint() + QPoint{12, 16},
                               tip.toHtmlEscaped().replace(u'\n', u"<br>"_s), this);
            return;
        }
    }

    QToolTip::hideText();
}

void ChartWidget::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    setHovered(-1);
    QToolTip::hideText();
}

void ChartWidget::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    if(event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
        m_colours = Colours::fromPalette(palette());
        update();
    }
}

VerticalBarsChart::VerticalBarsChart(QWidget* parent)
    : ChartWidget{parent}
    , m_plotHeight{90}
{ }

void VerticalBarsChart::setData(std::vector<ChartData> points)
{
    m_data = std::move(points);
    setHovered(-1);
    restartReveal();
}

void VerticalBarsChart::setPlotHeight(int height)
{
    m_plotHeight = height;
    updateGeometry();
}

void VerticalBarsChart::setClickable(bool clickable)
{
    setClickableItems(clickable);
}

void VerticalBarsChart::setTipFormatter(std::function<QString(const ChartData&)> formatter)
{
    m_tipFormatter = std::move(formatter);
}

QSize VerticalBarsChart::sizeHint() const
{
    return {300, m_plotHeight + ValueLabelHeight + AxisLabelHeight};
}

QSize VerticalBarsChart::minimumSizeHint() const
{
    return {120, m_plotHeight + ValueLabelHeight + AxisLabelHeight};
}

QRectF VerticalBarsChart::plotRect() const
{
    return QRectF{0, ValueLabelHeight, static_cast<qreal>(width()),
                  static_cast<qreal>(height() - ValueLabelHeight - AxisLabelHeight)};
}

int VerticalBarsChart::hitTest(const QPoint& pos) const
{
    if(m_data.empty() || pos.x() < 0 || pos.x() >= width()) {
        return -1;
    }

    return std::min(static_cast<int>(pos.x() / (static_cast<qreal>(width()) / static_cast<qreal>(m_data.size()))),
                    static_cast<int>(m_data.size()) - 1);
}

QString VerticalBarsChart::tipFor(int item, const QPoint& /*pos*/) const
{
    const auto& points = m_data.at(static_cast<size_t>(item));
    if(m_tipFormatter) {
        return m_tipFormatter(points);
    }
    return u"%1\n%2"_s.arg(points.label, listensText(points.value));
}

void VerticalBarsChart::mousePressEvent(QMouseEvent* event)
{
    if(clickableItems() && event->button() == Qt::LeftButton) {
        if(const int item = hitTest(event->position().toPoint()); item >= 0) {
            Q_EMIT barClicked(item);
            return;
        }
    }
    ChartWidget::mousePressEvent(event);
}

void VerticalBarsChart::paintEvent(QPaintEvent* /*event*/)
{
    if(m_data.empty()) {
        return;
    }

    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF plot = plotRect();
    const auto count  = static_cast<qreal>(m_data.size());
    const qreal slot  = plot.width() / count;
    const qreal barW  = std::min(24.0, slot * 0.62);

    const int maxValue = std::max(1, std::ranges::max_element(m_data, {}, &ChartData::value)->value);
    const auto maxIdx  = static_cast<int>(std::ranges::max_element(m_data, {}, &ChartData::value) - m_data.begin());

    painter.setPen(QPen{colours().grid, 1});
    painter.drawLine(QPointF{plot.left(), plot.bottom()}, QPointF{plot.right(), plot.bottom()});

    const QFont labelFont = smallFont();
    const QFontMetricsF labelMetrics{labelFont};

    qreal widest{0};
    for(const auto& points : m_data) {
        widest = std::max(widest, labelMetrics.horizontalAdvance(points.label));
    }
    // Skip labels rather than letting them collide
    const int labelStep = std::max(1, static_cast<int>(std::ceil((widest + 6) / slot)));

    for(size_t i{0}; i < m_data.size(); ++i) {
        const auto& points = m_data[i];
        const qreal x      = plot.left() + slot * static_cast<qreal>(i) + (slot - barW) / 2;
        const qreal fullH  = plot.height() * points.value / maxValue;
        const qreal barH   = points.value > 0 ? std::max(2.0, fullH * reveal()) : 0.0;
        const bool isHot   = static_cast<int>(i) == hovered();

        if(barH > 0) {
            const QRectF bar{x, plot.bottom() - barH, barW, barH};
            painter.setPen(Qt::NoPen);
            painter.setBrush(isHot ? colours().accent.lighter(120) : colours().accent);
            painter.drawPath(roundedTop(bar, 4));
        }

        painter.setFont(labelFont);
        painter.setPen(colours().textMuted);
        if(static_cast<int>(i) % labelStep == 0) {
            painter.drawText(QRectF{plot.left() + slot * static_cast<qreal>(i), plot.bottom() + 3, slot,
                                    static_cast<qreal>(AxisLabelHeight - 3)},
                             Qt::AlignHCenter | Qt::AlignTop, points.label);
        }

        // Only label the extremes, the rest is in the tooltip
        if(points.value > 0 && (isHot || (hovered() < 0 && static_cast<int>(i) == maxIdx)) && reveal() >= 1.0) {
            painter.setPen(colours().text);
            painter.drawText(
                QRectF{plot.left() + slot * static_cast<qreal>(i) - 10, plot.bottom() - barH - 15, slot + 20, 14},
                Qt::AlignHCenter | Qt::AlignBottom, compactNumber(points.value));
        }
    }
}

RankBarsChart::RankBarsChart(QWidget* parent)
    : ChartWidget{parent}
    , m_rowHeight{28}
{ }

void RankBarsChart::setData(std::vector<ChartData> points)
{
    m_data = std::move(points);
    setHovered(-1);
    updateGeometry();
    restartReveal();
}

void RankBarsChart::setRowHeight(int height)
{
    m_rowHeight = height;
    updateGeometry();
}

void RankBarsChart::setClickable(bool clickable)
{
    setClickableItems(clickable);
}

QSize RankBarsChart::sizeHint() const
{
    return {300, static_cast<int>(m_data.size()) * m_rowHeight};
}

QSize RankBarsChart::minimumSizeHint() const
{
    return {160, static_cast<int>(m_data.size()) * m_rowHeight};
}

int RankBarsChart::hitTest(const QPoint& pos) const
{
    const int row = pos.y() / m_rowHeight;
    return pos.y() >= 0 && row < static_cast<int>(m_data.size()) ? row : -1;
}

QString RankBarsChart::tipFor(int item, const QPoint& /*pos*/) const
{
    const auto& points = m_data.at(static_cast<size_t>(item));
    return u"%1\n%2"_s.arg(points.label, listensText(points.value));
}

void RankBarsChart::mousePressEvent(QMouseEvent* event)
{
    if(clickableItems() && event->button() == Qt::LeftButton) {
        if(const int row = hitTest(event->position().toPoint()); row >= 0) {
            Q_EMIT rowClicked(row);
            return;
        }
    }
    ChartWidget::mousePressEvent(event);
}

void RankBarsChart::paintEvent(QPaintEvent* /*event*/)
{
    if(m_data.empty()) {
        return;
    }

    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);

    const QFontMetricsF metrics{font()};
    const QLocale locale;

    qreal widestLabel{0};
    qreal widestValue{0};
    int maxValue{1};
    for(const auto& points : m_data) {
        widestLabel = std::max(widestLabel, metrics.horizontalAdvance(points.label));
        widestValue = std::max(widestValue, metrics.horizontalAdvance(locale.toString(points.value)));
        maxValue    = std::max(maxValue, points.value);
    }

    const qreal labelWidth = std::min(widestLabel + 8, width() * 0.38);
    const qreal barLeft    = labelWidth + 12;
    const qreal barMax     = std::max(0.0, width() - barLeft - widestValue - 12);
    const qreal barHeight  = std::min<qreal>(12, m_rowHeight - 10);

    for(size_t i{0}; i < m_data.size(); ++i) {
        const auto& points = m_data[i];
        const QRectF row{0, static_cast<qreal>(i) * m_rowHeight, static_cast<qreal>(width()),
                         static_cast<qreal>(m_rowHeight)};
        const bool isHot = static_cast<int>(i) == hovered();

        if(isHot) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(withAlpha(colours().grid, 140));
            painter.drawRoundedRect(row, 6, 6);
        }

        painter.setPen(colours().text);
        painter.drawText(QRectF{4, row.top(), labelWidth - 8, row.height()}, Qt::AlignVCenter | Qt::AlignLeft,
                         metrics.elidedText(points.label, Qt::ElideRight, labelWidth - 8));

        const qreal barW = std::max(points.value > 0 ? 2.0 : 0.0, barMax * points.value / maxValue * reveal());
        const QRectF bar{barLeft, row.center().y() - barHeight / 2, barW, barHeight};
        if(barW > 0) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(isHot ? colours().accent.lighter(120) : colours().accent);
            painter.drawPath(roundedRight(bar, 4));
        }

        painter.setPen(colours().textMuted);
        painter.drawText(QRectF{bar.right() + 6, row.top(), widestValue + 6, row.height()},
                         Qt::AlignVCenter | Qt::AlignLeft, locale.toString(points.value));
    }
}

DonutChart::DonutChart(QWidget* parent)
    : ChartWidget{parent}
    , m_other{0}
    , m_ringSize{150}
{ }

void DonutChart::setData(std::vector<ChartData> points, int other)
{
    m_data  = std::move(points);
    m_other = other;
    rebuildSegments();
    setHovered(-1);
    updateGeometry();
    restartReveal();
}

void DonutChart::setCentreLabel(const QString& label)
{
    m_centreLabel = label;
    update();
}

void DonutChart::setRingSize(int size)
{
    m_ringSize = size;
    updateGeometry();
}

void DonutChart::rebuildSegments()
{
    m_segments.clear();

    for(size_t i{0}; i < m_data.size(); ++i) {
        if(m_data[i].value > 0) {
            m_segments.push_back({m_data[i].label, m_data[i].value, colours().seriesColour(i)});
        }
    }
    if(m_other > 0) {
        m_segments.push_back({tr("Other"), m_other, colours().other});
    }
}

qreal DonutChart::legendRowHeight() const
{
    return QFontMetricsF{font()}.height() + 8;
}

QSize DonutChart::sizeHint() const
{
    const int legendHeight = static_cast<int>(std::ceil(legendRowHeight() * static_cast<qreal>(m_segments.size())));
    return {360, std::max(m_ringSize, legendHeight)};
}

QSize DonutChart::minimumSizeHint() const
{
    return {200, sizeHint().height()};
}

int DonutChart::total() const
{
    int sum{0};
    for(const auto& segment : m_segments) {
        sum += segment.value;
    }
    return sum;
}

QRectF DonutChart::ringRect() const
{
    const qreal side = std::min<qreal>({static_cast<qreal>(m_ringSize), width() * 0.42, static_cast<qreal>(height())});
    const qreal pen  = side * 0.17;
    return QRectF{pen / 2 + 2, (height() - side) / 2 + pen / 2, side - pen - 4, side - pen - 4};
}

QRectF DonutChart::legendRect() const
{
    const QRectF ring = ringRect();
    const qreal left  = ring.right() + ring.width() * 0.17 + 24;
    return QRectF{left, 0, std::max(0.0, width() - left), static_cast<qreal>(height())};
}

int DonutChart::hitTest(const QPoint& pos) const
{
    const int sum = total();
    if(sum <= 0) {
        return -1;
    }

    const QRectF legend = legendRect();
    if(legend.contains(pos)) {
        const qreal rowH = legendRowHeight();
        const qreal top  = (height() - rowH * static_cast<qreal>(m_segments.size())) / 2;
        const int row    = static_cast<int>(std::floor((pos.y() - top) / rowH));
        return row >= 0 && row < static_cast<int>(m_segments.size()) ? row : -1;
    }

    const QRectF ring    = ringRect();
    const qreal distance = std::hypot(pos.x() - ring.center().x(), pos.y() - ring.center().y());
    const qreal thick    = ring.width() * 0.17 / (1 - 0.17);
    if(distance < ring.width() / 2 - thick / 2 - 4 || distance > ring.width() / 2 + thick / 2 + 4) {
        return -1;
    }

    const double angle = clockAngle(ring.center(), pos);
    double start{0};
    for(size_t i{0}; i < m_segments.size(); ++i) {
        start += 360.0 * m_segments[i].value / sum;
        if(angle < start) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(m_segments.size()) - 1;
}

QString DonutChart::tipFor(int item, const QPoint& /*pos*/) const
{
    const auto& segment = m_segments.at(static_cast<size_t>(item));
    const double share  = 100.0 * segment.value / std::max(1, total());
    return u"%1\n%2 (%3%)"_s.arg(segment.label, listensText(segment.value), QLocale().toString(share, 'f', 1));
}

void DonutChart::resizeEvent(QResizeEvent* event)
{
    ChartWidget::resizeEvent(event);
    update();
}

void DonutChart::paintEvent(QPaintEvent* /*event*/)
{
    const int sum = total();
    if(sum <= 0) {
        return;
    }

    // Segment colours depend on the palette, which may have changed since the data was set
    rebuildSegments();

    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF ring = ringRect();
    const qreal pen   = ring.width() * 0.17 / (1 - 0.17);

    // Segments start at twelve o'clock and run clockwise, separated by a small gap
    constexpr double gap = 1.6;
    double start         = 90.0;
    for(size_t i{0}; i < m_segments.size(); ++i) {
        const auto& segment = m_segments[i];
        const double sweep  = 360.0 * segment.value / sum * reveal();
        const bool isHot    = static_cast<int>(i) == hovered();

        QPen segmentPen{segment.colour, isHot ? pen + 4 : pen};
        segmentPen.setCapStyle(Qt::FlatCap);
        painter.setPen(segmentPen);

        const double drawn = m_segments.size() > 1 ? std::max(0.5, sweep - gap) : sweep;
        painter.drawArc(ring, static_cast<int>(std::lround(start * 16)), static_cast<int>(std::lround(-drawn * 16)));
        start -= sweep;
    }

    // Centre
    QString big;
    QString small;
    if(hovered() >= 0 && static_cast<size_t>(hovered()) < m_segments.size()) {
        const auto& segment = m_segments[static_cast<size_t>(hovered())];
        big                 = QLocale().toString(100.0 * segment.value / sum, 'f', 1) + u"%"_s;
        small               = segment.label;
    }
    else {
        big   = compactNumber(sum);
        small = m_centreLabel;
    }

    const qreal inner = ring.width() - pen;
    QFont bigFont     = font();
    bigFont.setBold(true);
    bigFont.setPointSizeF(bigFont.pointSizeF() * 1.4);
    painter.setFont(bigFont);
    painter.setPen(colours().text);
    painter.drawText(QRectF{ring.center().x() - inner / 2, ring.center().y() - inner / 2, inner, inner / 2 + 4},
                     Qt::AlignHCenter | Qt::AlignBottom, big);

    painter.setFont(smallFont());
    painter.setPen(colours().textMuted);
    painter.drawText(QRectF{ring.center().x() - inner / 2, ring.center().y() + 4, inner, inner / 2 - 4},
                     Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                     QFontMetricsF{smallFont()}.elidedText(small, Qt::ElideRight, inner));

    // Legend
    const QRectF legend = legendRect();
    const qreal rowH    = legendRowHeight();
    const qreal top     = (height() - rowH * static_cast<qreal>(m_segments.size())) / 2;
    const QFontMetricsF metrics{font()};
    const QLocale locale;

    qreal valueWidth{0};
    for(const auto& segment : m_segments) {
        valueWidth = std::max(valueWidth, metrics.horizontalAdvance(locale.toString(segment.value)));
    }
    constexpr qreal shareWidth = 44;

    painter.setFont(font());
    for(size_t i{0}; i < m_segments.size(); ++i) {
        const auto& segment = m_segments[i];
        const QRectF row{legend.left(), top + rowH * static_cast<qreal>(i), legend.width(), rowH};

        if(static_cast<int>(i) == hovered()) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(withAlpha(colours().grid, 140));
            painter.drawRoundedRect(row, 6, 6);
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(segment.colour);
        painter.drawEllipse(QPointF{row.left() + 10, row.center().y()}, 4.5, 4.5);

        const qreal textLeft = row.left() + 22;
        const qreal textMax  = std::max(0.0, row.right() - shareWidth - valueWidth - 16 - textLeft);

        painter.setPen(colours().text);
        painter.drawText(QRectF{textLeft, row.top(), textMax, row.height()}, Qt::AlignVCenter | Qt::AlignLeft,
                         metrics.elidedText(segment.label, Qt::ElideRight, textMax));

        painter.setPen(colours().textMuted);
        painter.drawText(QRectF{row.right() - shareWidth - valueWidth - 10, row.top(), valueWidth, row.height()},
                         Qt::AlignVCenter | Qt::AlignRight, locale.toString(segment.value));
        painter.drawText(QRectF{row.right() - shareWidth - 4, row.top(), shareWidth, row.height()},
                         Qt::AlignVCenter | Qt::AlignRight,
                         locale.toString(100.0 * segment.value / sum, 'f', 0) + u"%"_s);
    }
}

GaugeChart::GaugeChart(QWidget* parent)
    : ChartWidget{parent}
    , m_fraction{0}
{ }

void GaugeChart::setValue(double fraction, const QString& valueText, const QString& subText)
{
    m_fraction  = fraction;
    m_valueText = valueText;
    m_subText   = subText;
    restartReveal();
}

QSize GaugeChart::sizeHint() const
{
    return {200, 170};
}

void GaugeChart::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal side = std::min<qreal>(160, std::min(width(), height()));
    const qreal pen  = 14;
    const QRectF arc = QRectF{(width() - side) / 2 + pen / 2 + 2, (height() - side) / 2 + pen / 2 + 2, side - pen - 4,
                              side - pen - 4};

    // 270 degrees opening at the bottom, running clockwise from the bottom left
    constexpr int startAngle = 225 * 16;
    constexpr int fullSpan   = -270 * 16;

    QPen track{colours().grid, pen};
    track.setCapStyle(Qt::RoundCap);
    painter.setPen(track);
    painter.drawArc(arc, startAngle, fullSpan);

    const double filled = std::clamp(m_fraction, 0.0, 1.0) * reveal();
    if(filled > 0) {
        QPen value{colours().accent, pen};
        value.setCapStyle(Qt::RoundCap);
        painter.setPen(value);
        painter.drawArc(arc, startAngle, static_cast<int>(std::lround(fullSpan * filled)));
    }

    QFont big = font();
    big.setBold(true);
    big.setPointSizeF(big.pointSizeF() * 1.3);
    painter.setFont(big);
    painter.setPen(colours().text);
    painter.drawText(QRectF{arc.left(), arc.center().y() - 16, arc.width(), 24}, Qt::AlignCenter,
                     QFontMetricsF{big}.elidedText(m_valueText, Qt::ElideRight, arc.width() - 8));

    painter.setFont(smallFont());
    painter.setPen(colours().textMuted);
    painter.drawText(QRectF{arc.left(), arc.center().y() + 10, arc.width(), 18}, Qt::AlignCenter, m_subText);
}

ListeningClockChart::ListeningClockChart(QWidget* parent)
    : ChartWidget{parent}
    , m_hours{}
    , m_size{200}
{ }

void ListeningClockChart::setData(const std::array<int, 24>& hours)
{
    m_hours = hours;
    setHovered(-1);
    restartReveal();
}

void ListeningClockChart::setClockSize(int size)
{
    m_size = size;
    updateGeometry();
}

QSize ListeningClockChart::sizeHint() const
{
    return {m_size, m_size};
}

QPointF ListeningClockChart::centre() const
{
    return {width() / 2.0, height() / 2.0};
}

qreal ListeningClockChart::radius() const
{
    return std::min(width(), height()) / 2.0 - 4;
}

int ListeningClockChart::hitTest(const QPoint& pos) const
{
    const qreal distance = std::hypot(pos.x() - centre().x(), pos.y() - centre().y());
    if(distance < radius() * 0.3 || distance > radius()) {
        return -1;
    }
    return std::min(23, static_cast<int>(clockAngle(centre(), pos) / 15.0));
}

QString ListeningClockChart::tipFor(int item, const QPoint& /*pos*/) const
{
    return u"%1:00 – %2:00\n%3"_s.arg(item, 2, 10, QLatin1Char{'0'})
        .arg((item + 1) % 24, 2, 10, QLatin1Char{'0'})
        .arg(listensText(m_hours.at(static_cast<size_t>(item))));
}

void ListeningClockChart::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);

    const QPointF middle = centre();
    const qreal outer    = radius();
    const qreal inner    = outer * 0.38;
    const int maxValue   = std::max(1, *std::ranges::max_element(m_hours));

    // Faint rings to read the hours against
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen{colours().grid, 1});
    painter.drawEllipse(middle, inner, inner);
    painter.drawEllipse(middle, outer, outer);

    const qreal barWidth = std::max(2.0, 2 * std::numbers::pi * inner / 24 * 0.7);
    for(int hour{0}; hour < 24; ++hour) {
        const int value = m_hours[static_cast<size_t>(hour)];
        if(value <= 0) {
            continue;
        }

        const qreal fraction = static_cast<qreal>(value) / maxValue;
        const qreal length   = std::max(2.0, (outer - inner - 2) * fraction * reveal());
        const double angle   = (hour + 0.5) * 15.0 * std::numbers::pi / 180.0;
        const QPointF from{middle.x() + inner * std::sin(angle), middle.y() - inner * std::cos(angle)};
        const QPointF to{middle.x() + (inner + length) * std::sin(angle),
                         middle.y() - (inner + length) * std::cos(angle)};

        QColor colour = colours().ramp(0.35 + 0.65 * fraction);
        if(hour == hovered()) {
            colour = colour.lighter(120);
        }

        QPen pen{colour, barWidth};
        pen.setCapStyle(Qt::FlatCap);
        painter.setPen(pen);
        painter.drawLine(from, to);
    }

    // Hours at the quarters sit inside the hole
    painter.setFont(smallFont());
    painter.setPen(colours().textMuted);
    for(const int hour : {0, 6, 12, 18}) {
        const double angle = hour * 15.0 * std::numbers::pi / 180.0;
        const QPointF at{middle.x() + inner * 0.68 * std::sin(angle), middle.y() - inner * 0.68 * std::cos(angle)};
        painter.drawText(QRectF{at.x() - 14, at.y() - 8, 28, 16}, Qt::AlignCenter, QString::number(hour));
    }
}

GridHeatmapChart::GridHeatmapChart(QWidget* parent)
    : ChartWidget{parent}
    , m_rows{0}
    , m_columns{0}
    , m_columnLabelStep{1}
{ }

void GridHeatmapChart::setData(std::vector<int> values, int rows, int columns)
{
    m_values  = std::move(values);
    m_rows    = rows;
    m_columns = columns;
    setHovered(-1);
    updateGeometry();
    restartReveal();
}

void GridHeatmapChart::setLabels(QStringList rowLabels, QStringList columnLabels, int columnLabelStep)
{
    m_rowLabels       = std::move(rowLabels);
    m_columnLabels    = std::move(columnLabels);
    m_columnLabelStep = std::max(1, columnLabelStep);
    updateGeometry();
    update();
}

void GridHeatmapChart::setTipFormatter(std::function<QString(int, int, int)> formatter)
{
    m_tipFormatter = std::move(formatter);
}

namespace {
constexpr auto GridCellHeight = 20;
}

QSize GridHeatmapChart::sizeHint() const
{
    const int header = m_columnLabels.isEmpty() ? 0 : QFontMetrics{smallFont()}.height() + 4;
    return {300, header + m_rows * GridCellHeight};
}

QSize GridHeatmapChart::minimumSizeHint() const
{
    return {160, sizeHint().height()};
}

GridHeatmapChart::Layout GridHeatmapChart::layout() const
{
    const QFontMetricsF metrics{smallFont()};

    qreal labelWidth{0};
    for(const QString& label : m_rowLabels) {
        labelWidth = std::max(labelWidth, metrics.horizontalAdvance(label));
    }

    Layout layout;
    layout.left       = m_rowLabels.isEmpty() ? 0 : labelWidth + 8;
    layout.top        = m_columnLabels.isEmpty() ? 0 : metrics.height() + 4;
    layout.cellWidth  = m_columns > 0 ? (width() - layout.left) / m_columns : 0;
    layout.cellHeight = GridCellHeight;
    return layout;
}

int GridHeatmapChart::hitTest(const QPoint& pos) const
{
    const Layout geometry = layout();
    if(geometry.cellWidth <= 0 || pos.x() < geometry.left || pos.y() < geometry.top) {
        return -1;
    }

    const int column = static_cast<int>((pos.x() - geometry.left) / geometry.cellWidth);
    const int row    = static_cast<int>((pos.y() - geometry.top) / geometry.cellHeight);
    return column < m_columns && row < m_rows ? row * m_columns + column : -1;
}

QString GridHeatmapChart::tipFor(int item, const QPoint& /*pos*/) const
{
    const int row    = item / m_columns;
    const int column = item % m_columns;
    const int value  = m_values.at(static_cast<size_t>(item));

    if(m_tipFormatter) {
        return m_tipFormatter(row, column, value);
    }
    return listensText(value);
}

void GridHeatmapChart::paintEvent(QPaintEvent* /*event*/)
{
    if(m_values.empty() || m_columns <= 0) {
        return;
    }

    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(smallFont());

    const Layout geometry = layout();
    const int maxValue    = std::max(1, *std::ranges::max_element(m_values));

    painter.setPen(colours().textMuted);
    for(int row{0}; row < std::min<int>(m_rows, static_cast<int>(m_rowLabels.size())); ++row) {
        painter.drawText(QRectF{0, geometry.top + row * geometry.cellHeight, geometry.left - 8, geometry.cellHeight},
                         Qt::AlignVCenter | Qt::AlignLeft, m_rowLabels.at(row));
    }
    for(int column{0}; column < std::min<int>(m_columns, static_cast<int>(m_columnLabels.size()));
        column += m_columnLabelStep) {
        painter.drawText(QRectF{geometry.left + column * geometry.cellWidth, 0, geometry.cellWidth * m_columnLabelStep,
                                geometry.top},
                         Qt::AlignVCenter | Qt::AlignLeft, m_columnLabels.at(column));
    }

    painter.setPen(Qt::NoPen);
    painter.setOpacity(reveal());
    for(int row{0}; row < m_rows; ++row) {
        for(int column{0}; column < m_columns; ++column) {
            const int index = row * m_columns + column;
            const int value = m_values.at(static_cast<size_t>(index));

            QColor colour = colours().ramp(static_cast<double>(value) / maxValue);
            if(index == hovered()) {
                colour = colour.lighter(120);
            }
            painter.setBrush(colour);
            painter.drawRoundedRect(QRectF{geometry.left + column * geometry.cellWidth + 1,
                                           geometry.top + row * geometry.cellHeight + 1, geometry.cellWidth - 2,
                                           geometry.cellHeight - 2},
                                    3, 3);
        }
    }
}

CalendarHeatmapChart::CalendarHeatmapChart(QWidget* parent)
    : ChartWidget{parent}
    , m_maxValue{0}
    , m_maxCell{13}
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void CalendarHeatmapChart::setData(std::vector<int> listensPerDay, const QDate& firstDay)
{
    m_values   = std::move(listensPerDay);
    m_firstDay = firstDay;
    m_lastDay  = firstDay.addDays(static_cast<qint64>(m_values.size()) - 1);
    m_maxValue = m_values.empty() ? 0 : *std::ranges::max_element(m_values);

    setHovered(-1);
    updateGeometry();
    restartReveal();
}

void CalendarHeatmapChart::setMaxCellSize(int size)
{
    m_maxCell = size;
    updateGeometry();
}

CalendarHeatmapChart::Layout CalendarHeatmapChart::layoutFor(int width) const
{
    Layout layout;
    if(m_values.empty()) {
        return layout;
    }

    const QFontMetricsF metrics{smallFont()};
    Geometry& geo    = layout.geometry;
    geo.headerHeight = metrics.height() + 4;
    geo.labelWidth   = std::max(metrics.horizontalAdvance(u"0000"_s), metrics.horizontalAdvance(u"Wed"_s)) + 8;

    // Weeks wrap on to another row rather than shrinking the cells beyond this
    constexpr qreal minCell = 9;
    constexpr qreal minGap  = 2;
    const int maxWeeks      = std::max(8, static_cast<int>((width - geo.labelWidth + minGap) / (minCell + minGap)));

    // A year or less is one block, anything longer gets a block per calendar year
    std::vector<std::pair<QDate, QDate>> blocks;
    if(m_values.size() <= 371) {
        blocks.emplace_back(m_firstDay, m_lastDay);
    }
    else {
        for(int year = m_firstDay.year(); year <= m_lastDay.year(); ++year) {
            blocks.emplace_back(std::max(m_firstDay, QDate{year, 1, 1}), std::min(m_lastDay, QDate{year, 12, 31}));
        }
    }
    layout.showYears = blocks.size() > 1;

    for(const auto& [start, end] : blocks) {
        const QDate blockGrid = start.addDays(-(start.dayOfWeek() % 7));
        const int weeks       = static_cast<int>(blockGrid.daysTo(end) / 7) + 1;
        const int pieces      = (weeks + maxWeeks - 1) / maxWeeks;
        const int perPiece    = (weeks + pieces - 1) / pieces;

        for(int piece{0}; piece < pieces; ++piece) {
            Strip strip;
            strip.start = std::max(start, blockGrid.addDays(static_cast<qint64>(piece) * perPiece * 7));
            strip.end   = std::min(end, blockGrid.addDays(static_cast<qint64>(piece + 1) * perPiece * 7 - 1));
            if(strip.start > strip.end) {
                break;
            }
            strip.gridStart    = strip.start.addDays(-(strip.start.dayOfWeek() % 7));
            strip.weeks        = static_cast<int>(strip.gridStart.daysTo(strip.end) / 7) + 1;
            strip.firstOfBlock = piece == 0;
            layout.strips.push_back(strip);
        }
    }

    int widest{1};
    for(const Strip& strip : layout.strips) {
        widest = std::max(widest, strip.weeks);
    }

    const auto cellFor = [&](qreal gap) {
        return std::clamp((width - geo.labelWidth - gap * (widest - 1)) / widest, 3.0, static_cast<qreal>(m_maxCell));
    };
    geo.gap  = minGap;
    geo.cell = cellFor(geo.gap);
    if(geo.cell < 6) {
        geo.gap  = 1;
        geo.cell = cellFor(geo.gap);
    }
    return layout;
}

qreal CalendarHeatmapChart::stripHeight(const Geometry& geo)
{
    return geo.headerHeight + 7 * (geo.cell + geo.gap) - geo.gap;
}

QSize CalendarHeatmapChart::sizeHint() const
{
    return {600, heightForWidth(600)};
}

bool CalendarHeatmapChart::hasHeightForWidth() const
{
    return true;
}

int CalendarHeatmapChart::heightForWidth(int width) const
{
    const Layout layout = layoutFor(width);
    if(layout.strips.empty()) {
        return 0;
    }

    const auto strips = static_cast<qreal>(layout.strips.size());
    return static_cast<int>(
        std::ceil(stripHeight(layout.geometry) * strips + layout.geometry.stripSpacing * (strips - 1)));
}

QRectF CalendarHeatmapChart::cellRect(const Layout& layout, size_t strip, const QDate& day)
{
    const Geometry& geo    = layout.geometry;
    const qreal stripPitch = stripHeight(geo) + geo.stripSpacing;
    const qint64 week      = layout.strips[strip].gridStart.daysTo(day) / 7;
    const int row          = day.dayOfWeek() % 7;

    return QRectF{geo.labelWidth + static_cast<qreal>(week) * (geo.cell + geo.gap),
                  static_cast<qreal>(strip) * stripPitch + geo.headerHeight + row * (geo.cell + geo.gap), geo.cell,
                  geo.cell};
}

int CalendarHeatmapChart::valueAt(const QDate& day) const
{
    const qint64 index = m_firstDay.daysTo(day);
    if(index < 0 || index >= static_cast<qint64>(m_values.size())) {
        return 0;
    }
    return m_values[static_cast<size_t>(index)];
}

int CalendarHeatmapChart::hitTest(const QPoint& pos) const
{
    const Layout layout = layoutFor(width());
    if(layout.strips.empty()) {
        return -1;
    }

    const Geometry& geo    = layout.geometry;
    const qreal stripPitch = stripHeight(geo) + geo.stripSpacing;
    const auto strip       = static_cast<size_t>(std::max(0, pos.y()) / stripPitch);
    if(strip >= layout.strips.size() || pos.x() < geo.labelWidth) {
        return -1;
    }

    const qreal y = pos.y() - static_cast<qreal>(strip) * stripPitch - geo.headerHeight;
    if(y < 0) {
        return -1;
    }

    const int row  = static_cast<int>(y / (geo.cell + geo.gap));
    const int week = static_cast<int>((pos.x() - geo.labelWidth) / (geo.cell + geo.gap));
    if(row > 6) {
        return -1;
    }

    const QDate day = layout.strips[strip].gridStart.addDays(week * 7 + row);
    if(day < layout.strips[strip].start || day > layout.strips[strip].end) {
        return -1;
    }
    return static_cast<int>(m_firstDay.daysTo(day));
}

QString CalendarHeatmapChart::tipFor(int item, const QPoint& /*pos*/) const
{
    const QDate day = m_firstDay.addDays(item);
    return u"%1\n%2"_s.arg(QLocale().toString(day, QLocale::LongFormat),
                           listensText(m_values.at(static_cast<size_t>(item))));
}

void CalendarHeatmapChart::paintEvent(QPaintEvent* /*event*/)
{
    const Layout layout = layoutFor(width());
    if(layout.strips.empty()) {
        return;
    }

    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(smallFont());

    const Geometry& geo    = layout.geometry;
    const qreal stripPitch = stripHeight(geo) + geo.stripSpacing;
    const QFontMetricsF metrics{smallFont()};
    const QLocale locale;

    for(size_t s{0}; s < layout.strips.size(); ++s) {
        const Strip& strip = layout.strips[s];
        const qreal top    = static_cast<qreal>(s) * stripPitch;

        painter.setPen(colours().textMuted);
        if(layout.showYears && strip.firstOfBlock) {
            QFont bold = smallFont();
            bold.setBold(true);
            painter.setFont(bold);
            painter.drawText(QRectF{0, top, geo.labelWidth, geo.headerHeight}, Qt::AlignLeft | Qt::AlignVCenter,
                             QString::number(strip.start.year()));
            painter.setFont(smallFont());
        }

        // Weekday labels on alternate rows
        for(const int row : {1, 3, 5}) {
            painter.drawText(
                QRectF{0, top + geo.headerHeight + row * (geo.cell + geo.gap), geo.labelWidth - 6, geo.cell},
                Qt::AlignRight | Qt::AlignVCenter, locale.dayName(row, QLocale::ShortFormat).left(3));
        }

        // Month labels over the week each month starts in
        qreal lastRight = geo.labelWidth - 4;
        for(int week{0}; week < strip.weeks; ++week) {
            for(int row{0}; row < 7; ++row) {
                const QDate day = strip.gridStart.addDays(week * 7 + row);
                if(day.day() != 1 || day < strip.start || day > strip.end) {
                    continue;
                }

                const QString name = locale.monthName(day.month(), QLocale::ShortFormat);
                const qreal x      = geo.labelWidth + week * (geo.cell + geo.gap);
                // Skip a label that would collide with the last or run off the end
                const qreal nameWidth = metrics.horizontalAdvance(name);
                if(x >= lastRight + 4 && x + nameWidth <= width()) {
                    painter.drawText(QPointF{x, top + geo.headerHeight - 6}, name);
                    lastRight = x + nameWidth;
                }
                break;
            }
        }

        painter.setPen(Qt::NoPen);
        painter.setOpacity(reveal());
        for(QDate day = strip.start; day <= strip.end; day = day.addDays(1)) {
            const int value = valueAt(day);
            QColor colour   = colours().grid;
            if(value > 0 && m_maxValue > 0) {
                // Quantise to a few steps so single listens stay visible next to a very busy day
                const int level = std::clamp(static_cast<int>(std::ceil(4.0 * value / m_maxValue)), 1, 4);
                colour          = colours().ramp(level / 4.0);
            }
            if(static_cast<int>(m_firstDay.daysTo(day)) == hovered()) {
                colour = colour.lighter(125);
            }

            painter.setBrush(colour);
            const QRectF rect = cellRect(layout, s, day);
            if(geo.cell >= 6) {
                painter.drawRoundedRect(rect, 2, 2);
            }
            else {
                painter.drawRect(rect);
            }
        }
        painter.setOpacity(1.0);
    }
}

ListensLineChart::ListensLineChart(QWidget* parent)
    : ChartWidget{parent}
    , m_cumulative{true}
    , m_plotHeight{150}
{ }

void ListensLineChart::setData(std::vector<int> listensPerDay, const QDate& firstDay, bool cumulative)
{
    m_listens    = std::move(listensPerDay);
    m_firstDay   = firstDay;
    m_cumulative = cumulative;

    m_series.assign(m_listens.size(), 0);
    qint64 running{0};
    for(size_t i{0}; i < m_listens.size(); ++i) {
        running += m_listens[i];
        m_series[i] = cumulative ? running : m_listens[i];
    }

    setHovered(-1);
    restartReveal();
}

void ListensLineChart::setPlotHeight(int height)
{
    m_plotHeight = height;
    updateGeometry();
}

QSize ListensLineChart::sizeHint() const
{
    return {400, m_plotHeight + 32};
}

QSize ListensLineChart::minimumSizeHint() const
{
    return {200, m_plotHeight + 32};
}

ListensLineChart::Axis ListensLineChart::axis() const
{
    const qint64 maxValue = m_series.empty() ? 0 : *std::ranges::max_element(m_series);
    const double largest  = std::max<double>(1.0, static_cast<double>(maxValue));

    // Round the step so the labels are clean numbers, and stop at the first one above the data
    Axis axis;
    axis.step      = niceStep(largest / 4.0);
    axis.intervals = std::max(1, static_cast<int>(std::ceil(largest / axis.step)));
    return axis;
}

qreal ListensLineChart::plotMax() const
{
    const Axis scale = axis();
    return scale.step * scale.intervals;
}

QRectF ListensLineChart::plotRect() const
{
    const QFontMetricsF metrics{smallFont()};
    const qreal left = metrics.horizontalAdvance(compactNumber(static_cast<qint64>(plotMax()))) + 10;
    return QRectF{left, 8, std::max(1.0, width() - left - 14), static_cast<qreal>(height() - 8 - 24)};
}

qreal ListensLineChart::yFor(double value, const QRectF& plot, double max) const
{
    return plot.bottom() - plot.height() * value / max;
}

int ListensLineChart::hitTest(const QPoint& pos) const
{
    if(m_series.empty()) {
        return -1;
    }

    const QRectF plot = plotRect();
    if(pos.x() < plot.left() - 6 || pos.x() > plot.right() + 6) {
        return -1;
    }
    if(m_series.size() == 1) {
        return 0;
    }

    const double fraction = std::clamp((pos.x() - plot.left()) / plot.width(), 0.0, 1.0);
    return static_cast<int>(std::lround(fraction * static_cast<double>(m_series.size() - 1)));
}

QString ListensLineChart::tipFor(int item, const QPoint& /*pos*/) const
{
    const auto index = static_cast<size_t>(item);
    const QDate day  = m_firstDay.addDays(item);

    QString tip = QLocale().toString(day, QLocale::LongFormat) + u"\n"_s + listensText(m_listens.at(index));
    if(m_cumulative) {
        tip += u"\n"_s + tr("%1 in total").arg(QLocale().toString(m_series.at(index)));
    }
    return tip;
}

void ListensLineChart::paintEvent(QPaintEvent* /*event*/)
{
    if(m_series.empty()) {
        return;
    }

    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(smallFont());

    const QRectF plot     = plotRect();
    const double maxValue = plotMax();
    const QFontMetricsF metrics{smallFont()};

    // Gridlines and the value axis
    const Axis scale = axis();
    for(int tick{0}; tick <= scale.intervals; ++tick) {
        const double value = scale.step * tick;
        const qreal y      = yFor(value, plot, maxValue);
        painter.setPen(QPen{colours().grid, 1});
        painter.drawLine(QPointF{plot.left(), y}, QPointF{plot.right(), y});
        painter.setPen(colours().textMuted);
        painter.drawText(QRectF{0, y - 8, plot.left() - 6, 16}, Qt::AlignRight | Qt::AlignVCenter,
                         compactNumber(static_cast<qint64>(value)));
    }

    // Dates along the bottom, the ends aligned to the plot edges so they never run off the widget
    const auto count     = static_cast<qint64>(m_series.size());
    const QString format = count > 120 ? u"MMM yyyy"_s : u"d MMM"_s;
    const QLocale locale;
    std::vector<qint64> labelled{0};
    if(count > 1) {
        labelled = count > 3 ? std::vector<qint64>{0, (count - 1) / 3, 2 * (count - 1) / 3, count - 1}
                             : std::vector<qint64>{0, count - 1};
    }
    for(const qint64 i : labelled) {
        const qreal x
            = count == 1 ? plot.center().x() : plot.left() + plot.width() * static_cast<qreal>(i) / (count - 1);
        const QString text = locale.toString(m_firstDay.addDays(i), format);
        const qreal textW  = metrics.horizontalAdvance(text);
        const qreal left   = i == 0 ? plot.left() : (i == count - 1 ? plot.right() - textW : x - textW / 2);
        painter.setPen(colours().textMuted);
        painter.drawText(QPointF{left, plot.bottom() + 16}, text);
    }

    // The line, thinned out to roughly two points per pixel for long ranges
    const auto stride
        = static_cast<size_t>(std::max<qint64>(1, static_cast<qint64>(count / std::max(1.0, plot.width() * 2))));
    QPainterPath line;
    const auto pointFor = [&](size_t i, qint64 value) {
        const qreal x = count == 1 ? plot.center().x()
                                   : plot.left() + plot.width() * static_cast<qreal>(i) / static_cast<qreal>(count - 1);
        return QPointF{x, yFor(static_cast<double>(value), plot, maxValue)};
    };

    bool first{true};
    for(size_t i{0}; i < m_series.size(); i += stride) {
        qint64 value = m_series[i];
        if(!m_cumulative && stride > 1) {
            // Keep the peaks when thinning out daily values
            const size_t end = std::min(m_series.size(), i + stride);
            value            = *std::max_element(m_series.begin() + static_cast<std::ptrdiff_t>(i),
                                                 m_series.begin() + static_cast<std::ptrdiff_t>(end));
        }
        if(first) {
            line.moveTo(pointFor(i, value));
            first = false;
        }
        else {
            line.lineTo(pointFor(i, value));
        }
    }
    line.lineTo(pointFor(m_series.size() - 1, m_series.back()));

    painter.save();
    painter.setClipRect(QRectF{plot.left() - 8, 0, plot.width() * reveal() + 16, static_cast<qreal>(height())});

    QPainterPath area{line};
    area.lineTo(QPointF{count == 1 ? plot.center().x() : plot.right(), plot.bottom()});
    area.lineTo(QPointF{count == 1 ? plot.center().x() : plot.left(), plot.bottom()});
    area.closeSubpath();
    painter.setPen(Qt::NoPen);
    painter.setBrush(withAlpha(colours().accent, 36));
    painter.drawPath(area);

    QPen pen{colours().accent, 2};
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(line);
    painter.restore();

    const auto drawDot = [&](const QPointF& at) {
        painter.setPen(QPen{colours().card, 2});
        painter.setBrush(colours().accent);
        painter.drawEllipse(at, 4.5, 4.5);
    };

    if(hovered() >= 0) {
        const auto index = static_cast<size_t>(hovered());
        const QPointF at = pointFor(index, m_series[index]);
        painter.setPen(QPen{colours().textMuted, 1});
        painter.drawLine(QPointF{at.x(), plot.top()}, QPointF{at.x(), plot.bottom()});
        drawDot(at);
    }
    else if(reveal() >= 1.0) {
        drawDot(pointFor(m_series.size() - 1, m_series.back()));
    }
}
} // namespace Fooyin::Stats
