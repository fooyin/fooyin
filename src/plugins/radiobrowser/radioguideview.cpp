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

#include "radioguideview.h"

#include <gui/iconloader.h>
#include <gui/widgets/messagebanner.h>

#include <QApplication>
#include <QPaintEvent>
#include <QPainter>
#include <QStyle>
#include <QStyledItemDelegate>

constexpr auto BannerMargin   = 8;
constexpr auto BannerPaddingX = 12;
constexpr auto BannerPaddingY = 8;

namespace {
class RadioGuideDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt{option};
        initStyleOption(&opt, index);

        const QIcon icon     = opt.icon;
        const QStyle* style  = opt.widget ? opt.widget->style() : QApplication::style();
        const QRect iconRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, opt.widget);

        opt.icon = {};
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        Fooyin::Gui::drawItemViewIcon(painter, opt, icon, iconRect, opt.decorationAlignment);
    }
};

QRect textBoundingRect(const QFontMetrics& metrics, const QString& text, int maxWidth)
{
    maxWidth = std::max(1, maxWidth);
    return metrics.boundingRect(QRect{0, 0, maxWidth, 0}, Qt::AlignCenter | Qt::TextWordWrap, text);
}
} // namespace

namespace Fooyin::RadioBrowser {
RadioGuideView::RadioGuideView(QWidget* parent)
    : QTreeView{parent}
{
    setItemDelegate(new RadioGuideDelegate(this));
}

void RadioGuideView::setStatusText(const QString& text)
{
    if(std::exchange(m_statusText, text) != text) {
        viewport()->update();
    }
}

void RadioGuideView::clearStatusText()
{
    setStatusText({});
}

void RadioGuideView::setErrorText(const QString& text)
{
    if(text.isEmpty()) {
        if(m_errorBanner) {
            m_errorBanner->deleteLater();
        }
        return;
    }

    ensureErrorBanner()->setText(text);
}

void RadioGuideView::clearErrorText()
{
    setErrorText({});
}

MessageBanner* RadioGuideView::ensureErrorBanner()
{
    if(!m_errorBanner) {
        m_errorBanner = new MessageBanner(viewport());
        QObject::connect(m_errorBanner, &MessageBanner::closed, m_errorBanner, &QObject::deleteLater);
    }

    return m_errorBanner;
}

void RadioGuideView::paintEvent(QPaintEvent* event)
{
    QTreeView::paintEvent(event);

    if(m_statusText.isEmpty()) {
        return;
    }

    QPainter painter{viewport()};
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect viewportRect = viewport()->rect();

    if(!m_statusText.isEmpty()) {
        const int maxTextWidth = std::max(120, (viewportRect.width() * 4) / 5);
        QRect textRect         = textBoundingRect(painter.fontMetrics(), m_statusText, maxTextWidth);
        textRect.moveCenter(viewportRect.center());
        textRect.adjust(-BannerPaddingX, -BannerPaddingY, BannerPaddingX, BannerPaddingY);

        painter.setPen(Qt::NoPen);

        QColor background = palette().color(QPalette::Base);
        background.setAlpha(220);
        painter.setBrush(background);

        painter.drawRoundedRect(textRect, 4, 4);

        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(textRect.adjusted(BannerPaddingX, BannerPaddingY, -BannerPaddingX, -BannerPaddingY),
                         Qt::AlignCenter | Qt::TextWordWrap, m_statusText);
    }

    if(m_errorBanner) {
        m_errorBanner->raise();
    }
}
} // namespace Fooyin::RadioBrowser

#include "moc_radioguideview.cpp"
