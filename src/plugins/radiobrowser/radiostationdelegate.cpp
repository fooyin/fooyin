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
#include <gui/guiutils.h>
#include <gui/iconloader.h>
#include <gui/widgets/expandedtreeview.h>
#include <gui/widgets/widgetbackground.h>

#include <QApplication>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QStyle>

constexpr auto LineSpacing           = 2;
constexpr auto RightCaptionWidth     = 190;
constexpr auto CaptionPadding        = 8;
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

QRect iconRect(const QStyleOptionViewItem& option, ExpandedTreeView::CaptionDisplay captions, const QMargins& margins)
{
    const QSize iconSize = iconDecorationSize(option);
    const QRect rect     = option.rect.marginsRemoved(margins);

    switch(captions) {
        case ExpandedTreeView::CaptionDisplay::Right:
            return QStyle::alignedRect(option.direction, Qt::AlignLeft | Qt::AlignVCenter, iconSize, rect);
        case ExpandedTreeView::CaptionDisplay::Bottom:
        case ExpandedTreeView::CaptionDisplay::None:
        case ExpandedTreeView::CaptionDisplay::Overlay:
            return QStyle::alignedRect(option.direction, Qt::AlignTop | Qt::AlignHCenter, iconSize, rect);
    }

    return {};
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

QRect iconTextRect(const QStyleOptionViewItem& option, ExpandedTreeView::CaptionDisplay captions,
                   const QMargins& margins)
{
    QRect rect{option.rect};

    const QStyle* style         = option.widget ? option.widget->style() : QApplication::style();
    const int horizontalPadding = std::max(4, style->pixelMetric(QStyle::PM_FocusFrameHMargin, &option, option.widget));
    const int verticalPadding   = std::max(2, style->pixelMetric(QStyle::PM_FocusFrameVMargin, &option, option.widget));

    switch(captions) {
        case ExpandedTreeView::CaptionDisplay::Right:
            rect.adjust(CaptionPadding, CaptionPadding, -CaptionPadding, -CaptionPadding);
            if(option.direction == Qt::RightToLeft) {
                rect.setRight(iconRect(option, captions, margins).left() - margins.left() - CaptionPadding - 1);
            }
            else {
                rect.setLeft(iconRect(option, captions, margins).right() + margins.right() + CaptionPadding + 1);
            }
            break;
        case ExpandedTreeView::CaptionDisplay::Bottom:
            rect.setTop(rect.top() + margins.top() + iconDecorationSize(option).height() + margins.bottom()
                        + verticalPadding);
            rect.adjust(horizontalPadding, 0, -horizontalPadding, -verticalPadding);
            break;
        case ExpandedTreeView::CaptionDisplay::None:
        case ExpandedTreeView::CaptionDisplay::Overlay:
            rect = {};
            break;
    }

    return rect;
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

void drawIconInRect(QPainter* painter, const QStyleOptionViewItem& option, const QIcon& icon, const QRect& target,
                    int iconRadius, bool uniformSize)
{
    if(target.isEmpty()) {
        return;
    }

    QIcon::Mode mode = Gui::itemViewIconMode(option);
    if(uniformSize) {
        mode = option.state & QStyle::State_Enabled ? QIcon::Normal : QIcon::Disabled;
    }

    const qreal dpr  = option.widget ? option.widget->devicePixelRatioF() : qApp->devicePixelRatio();
    const auto state = option.state & QStyle::State_Open ? QIcon::On : QIcon::Off;
    QPixmap pixmap   = icon.pixmap(target.size(), dpr, mode, state);
    if(pixmap.isNull()) {
        return;
    }

    if(uniformSize) {
        pixmap = pixmap.scaled(target.size() * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pixmap.setDevicePixelRatio(dpr);

        const auto stats = pixmapStats(pixmap);
        if(stats.transparentRatio > 0.2) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(iconBackdropColor(option, stats));
            const qreal radius = std::min(target.width(), target.height()) * iconRadius / 200.0;
            painter->drawRoundedRect(target, radius, radius);
            painter->restore();
        }
    }

    Gui::drawRoundedPixmap(*painter, target, Qt::AlignCenter, pixmap, iconRadius);
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
                                bool hasIcon, const QMargins& margins)
{
    if(hasIcon && captions == ExpandedTreeView::CaptionDisplay::Bottom) {
        return savedBadgeRect(iconRect(option, captions, margins));
    }

    const int size = savedBadgeSizeForAnchor(hasIcon ? iconRect(option, captions, margins) : option.rect);
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
    const QColor colour
        = option.palette.color((option.state & QStyle::State_Selected) ? Gui::itemViewSelectionTextRole(option) : role);

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

QSize iconItemSize(const QStyleOptionViewItem& option, ExpandedTreeView::CaptionDisplay captions, int captionLines,
                   const QMargins& margins)
{
    const QSize artworkSize
        = iconDecorationSize(option) + QSize{margins.left() + margins.right(), margins.top() + margins.bottom()};
    const int textLines  = captions == ExpandedTreeView::CaptionDisplay::None ? 0 : std::max(1, captionLines);
    const int textHeight = captionTextHeight(option, textLines);

    const QStyle* style       = option.widget ? option.widget->style() : QApplication::style();
    const int verticalPadding = std::max(2, style->pixelMetric(QStyle::PM_FocusFrameVMargin, &option, option.widget));

    switch(captions) {
        case ExpandedTreeView::CaptionDisplay::Right:
            return {artworkSize.width() + RightCaptionWidth + (2 * CaptionPadding),
                    std::max(artworkSize.height(), textHeight + (2 * CaptionPadding))};
        case ExpandedTreeView::CaptionDisplay::Bottom:
            return {artworkSize.width(), artworkSize.height() + textHeight + (2 * verticalPadding)};
        case ExpandedTreeView::CaptionDisplay::None:
        case ExpandedTreeView::CaptionDisplay::Overlay:
            return artworkSize;
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

void drawColumnItem(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index, const QIcon& icon,
                    int iconRadius)
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
    painter->setClipRect(option.rect, Qt::IntersectClip);

    const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    if(hasIcon) {
        drawIconInRect(painter, opt, icon, columnIconRect(opt), iconRadius, true);
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

void RadioStationDelegate::setIconCornerRadius(int radius)
{
    m_iconCornerRadius = std::clamp(radius, 0, 100);
}

void RadioStationDelegate::setIconMargins(const QMargins& margins)
{
    m_iconMargins = margins;
}

void RadioStationDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);
    // initStyleOption can replace the configured decoration size with the icon's actual size
    opt.decorationSize = option.decorationSize;
    WidgetBackground::clearDelegateBackground(opt);

    const auto* iconView = iconModeView(option);
    const auto icon      = index.data(Qt::DecorationRole).value<QIcon>();
    const bool hasIcon   = !icon.isNull();

    if(!iconView) {
        drawColumnItem(painter, opt, index, icon, m_iconCornerRadius);
        return;
    }

    const auto captions           = iconView->captionDisplay();
    const Qt::Edges squareEdges   = captions == ExpandedTreeView::CaptionDisplay::Bottom ? Qt::Edges{Qt::BottomEdge}
                                  : captions == ExpandedTreeView::CaptionDisplay::Right
                                      ? Qt::Edges{opt.direction == Qt::RightToLeft ? Qt::LeftEdge : Qt::RightEdge}
                                      : Qt::Edges{};
    QRect textRect                = iconTextRect(opt, captions, m_iconMargins);
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

    opt.icon = {};
    opt.text.clear();

    painter->save();

    if(!(opt.state & QStyle::State_Selected) && opt.backgroundBrush.style() != Qt::NoBrush) {
        painter->save();
        Gui::clipRoundedRect(*painter, opt.rect, iconView->iconItemCornerRadius(), squareEdges);
        painter->fillRect(opt.rect, opt.backgroundBrush);
        painter->restore();
        opt.backgroundBrush = Qt::NoBrush;
    }

    Gui::drawRoundedStyleControl(painter, QStyle::CE_ItemViewItem, opt, opt.widget, iconView->iconItemCornerRadius(),
                                 squareEdges);

    if(hasIcon) {
        drawIconInRect(painter, opt, icon, iconRect(opt, captions, m_iconMargins), m_iconCornerRadius,
                       m_uniformStationIcons);
    }
    if(hasSavedBadge) {
        const QRect badgeRect = savedBadgeRectForIconItem(opt, captions, hasIcon, m_iconMargins);
        drawSavedBadge(painter, opt, badgeRect);
        if(textRect.intersects(badgeRect)) {
            textRect.setRight(badgeRect.left() - SavedBadgeMargin);
        }
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
    return applyHeightOverride(iconItemSize(option, iconView->captionDisplay(), captionLines, m_iconMargins));
}
} // namespace Fooyin::RadioBrowser
