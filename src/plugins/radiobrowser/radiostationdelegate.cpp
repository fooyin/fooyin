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

#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <gui/widgets/expandedtreeview.h>

#include <QApplication>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QStyle>

constexpr auto LineSpacing           = 2;
constexpr auto RightCaptionWidth     = 190;
constexpr auto IconPadding           = 8;
constexpr auto BottomIconPadding     = 12;
constexpr auto ColumnIconPadding     = 4;
constexpr auto DefaultColumnIconSize = 36;
constexpr auto SavedBadgeMinSize     = 7;
constexpr auto SavedBadgeMaxSize     = 20;
constexpr auto SavedBadgeCellSize    = 14;
constexpr auto SavedBadgeMargin      = 3;
constexpr auto SavedBadgeItemMargin  = 8;

namespace Fooyin::RadioBrowser {
namespace {
const ExpandedTreeView* iconModeView(const QStyleOptionViewItem& option)
{
    const auto* view = qobject_cast<const ExpandedTreeView*>(option.widget);
    return view && view->viewMode() == ExpandedTreeView::ViewMode::Icon ? view : nullptr;
}

QSize iconDecorationSize(const QStyleOptionViewItem& option)
{
    return option.decorationSize.isValid() ? option.decorationSize : QSize{96, 64};
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

QRect iconTextRect(const QStyleOptionViewItem& option, ExpandedTreeView::CaptionDisplay captions, bool hasIcon)
{
    QRect rect{option.rect};

    const QStyle* style = option.widget ? option.widget->style() : QApplication::style();
    const int margin    = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &option, option.widget);

    switch(captions) {
        case ExpandedTreeView::CaptionDisplay::Right:
            rect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);
            if(hasIcon) {
                rect.setLeft(std::max(rect.left(), iconRect(option, captions).right() + IconPadding));
            }
            else {
                rect.setLeft(rect.left() + (2 * IconPadding));
            }
            rect.adjust(margin, 0, -margin, 0);
            break;
        case ExpandedTreeView::CaptionDisplay::Bottom:
            if(hasIcon) {
                rect.setTop(rect.top() + BottomIconPadding + iconDecorationSize(option).height() + IconPadding);
            }
            else {
                rect.setTop(rect.top() + BottomIconPadding + (iconDecorationSize(option).height() / 2));
            }
            rect.adjust(margin, 0, -margin, 0);
            break;
        case ExpandedTreeView::CaptionDisplay::None:
            rect = {};
            break;
    }

    return rect;
}

void drawIconItemBackground(QPainter* painter, QStyleOptionViewItem* option, int borderWidth)
{
    if(option->state & QStyle::State_Selected) {
        return;
    }

    painter->setPen(borderWidth > 0 ? QPen{option->palette.color(QPalette::Mid), static_cast<qreal>(borderWidth)}
                                    : Qt::NoPen);
    painter->setBrush(option->backgroundBrush.style() == Qt::NoBrush ? option->palette.color(QPalette::Base)
                                                                     : option->backgroundBrush);
    const qreal inset = std::max<qreal>(1.0, static_cast<qreal>(borderWidth) / 2.0);
    painter->drawRoundedRect(QRectF{option->rect}.adjusted(inset, inset, -inset, -inset), 5, 5);
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

QRect savedBadgeRect(const QRect& anchor)
{
    if(anchor.isEmpty()) {
        return {};
    }

    const int anchorSize = std::min(anchor.width(), anchor.height());
    const int size       = std::min(
        std::clamp(static_cast<int>(std::round(anchorSize * 0.5)), SavedBadgeMinSize, SavedBadgeMaxSize), anchorSize);
    return {anchor.right() - size + 1 - SavedBadgeMargin, anchor.top() + SavedBadgeMargin, size, size};
}

int savedBadgeSizeForAnchor(const QRect& anchor)
{
    if(anchor.isEmpty()) {
        return 0;
    }

    const int anchorSize = std::min(anchor.width(), anchor.height());
    return std::min(std::clamp(static_cast<int>(std::round(anchorSize * 0.5)), SavedBadgeMinSize, SavedBadgeMaxSize),
                    anchorSize);
}

QRect savedBadgeRectForColumn(const QStyleOptionViewItem& option, bool hasIcon)
{
    if(hasIcon) {
        return savedBadgeRect(columnIconRect(option));
    }

    const int size = std::min(SavedBadgeCellSize, std::max(0, option.rect.height() - (2 * SavedBadgeMargin)));
    return {option.rect.right() - size - SavedBadgeMargin, option.rect.top() + ((option.rect.height() - size) / 2),
            size, size};
}

QRect savedBadgeRectForIconItem(const QStyleOptionViewItem& option, ExpandedTreeView::CaptionDisplay captions,
                                bool hasIcon)
{
    const int size = savedBadgeSizeForAnchor(hasIcon ? iconRect(option, captions) : option.rect);
    if(size <= 0 || option.rect.isEmpty()) {
        return {};
    }

    return {option.rect.right() - size - SavedBadgeItemMargin, option.rect.bottom() - size - SavedBadgeItemMargin, size,
            size};
}

void drawSavedBadge(QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect)
{
    if(rect.isEmpty()) {
        return;
    }

    QColor background = option.palette.color(QPalette::Highlight);
    background.setAlphaF(0.92);

    QColor border = (option.state & QStyle::State_Selected) ? option.palette.color(QPalette::HighlightedText)
                                                            : option.palette.color(QPalette::Base);
    border.setAlphaF(0.88);

    painter->save();

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen{border, 1.0});
    painter->setBrush(background);
    painter->drawEllipse(rect);

    const int iconInset   = std::max(2, rect.width() / 4);
    const QRectF iconRect = QRectF{rect}.adjusted(iconInset, iconInset, -iconInset, -iconInset);

    if(iconRect.width() >= 6 && iconRect.height() >= 6) {
        Gui::drawItemViewIcon(painter, option, Gui::iconFromTheme(Constants::Icons::Bookmarks),
                              iconRect.toAlignedRect(), Qt::AlignCenter, QPalette::HighlightedText);
    }

    painter->restore();
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

QRect columnTextRect(const QStyleOptionViewItem& option, bool hasIcon, bool hasSavedBadge)
{
    QRect rect{option.rect};

    const QStyle* style  = option.widget ? option.widget->style() : QApplication::style();
    const int margin     = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &option, option.widget);
    const auto rectRight = hasIcon ? columnIconRect(option).right() : option.rect.left();

    rect.setLeft(rectRight + (2 * ColumnIconPadding));
    rect.adjust(margin, 0, -margin, 0);
    if(hasSavedBadge && !hasIcon) {
        rect.setRight(std::min(rect.right(), savedBadgeRectForColumn(option, hasIcon).left() - ColumnIconPadding));
    }

    return rect;
}

void drawColumnItem(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index, const QIcon& icon)
{
    QStyleOptionViewItem opt{option};
    opt.text.clear();

    const bool hasIcon       = !icon.isNull();
    const bool hasSavedBadge = index.column() == Station && index.data(RadioBrowserModel::SavedStationRole).toBool();

    if(hasIcon) {
        opt.icon = {};
        opt.features &= ~(QStyleOptionViewItem::HasDisplay | QStyleOptionViewItem::HasDecoration);
    }

    painter->save();

    const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    if(hasIcon) {
        drawIconInRect(painter, opt, icon, columnIconRect(opt));
    }

    const QString text = index.data(Qt::DisplayRole).toString();
    drawElidedText(painter, opt, columnTextRect(opt, hasIcon, hasSavedBadge), text, Qt::AlignLeft, QPalette::Text,
                   opt.font);
    if(hasSavedBadge) {
        drawSavedBadge(painter, opt, savedBadgeRectForColumn(opt, hasIcon));
    }

    painter->restore();
}
} // namespace

bool RadioStationDelegate::uniformStationIcons() const
{
    return m_uniformStationIcons;
}

void RadioStationDelegate::setUniformStationIcons(bool enabled)
{
    m_uniformStationIcons = enabled;
}

int RadioStationDelegate::iconItemBorderWidth() const
{
    return m_iconItemBorderWidth;
}

void RadioStationDelegate::setIconItemBorderWidth(const int width)
{
    m_iconItemBorderWidth = std::max(0, width);
}

void RadioStationDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);
    // initStyleOption can replace the configured decoration size with the icon's actual size
    opt.decorationSize = option.decorationSize;

    const auto* iconView = iconModeView(option);
    const auto icon      = index.data(Qt::DecorationRole).value<QIcon>();
    const bool hasIcon   = !icon.isNull();

    if(!iconView) {
        drawColumnItem(painter, opt, index, icon);
        return;
    }

    const auto captions           = iconView->captionDisplay();
    const QRect textRect          = iconTextRect(opt, captions, hasIcon);
    const QVariant iconBackground = index.data(RadioBrowserModel::IconBackgroundRole);
    const bool hasSavedBadge      = index.data(RadioBrowserModel::SavedStationRole).toBool();

    if(hasIcon) {
        if(iconBackground.canConvert<QBrush>()) {
            opt.backgroundBrush = iconBackground.value<QBrush>();
        }
        else if(iconBackground.canConvert<QColor>()) {
            opt.backgroundBrush = iconBackground.value<QColor>();
        }
    }

    if(hasIcon && m_uniformStationIcons) {
        opt.icon = {};
    }

    opt.text.clear();

    painter->save();

    if(hasIcon) {
        drawIconItemBackground(painter, &opt, m_iconItemBorderWidth);
    }

    const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    if(hasIcon && m_uniformStationIcons) {
        drawIcon(painter, opt, icon, captions);
    }
    if(hasSavedBadge) {
        drawSavedBadge(painter, opt, savedBadgeRectForIconItem(opt, captions, hasIcon));
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
