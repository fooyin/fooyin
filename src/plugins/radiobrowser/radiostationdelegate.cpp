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

#include "radiostationdelegate.h"

#include "radiobrowsermodel.h"

#include <gui/widgets/expandedtreeview.h>

#include <QApplication>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QStyle>

constexpr auto LineSpacing           = 2;
constexpr auto RightCaptionWidth     = 190;
constexpr auto IconPadding           = 8;
constexpr auto BottomIconPadding     = 12;
constexpr auto ColumnIconPadding     = 4;
constexpr auto DefaultColumnIconSize = 36;

namespace {
using Fooyin::ExpandedTreeView;

const ExpandedTreeView* iconModeView(const QStyleOptionViewItem& option)
{
    const auto* view = qobject_cast<const ExpandedTreeView*>(option.widget);
    return view && view->viewMode() == ExpandedTreeView::ViewMode::Icon ? view : nullptr;
}

QSize iconDecorationSize(const QStyleOptionViewItem& option)
{
    return option.decorationSize.isValid() ? option.decorationSize : QSize{96, 64};
}

QRect iconRect(const QStyleOptionViewItem& option, ExpandedTreeView::CaptionDisplay captions)
{
    const QSize iconSize = iconDecorationSize(option);
    QRect rect{{}, iconSize};

    switch(captions) {
        case ExpandedTreeView::CaptionDisplay::Right:
            rect.moveLeft(option.rect.left() + IconPadding);
            rect.moveTop(option.rect.top() + std::max(0, (option.rect.height() - iconSize.height()) / 2));
            break;
        case ExpandedTreeView::CaptionDisplay::Bottom:
        case ExpandedTreeView::CaptionDisplay::None:
            rect.moveLeft(option.rect.left() + std::max(0, (option.rect.width() - iconSize.width()) / 2));
            rect.moveTop(option.rect.top()
                         + (captions == ExpandedTreeView::CaptionDisplay::Bottom ? BottomIconPadding : 0));
            break;
    }

    return rect;
}

int captionTextHeight(const QStyleOptionViewItem& option, const int lineCount)
{
    if(lineCount <= 0) {
        return 0;
    }

    QFont titleFont = option.font;
    titleFont.setBold(true);
    return QFontMetrics{titleFont}.height() + ((lineCount - 1) * (option.fontMetrics.height() + LineSpacing));
}

QRect iconTextRect(const QStyleOptionViewItem& option, ExpandedTreeView::CaptionDisplay captions)
{
    const QStyle* style = option.widget ? option.widget->style() : QApplication::style();
    const int margin    = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &option, option.widget);
    QRect rect          = option.rect;

    switch(captions) {
        case ExpandedTreeView::CaptionDisplay::Right:
            rect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);
            rect.setLeft(std::max(rect.left(), iconRect(option, captions).right() + IconPadding));
            rect.adjust(margin, 0, -margin, 0);
            break;
        case ExpandedTreeView::CaptionDisplay::Bottom:
            rect.setTop(rect.top() + BottomIconPadding + iconDecorationSize(option).height() + IconPadding);
            rect.adjust(margin, 0, -margin, 0);
            break;
        case ExpandedTreeView::CaptionDisplay::None:
            rect = {};
            break;
    }

    return rect;
}

void drawIconItemBackground(QPainter* painter, QStyleOptionViewItem* option)
{
    if(option->state & QStyle::State_Selected) {
        return;
    }

    painter->setPen(option->palette.color(QPalette::Mid));
    painter->setBrush(option->backgroundBrush.style() == Qt::NoBrush ? option->palette.color(QPalette::Base)
                                                                     : option->backgroundBrush);
    painter->drawRoundedRect(option->rect.adjusted(1, 1, -1, -1), 5, 5);
    option->backgroundBrush = Qt::NoBrush;
}

struct PixmapStats
{
    double opaqueLuminance{0.5};
    double transparentRatio{0.0};
};

PixmapStats pixmapStats(const QPixmap& pixmap)
{
    PixmapStats stats;

    const QImage image = pixmap.toImage();
    double luminance{0.0};
    int opaqueCount{0};
    int transparentCount{0};

    for(int y{0}; y < image.height(); ++y) {
        for(int x{0}; x < image.width(); ++x) {
            const QRgb pixel = image.pixel(x, y);
            if(qAlpha(pixel) < 32) {
                ++transparentCount;
                continue;
            }

            luminance += ((0.299 * qRed(pixel)) + (0.587 * qGreen(pixel)) + (0.114 * qBlue(pixel))) / 255.0;
            ++opaqueCount;
        }
    }

    const int pixelCount   = image.width() * image.height();
    stats.opaqueLuminance  = opaqueCount > 0 ? luminance / opaqueCount : 0.5;
    stats.transparentRatio = pixelCount > 0 ? static_cast<double>(transparentCount) / pixelCount : 0.0;
    return stats;
}

QColor blend(const QColor& first, const QColor& second, double firstWeight)
{
    firstWeight = std::clamp(firstWeight, 0.0, 1.0);
    const double secondWeight{1.0 - firstWeight};
    return QColor::fromRgbF((first.redF() * firstWeight) + (second.redF() * secondWeight),
                            (first.greenF() * firstWeight) + (second.greenF() * secondWeight),
                            (first.blueF() * firstWeight) + (second.blueF() * secondWeight));
}

QColor iconBackdropColor(const QStyleOptionViewItem& option, const PixmapStats& stats)
{
    const QColor base = option.palette.color(QPalette::Base);
    const QColor text = option.palette.color(QPalette::Text);

    QColor colour = stats.opaqueLuminance < 0.45 ? blend(text, base, 0.82) : blend(base, text, 0.88);
    colour.setAlphaF(0.96);
    return colour;
}

void drawIconInRect(QPainter* painter, const QStyleOptionViewItem& option, const QIcon& icon, const QRect& target)
{
    if(target.isEmpty()) {
        return;
    }

    QIcon::Mode mode{QIcon::Normal};
    if(!(option.state & QStyle::State_Enabled)) {
        mode = QIcon::Disabled;
    }
    else if(option.state & QStyle::State_Selected) {
        mode = QIcon::Selected;
    }

    const qreal dpr = option.widget ? option.widget->devicePixelRatioF() : qApp->devicePixelRatio();
    QPixmap pixmap  = icon.pixmap(target.size() * dpr, dpr, mode);
    if(pixmap.isNull()) {
        return;
    }

    pixmap = pixmap.scaled(target.size() * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    pixmap.setDevicePixelRatio(dpr);

    const QSize pixmapSize = pixmap.deviceIndependentSize().toSize();
    const QRect pixmapRect{target.topLeft()
                               + QPoint{std::max(0, (target.width() - pixmapSize.width()) / 2),
                                        std::max(0, (target.height() - pixmapSize.height()) / 2)},
                           pixmapSize};

    const auto stats = pixmapStats(pixmap);
    if(stats.transparentRatio > 0.2) {
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(iconBackdropColor(option, stats));
        painter->drawRoundedRect(target, 4, 4);
        painter->restore();
    }

    painter->drawPixmap(pixmapRect, pixmap);
}

void drawIcon(QPainter* painter, const QStyleOptionViewItem& option, const QIcon& icon,
              ExpandedTreeView::CaptionDisplay captions)
{
    drawIconInRect(painter, option, icon, iconRect(option, captions));
}

void drawElidedText(QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect, const QString& text,
                    const Qt::Alignment alignment, QPalette::ColorRole role, const QFont& font)
{
    if(text.isEmpty() || rect.width() <= 0 || rect.height() <= 0) {
        return;
    }

    const QFontMetrics metrics{font};
    const QString elided = metrics.elidedText(text, Qt::ElideRight, rect.width());
    const QColor colour  = (option.state & QStyle::State_Selected) ? option.palette.color(QPalette::HighlightedText)
                                                                   : option.palette.color(role);

    painter->setFont(font);
    painter->setPen(colour);
    painter->drawText(rect, alignment | Qt::AlignVCenter | Qt::TextSingleLine, elided);
}

void drawCaptionLines(QPainter* painter, const QStyleOptionViewItem& option, QRect rect,
                      ExpandedTreeView::CaptionDisplay captions, const QStringList& lines)
{
    if(captions == ExpandedTreeView::CaptionDisplay::None || !rect.isValid()) {
        return;
    }

    QFont titleFont{option.font};
    titleFont.setBold(true);

    const int titleHeight = QFontMetrics{titleFont}.height();
    const int lineHeight  = option.fontMetrics.height();
    const int textHeight  = captionTextHeight(option, std::max(1, static_cast<int>(lines.size())));
    int y                 = captions == ExpandedTreeView::CaptionDisplay::Right
                              ? rect.y() + std::max(0, (rect.height() - textHeight) / 2)
                              : rect.y();
    const Qt::Alignment alignment
        = captions == ExpandedTreeView::CaptionDisplay::Bottom ? Qt::AlignCenter : Qt::AlignLeft;

    for(int line{0}; line < lines.size(); ++line) {
        const bool titleLine = line == 0;
        const int height     = titleLine ? titleHeight : lineHeight;
        const QRect lineRect{rect.x(), y, rect.width(), height};

        drawElidedText(painter, option, lineRect, lines.at(line), alignment, QPalette::Text,
                       titleLine ? titleFont : option.font);
        y += height + LineSpacing;
    }
}

QSize iconItemSize(const QStyleOptionViewItem& option, ExpandedTreeView::CaptionDisplay captions, int captionLines)
{
    const QSize iconSize = iconDecorationSize(option);
    const int textLines  = captions == ExpandedTreeView::CaptionDisplay::None ? 0 : std::max(1, captionLines);
    const int textHeight = captionTextHeight(option, textLines);

    switch(captions) {
        case ExpandedTreeView::CaptionDisplay::Right:
            return {iconSize.width() + RightCaptionWidth + (3 * IconPadding),
                    std::max(iconSize.height() + (2 * IconPadding), textHeight + (2 * IconPadding))};
        case ExpandedTreeView::CaptionDisplay::Bottom:
            return {std::max(iconSize.width() + (2 * IconPadding), RightCaptionWidth),
                    BottomIconPadding + iconSize.height() + textHeight + (2 * IconPadding) + IconPadding};
        case ExpandedTreeView::CaptionDisplay::None:
            return {iconSize.width() + (2 * IconPadding), iconSize.height() + (2 * IconPadding)};
    }

    return {};
}

int columnIconSize(const QStyleOptionViewItem& option)
{
    const QSize decorationSize
        = option.decorationSize.isValid() ? option.decorationSize : QSize{DefaultColumnIconSize, DefaultColumnIconSize};
    return std::max(decorationSize.width(), decorationSize.height());
}

QRect columnIconRect(const QStyleOptionViewItem& option)
{
    const int iconSize = columnIconSize(option);
    QRect rect{option.rect.left() + ColumnIconPadding, option.rect.top() + ColumnIconPadding, iconSize, iconSize};
    rect.moveTop(option.rect.top() + std::max(0, (option.rect.height() - iconSize) / 2));
    return rect;
}

QRect columnTextRect(const QStyleOptionViewItem& option)
{
    const QStyle* style = option.widget ? option.widget->style() : QApplication::style();
    const int margin    = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &option, option.widget);
    QRect rect          = option.rect;
    rect.setLeft(columnIconRect(option).right() + (2 * ColumnIconPadding));
    rect.adjust(margin, 0, -margin, 0);
    return rect;
}

void drawColumnStationItem(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index,
                           const QIcon& icon)
{
    QStyleOptionViewItem opt{option};
    const QString text = index.data(Qt::DisplayRole).toString();

    opt.text.clear();
    opt.icon = {};
    opt.features &= ~(QStyleOptionViewItem::HasDisplay | QStyleOptionViewItem::HasDecoration);

    painter->save();

    const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    drawIconInRect(painter, opt, icon, columnIconRect(opt));
    drawElidedText(painter, opt, columnTextRect(opt), text, Qt::AlignLeft, QPalette::Text, opt.font);

    painter->restore();
}
} // namespace

namespace Fooyin::RadioBrowser {
bool RadioStationDelegate::uniformStationIcons() const
{
    return m_uniformStationIcons;
}

void RadioStationDelegate::setUniformStationIcons(bool enabled)
{
    m_uniformStationIcons = enabled;
}

void RadioStationDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto* iconView = iconModeView(option);
    const auto icon      = index.data(Qt::DecorationRole).value<QIcon>();

    if(!iconView) {
        if(m_uniformStationIcons && index.column() == Station && !icon.isNull()) {
            QStyleOptionViewItem opt{option};
            initStyleOption(&opt, index);
            // initStyleOption can replace the configured decoration size with the icon's actual size
            opt.decorationSize = option.decorationSize;
            drawColumnStationItem(painter, opt, index, icon);
            return;
        }

        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    if(icon.isNull()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    const auto captions = iconView->captionDisplay();

    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);
    opt.decorationSize = option.decorationSize;

    const QRect textRect          = iconTextRect(opt, captions);
    const QVariant iconBackground = index.data(RadioBrowserModel::IconBackgroundRole);
    if(iconBackground.canConvert<QBrush>()) {
        opt.backgroundBrush = iconBackground.value<QBrush>();
    }
    else if(iconBackground.canConvert<QColor>()) {
        opt.backgroundBrush = iconBackground.value<QColor>();
    }

    if(m_uniformStationIcons) {
        opt.icon = {};
    }
    opt.text.clear();

    painter->save();

    const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    drawIconItemBackground(painter, &opt);
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    if(m_uniformStationIcons) {
        drawIcon(painter, opt, icon, captions);
    }

    drawCaptionLines(painter, opt, textRect, captions,
                     index.data(RadioBrowserModel::IconCaptionLinesRole).toStringList());

    painter->restore();
}

QSize RadioStationDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto applyHeightOverride = [&index](QSize size) {
        const QSize sizeHint = index.data(Qt::SizeHintRole).toSize();
        if(sizeHint.height() > 0) {
            size.setHeight(sizeHint.height());
        }
        return size;
    };

    const auto* iconView = iconModeView(option);

    if(!iconView) {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        if(index.column() == Station) {
            size.setHeight(std::max(size.height(), columnIconSize(option) + (2 * ColumnIconPadding)));
        }
        return applyHeightOverride(size);
    }

    const int captionLines
        = std::max(static_cast<int>(index.data(RadioBrowserModel::IconCaptionLinesRole).toStringList().size()),
                   index.data(RadioBrowserModel::IconCaptionLineCountRole).toInt());
    return applyHeightOverride(iconItemSize(option, iconView->captionDisplay(), captionLines));
}
} // namespace Fooyin::RadioBrowser
