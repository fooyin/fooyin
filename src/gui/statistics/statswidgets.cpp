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

#include "statswidgets.h"

#include <QButtonGroup>
#include <QCoreApplication>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QTime>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace Fooyin::Stats {
namespace {
QString tr(const char* text)
{
    return QCoreApplication::translate("Fooyin::Stats::Widgets", text);
}

QFont scaled(QFont font, qreal factor, bool bold = false)
{
    if(font.pointSizeF() > 0) {
        font.setPointSizeF(font.pointSizeF() * factor);
    }
    font.setBold(bold);
    return font;
}
} // namespace

QString formatListenTime(qint64 seconds)
{
    const qint64 minutes = seconds / 60;
    const qint64 hours   = minutes / 60;
    const qint64 days    = hours / 24;

    if(days > 0) {
        return tr("%1d %2h").arg(days).arg(hours % 24);
    }
    if(hours > 0) {
        return tr("%1h %2m").arg(hours).arg(minutes % 60);
    }
    return tr("%1m").arg(minutes);
}

QString formatDuration(qint64 ms)
{
    const qint64 seconds = ms / 1000;
    const qint64 hours   = seconds / 3600;
    const qint64 minutes = (seconds / 60) % 60;

    if(hours > 0) {
        return u"%1:%2:%3"_s.arg(hours)
            .arg(minutes, 2, 10, QLatin1Char{'0'})
            .arg(seconds % 60, 2, 10, QLatin1Char{'0'});
    }
    return u"%1:%2"_s.arg(minutes).arg(seconds % 60, 2, 10, QLatin1Char{'0'});
}

QString formatMinuteOfDay(int minute)
{
    if(minute < 0) {
        return {};
    }
    return QLocale().toString(QTime{minute / 60, minute % 60}, QLocale::ShortFormat);
}

QString formatDay(const QDate& day)
{
    return QLocale().toString(day, QLocale::ShortFormat);
}

QString formatRange(const QDate& first, const QDate& last)
{
    const QLocale locale;
    // Only mention the year when it isn't obvious from the range itself
    const QString format
        = first.year() == last.year() && last.year() == QDate::currentDate().year() ? u"d MMM"_s : u"d MMM yyyy"_s;
    return u"%1 – %2"_s.arg(locale.toString(first, format), locale.toString(last, format));
}

StatsCard::StatsCard(const QString& title, const QString& subtitle, QWidget* parent)
    : QFrame{parent}
    , m_subtitle{new QLabel(subtitle, this)}
    , m_header{new QHBoxLayout()}
    , m_body{new QVBoxLayout()}
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 16);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(title, this);
    titleLabel->setFont(scaled(font(), 1.15, true));
    titleLabel->setVisible(!title.isEmpty());

    auto* titles = new QVBoxLayout();
    titles->setSpacing(0);
    titles->addWidget(titleLabel);
    titles->addWidget(m_subtitle);

    const auto muted      = Colours::fromPalette(palette()).textMuted;
    QPalette mutedPalette = m_subtitle->palette();
    mutedPalette.setColor(QPalette::WindowText, muted);
    m_subtitle->setPalette(mutedPalette);
    m_subtitle->setFont(scaled(font(), 0.9));
    m_subtitle->setVisible(!subtitle.isEmpty());

    m_header->addLayout(titles, 1);
    layout->addLayout(m_header);
    layout->addLayout(m_body);
}

void StatsCard::setSubtitle(const QString& subtitle)
{
    m_subtitle->setText(subtitle);
    m_subtitle->setVisible(!subtitle.isEmpty());
}

void StatsCard::addHeaderWidget(QWidget* widget)
{
    m_header->addWidget(widget, 0, Qt::AlignTop);
}

void StatsCard::addWidget(QWidget* widget)
{
    m_body->addWidget(widget);
}

void StatsCard::addLayout(QLayout* layout)
{
    m_body->addLayout(layout);
}

void StatsCard::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);

    const auto colours = Colours::fromPalette(palette());
    painter.setPen(QPen{colours.grid, 1});
    painter.setBrush(colours.card);
    painter.drawRoundedRect(QRectF{rect()}.adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
}

MiniTile::MiniTile(QString label, QString value, QWidget* parent)
    : QWidget{parent}
    , m_label{std::move(label)}
    , m_value{std::move(value)}
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void MiniTile::setDetail(const QString& detail)
{
    m_detail = detail;
    updateGeometry();
    update();
}

void MiniTile::setDelta(qint64 current, qint64 previous)
{
    if(previous <= 0) {
        m_delta.reset();
    }
    else {
        m_delta = 100.0 * static_cast<double>(current - previous) / static_cast<double>(previous);
    }
    update();
}

QString MiniTile::deltaText() const
{
    if(!m_delta) {
        return {};
    }

    const int percent = static_cast<int>(std::lround(*m_delta));
    if(percent == 0) {
        return u"0%"_s;
    }
    return u"%1 %2%"_s.arg(percent > 0 ? u"▲"_s : u"▼"_s).arg(std::abs(percent));
}

QSize MiniTile::sizeHint() const
{
    const QFontMetrics label{scaled(font(), 0.85)};
    const QFontMetrics value{scaled(font(), 1.15, true)};

    int height = 16 + label.height() + value.height();
    if(!m_detail.isEmpty()) {
        height += label.height();
    }
    return {160, height};
}

QSize MiniTile::minimumSizeHint() const
{
    return {80, sizeHint().height()};
}

void MiniTile::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);

    const auto colours = Colours::fromPalette(palette());
    painter.setPen(Qt::NoPen);
    painter.setBrush(Colours::mix(colours.text, colours.card, 0.06));
    painter.drawRoundedRect(rect(), 8, 8);

    const QFont labelFont = scaled(font(), 0.85);
    const QFont valueFont = scaled(font(), 1.15, true);
    const QFontMetricsF labelMetrics{labelFont};
    const QFontMetricsF valueMetrics{valueFont};

    const QRectF inner = QRectF{rect()}.adjusted(12, 8, -12, -8);
    qreal y            = inner.top();

    painter.setFont(labelFont);
    painter.setPen(colours.textMuted);
    painter.drawText(QRectF{inner.left(), y, inner.width(), labelMetrics.height()}, Qt::AlignLeft | Qt::AlignVCenter,
                     labelMetrics.elidedText(m_label, Qt::ElideRight, inner.width()));
    y += labelMetrics.height();

    const QString delta = deltaText();
    qreal deltaWidth{0};
    if(!delta.isEmpty()) {
        deltaWidth = labelMetrics.horizontalAdvance(delta) + 8;

        QColor deltaColour = colours.textMuted;
        if(*m_delta > 0.5) {
            deltaColour = colours.seriesColour(2);
        }
        else if(*m_delta < -0.5) {
            deltaColour = colours.seriesColour(7);
        }
        // Green and red can vanish on a coloured card, where the arrow alone has to show the direction
        if(std::abs(deltaColour.lightness() - colours.card.lightness()) < 70) {
            deltaColour = colours.text;
        }
        painter.setPen(deltaColour);
        painter.drawText(QRectF{inner.right() - deltaWidth + 8, y, deltaWidth - 8, valueMetrics.height()},
                         Qt::AlignRight | Qt::AlignVCenter, delta);
    }

    painter.setFont(valueFont);
    painter.setPen(colours.text);
    painter.drawText(QRectF{inner.left(), y, inner.width() - deltaWidth, valueMetrics.height()},
                     Qt::AlignLeft | Qt::AlignVCenter,
                     valueMetrics.elidedText(m_value, Qt::ElideRight, inner.width() - deltaWidth));
    y += valueMetrics.height();

    if(!m_detail.isEmpty()) {
        painter.setFont(labelFont);
        painter.setPen(colours.textMuted);
        painter.drawText(QRectF{inner.left(), y, inner.width(), labelMetrics.height()},
                         Qt::AlignLeft | Qt::AlignVCenter,
                         labelMetrics.elidedText(m_detail, Qt::ElideRight, inner.width()));
    }
}

BigStat::BigStat(QString value, QString label, QWidget* parent)
    : QWidget{parent}
    , m_value{std::move(value)}
    , m_label{std::move(label)}
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize BigStat::sizeHint() const
{
    return {140, QFontMetrics{scaled(font(), 1.9, true)}.height() + QFontMetrics{scaled(font(), 0.9)}.height() + 2};
}

QSize BigStat::minimumSizeHint() const
{
    return {60, sizeHint().height()};
}

void BigStat::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};
    const auto colours = Colours::fromPalette(palette());

    const QFont valueFont = scaled(font(), 1.9, true);
    const QFont labelFont = scaled(font(), 0.9);
    const QFontMetricsF valueMetrics{valueFont};
    const QFontMetricsF labelMetrics{labelFont};

    painter.setFont(valueFont);
    painter.setPen(colours.text);
    painter.drawText(QRectF{0, 0, static_cast<qreal>(width()), valueMetrics.height()}, Qt::AlignLeft | Qt::AlignVCenter,
                     valueMetrics.elidedText(m_value, Qt::ElideRight, width()));

    painter.setFont(labelFont);
    painter.setPen(colours.textMuted);
    painter.drawText(QRectF{0, valueMetrics.height() + 2, static_cast<qreal>(width()), labelMetrics.height()},
                     Qt::AlignLeft | Qt::AlignVCenter, labelMetrics.elidedText(m_label, Qt::ElideRight, width()));
}

ChipBar::ChipBar(QWidget* parent)
    : QWidget{parent}
    , m_layout{new QHBoxLayout(this)}
    , m_group{new QButtonGroup(this)}
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(6);
    m_group->setExclusive(true);

    QObject::connect(m_group, &QButtonGroup::idClicked, this, &ChipBar::indexChanged);
}

void ChipBar::setItems(const QStringList& items)
{
    const auto buttons = m_group->buttons();
    for(auto* button : buttons) {
        m_group->removeButton(button);
        delete button;
    }
    while(m_layout->count() > 0) {
        delete m_layout->takeAt(0);
    }

    for(int i{0}; i < items.size(); ++i) {
        auto* button = new QPushButton(items.at(i), this);
        button->setCheckable(true);
        button->setFlat(false);
        m_group->addButton(button, i);
        m_layout->addWidget(button);
    }
    m_layout->addStretch();
}

void ChipBar::setCurrentIndex(int index)
{
    // Exclusive groups can't be cleared by unchecking, so lift the restriction briefly
    m_group->setExclusive(false);
    const auto buttons = m_group->buttons();
    for(auto* button : buttons) {
        button->setChecked(m_group->id(button) == index);
    }
    m_group->setExclusive(true);
}

int ChipBar::currentIndex() const
{
    return m_group->checkedId();
}

ShowMoreButton::ShowMoreButton(int total, int initial, int step, QWidget* parent)
    : QPushButton{parent}
    , m_total{total}
    , m_initial{initial}
    , m_step{step}
    , m_shown{std::min(total, initial)}
{
    setFlat(true);
    setVisible(total > initial);
    updateText();

    QObject::connect(this, &QPushButton::clicked, this, [this]() {
        // Once everything is shown the same button collapses the list again
        m_shown = m_shown >= m_total ? m_initial : std::min(m_total, m_shown + m_step);
        updateText();
        Q_EMIT shownChanged(m_shown);
    });
}

int ShowMoreButton::shown() const
{
    return m_shown;
}

void ShowMoreButton::updateText()
{
    setText(m_shown >= m_total ? tr("Show less") : tr("Show more"));
}

RowListWidget::RowListWidget(QWidget* parent)
    : ChartWidget{parent}
    , m_showRank{true}
    , m_rowHeight{0}
{ }

void RowListWidget::setRows(std::vector<Row> rows, bool showRank)
{
    m_rows     = std::move(rows);
    m_showRank = showRank;
    setHovered(-1);
    updateGeometry();
    update();
}

void RowListWidget::setRowHeight(int height)
{
    m_rowHeight = height;
    updateGeometry();
}

QSize RowListWidget::sizeHint() const
{
    const int rowHeight
        = m_rowHeight > 0 ? m_rowHeight : QFontMetrics{font()}.height() + QFontMetrics{smallFont()}.height() + 16;
    return {300, rowHeight * static_cast<int>(m_rows.size())};
}

QSize RowListWidget::minimumSizeHint() const
{
    return {160, sizeHint().height()};
}

int RowListWidget::hitTest(const QPoint& pos) const
{
    if(m_rows.empty() || pos.y() < 0) {
        return -1;
    }

    const int rowHeight = sizeHint().height() / static_cast<int>(m_rows.size());
    const int row       = pos.y() / rowHeight;
    return row < static_cast<int>(m_rows.size()) ? row : -1;
}

QString RowListWidget::tipFor(int item, const QPoint& /*pos*/) const
{
    const auto& row = m_rows.at(static_cast<size_t>(item));
    return row.subtitle.isEmpty() ? row.title : row.title + u"\n"_s + row.subtitle;
}

void RowListWidget::paintEvent(QPaintEvent* /*event*/)
{
    if(m_rows.empty()) {
        return;
    }

    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);

    const QFont titleFont = font();
    const QFont smaller   = smallFont();
    const QFontMetricsF titleMetrics{titleFont};
    const QFontMetricsF smallMetrics{smaller};
    const qreal rowHeight = static_cast<qreal>(sizeHint().height()) / static_cast<qreal>(m_rows.size());

    QFont valueFont = titleFont;
    valueFont.setBold(true);
    const QFontMetricsF valueMetrics{valueFont};

    qreal rightWidth{0};
    for(const auto& row : m_rows) {
        rightWidth = std::max(
            {rightWidth, valueMetrics.horizontalAdvance(row.value), smallMetrics.horizontalAdvance(row.trailing)});
    }

    const qreal rankWidth = m_showRank ? titleMetrics.horizontalAdvance(QString::number(m_rows.size())) + 16 : 0;
    const qreal textLeft  = 8 + rankWidth;
    const qreal textWidth = std::max(0.0, width() - textLeft - rightWidth - 20);

    for(size_t i{0}; i < m_rows.size(); ++i) {
        const auto& row = m_rows[i];
        const QRectF bounds{0, static_cast<qreal>(i) * rowHeight, static_cast<qreal>(width()), rowHeight};

        if(static_cast<int>(i) == hovered()) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(Colours::mix(colours().text, colours().card, 0.07));
            painter.drawRoundedRect(bounds, 6, 6);
        }

        if(m_showRank) {
            painter.setFont(titleFont);
            painter.setPen(colours().textMuted);
            painter.drawText(QRectF{8, bounds.top(), rankWidth - 8, bounds.height()}, Qt::AlignLeft | Qt::AlignVCenter,
                             QString::number(i + 1));
        }

        const bool hasSubtitle  = !row.subtitle.isEmpty();
        const qreal blockHeight = titleMetrics.height() + (hasSubtitle ? smallMetrics.height() : 0);
        const qreal top         = bounds.top() + (bounds.height() - blockHeight) / 2;

        painter.setFont(titleFont);
        painter.setPen(colours().text);
        painter.drawText(QRectF{textLeft, top, textWidth, titleMetrics.height()}, Qt::AlignLeft | Qt::AlignVCenter,
                         titleMetrics.elidedText(row.title, Qt::ElideRight, textWidth));

        if(hasSubtitle) {
            painter.setFont(smaller);
            painter.setPen(colours().textMuted);
            painter.drawText(QRectF{textLeft, top + titleMetrics.height(), textWidth, smallMetrics.height()},
                             Qt::AlignLeft | Qt::AlignVCenter,
                             smallMetrics.elidedText(row.subtitle, Qt::ElideRight, textWidth));
        }

        const bool hasTrailing = !row.trailing.isEmpty();
        const qreal rightBlock = valueMetrics.height() + (hasTrailing ? smallMetrics.height() : 0);
        const qreal rightTop   = bounds.top() + (bounds.height() - rightBlock) / 2;
        const qreal rightLeft  = width() - rightWidth - 10;

        painter.setFont(valueFont);
        painter.setPen(colours().text);
        painter.drawText(QRectF{rightLeft, rightTop, rightWidth, valueMetrics.height()},
                         Qt::AlignRight | Qt::AlignVCenter, row.value);

        if(hasTrailing) {
            painter.setFont(smaller);
            painter.setPen(colours().textMuted);
            painter.drawText(QRectF{rightLeft, rightTop + valueMetrics.height(), rightWidth, smallMetrics.height()},
                             Qt::AlignRight | Qt::AlignVCenter, row.trailing);
        }
    }
}

StoryCard::StoryCard(QWidget* parent)
    : QWidget{parent}
{
    setTint(QColor::fromHsv(210, 150, 120));
}

void StoryCard::setTint(const QColor& tint)
{
    m_tint = tint;

    // Everything on the card is light on a dark background, whatever the theme, so give it a palette to match.
    // Widgets on the card draw with these colours.
    const QColor text{255, 255, 255};
    QPalette cardPalette = palette();
    cardPalette.setColor(QPalette::Window, tint);
    cardPalette.setColor(QPalette::Base, tint);
    cardPalette.setColor(QPalette::WindowText, text);
    cardPalette.setColor(QPalette::Text, text);
    cardPalette.setColor(QPalette::ButtonText, text);
    cardPalette.setColor(QPalette::Highlight, Colours::mix(text, tint, 0.85));
    setPalette(cardPalette);
    update();
}

void StoryCard::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);

    QLinearGradient gradient{0, 0, 0, static_cast<qreal>(height())};
    gradient.setColorAt(0, m_tint.lighter(125));
    gradient.setColorAt(1, m_tint.darker(150));

    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawRoundedRect(rect(), 20, 20);
}

PageDots::PageDots(QWidget* parent)
    : QWidget{parent}
    , m_count{0}
    , m_current{0}
{
    setCursor(Qt::PointingHandCursor);
}

void PageDots::setCount(int count)
{
    m_count = count;
    updateGeometry();
    update();
}

void PageDots::setCurrent(int current)
{
    m_current = current;
    update();
}

QSize PageDots::sizeHint() const
{
    return {200, 18};
}

QRectF PageDots::dotRect(int index) const
{
    constexpr qreal dot     = 6;
    constexpr qreal current = 18;
    constexpr qreal gap     = 6;

    const qreal total = current + (m_count - 1) * dot + (m_count - 1) * gap;
    qreal x           = (width() - total) / 2;
    for(int i{0}; i < index; ++i) {
        x += (i == m_current ? current : dot) + gap;
    }
    return QRectF{x, (height() - dot) / 2, index == m_current ? current : dot, dot};
}

void PageDots::paintEvent(QPaintEvent* /*event*/)
{
    if(m_count < 2) {
        return;
    }

    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    const auto colours = Colours::fromPalette(palette());
    for(int i{0}; i < m_count; ++i) {
        QColor colour = i == m_current ? colours.accent : colours.text;
        if(i != m_current) {
            colour.setAlpha(70);
        }
        painter.setBrush(colour);
        painter.drawRoundedRect(dotRect(i), 3, 3);
    }
}

void PageDots::mousePressEvent(QMouseEvent* event)
{
    for(int i{0}; i < m_count; ++i) {
        // Pad the dots so they're easier to hit
        if(dotRect(i).adjusted(-3, -8, 3, 8).contains(event->position())) {
            Q_EMIT pageClicked(i);
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

QWidget* createEmptyHint(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setMargin(32);

    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, Colours::fromPalette(label->palette()).textMuted);
    label->setPalette(palette);
    return label;
}
} // namespace Fooyin::Stats
