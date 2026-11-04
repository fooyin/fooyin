/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <luket@pm.me>
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

#include "playlistdelegate.h"

#include "playlistitem.h"
#include "playlistview.h"
#include "scripting/richtextlayout.h"

#include <gui/guiutils.h>
#include <gui/iconloader.h>
#include <gui/scripting/richtextutils.h>
#include <gui/widgets/expandedtreeview.h>
#include <gui/widgets/metadatacompleter.h>
#include <utils/utils.h>

#include <QApplication>
#include <QHeaderView>
#include <QLineEdit>
#include <QPainter>

constexpr auto EditorInitialisedProperty = "fy_editor_initialised";

namespace Fooyin {
QWidget* PlaylistDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                        const QModelIndex& index) const
{
    auto* editor = qobject_cast<QLineEdit*>(QStyledItemDelegate::createEditor(parent, option, index));
    if(!editor) {
        editor = new QLineEdit(parent);
    }

    const QString writeField = index.data(PlaylistItem::Role::EditableWriteField).toString();
    if(writeField.isEmpty()) {
        return editor;
    }

    const auto playlistTrack = index.data(PlaylistItem::Role::ItemData).value<PlaylistTrack>();
    if(playlistTrack.track.metadataStore()) {
        Gui::setMetadataCompleter(editor, writeField, *playlistTrack.track.metadataStore());
    }

    return editor;
}

std::vector<InlineTrackEditorHit> drawSingleColumnTrack(QPainter* painter, const QStyleOptionViewItem& option,
                                                        const QModelIndex& index, const InlineTrackEditors* editors)
{
    const QIcon playingIcon{index.data(PlaylistItem::Role::PlayingIcon).value<QPixmap>()};
    const auto leftSide  = index.data(PlaylistItem::Role::Left).value<RichText>().blocks;
    const auto rightSide = index.data(PlaylistItem::Role::Right).value<RichText>().blocks;
    const QRect textRect = Gui::itemViewTextRect(option);

    const QRect rightRect = textRect.adjusted(textRect.center().x() - textRect.left(), 0, 0, 0);
    auto right           = drawTrackTextBlocks(painter, option, rightRect, rightSide, Qt::AlignVCenter | Qt::AlignRight,
                                               playingIcon, editors);
    const QRect leftRect = textRect.adjusted(0, 0, -right.totalWidth, 0);
    auto left = drawTrackTextBlocks(painter, option, leftRect, leftSide, Qt::AlignVCenter | Qt::AlignLeft, playingIcon,
                                    editors);
    left.editors.insert(left.editors.end(), right.editors.begin(), right.editors.end());
    return left.editors;
}

void paintHeader(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index, int cornerRadius)
{
    QStyleOptionViewItem opt{option};
    opt.text.clear();
    opt.icon = {};

    const auto text              = index.data(PlaylistItem::Role::Title).value<RichText>();
    const auto cover             = index.data(Qt::DecorationRole).value<QPixmap>();
    const auto horizontalPadding = std::max(0, index.data(PlaylistItem::Role::ImagePadding).toInt());
    const auto verticalPadding   = std::max(0, index.data(PlaylistItem::Role::ImagePaddingTop).toInt());

    const QRect& rect           = opt.rect;
    static constexpr int offset = 5;

    const int coverSize
        = std::max(0, std::min(rect.height() - (2 * verticalPadding), rect.width() - (2 * horizontalPadding)));
    const bool drawCover = !cover.isNull() && coverSize > 0;

    const QRect coverRect{rect.left() + horizontalPadding, rect.top() + ((rect.height() - coverSize) / 2), coverSize,
                          coverSize};
    const QRect coverFrameRect = coverRect.adjusted(-1, -1, 1, 1);
    const int contentLeft      = drawCover ? coverFrameRect.right() + horizontalPadding : rect.left() + offset;
    const QRect textRect{contentLeft, rect.top() + 4, std::max(0, rect.right() - offset - contentLeft + 1),
                         std::max(0, rect.height() - 8)};

    drawRichTextLayout(painter, opt, textRect, prepareRichTextLayout(text, opt.font));

    if(drawCover) {
        QPen coverPen     = painter->pen();
        QColor coverColor = opt.palette.color(QPalette::Text);
        coverColor.setAlpha(40);
        coverPen.setColor(coverColor);
        coverPen.setWidth(1);
        painter->setPen(coverPen);

        painter->setRenderHint(QPainter::Antialiasing);
        const qreal frameRadius
            = std::min(coverFrameRect.width(), coverFrameRect.height()) * std::clamp(cornerRadius, 0, 100) / 200.0;
        painter->drawRoundedRect(coverFrameRect, frameRadius, frameRadius);

        const double dpr = opt.widget ? opt.widget->devicePixelRatioF() : 1.0;
        Gui::drawRoundedPixmap(*painter, coverRect, Qt::AlignCenter, Utils::scalePixmap(cover, coverSize, dpr, true),
                               cornerRadius);
    }
}

void paintSubheader(QPainter* painter, const QStyleOptionViewItem& opt, const QModelIndex& index)
{
    const auto text  = index.data(PlaylistItem::Role::Title).value<RichText>();
    const QRect rect = PlaylistDelegate::subheaderContentRect(opt, index, 5);
    drawRichTextLayout(painter, opt, rect, prepareRichTextLayout(text, opt.font));
}

void paintTrack(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index,
                int cornerRadiusPercent, const PlaylistDelegate::ReflectionOptions& reflectionOptions)
{
    QStyleOptionViewItem opt{option};

    const bool singleColumn = index.data(PlaylistItem::Role::SingleColumnMode).toBool();
    const QIcon playingIcon{index.data(PlaylistItem::Role::PlayingIcon).value<QPixmap>()};

    if(singleColumn) {
        const auto editorData = index.data(PlaylistItem::Role::InlineEditors);
        if(editorData.isValid()) {
            const auto* view   = qobject_cast<const PlaylistView*>(opt.widget);
            const auto editors = view ? view->inlineEditors(index) : editorData.value<InlineTrackEditors>();
            drawSingleColumnTrack(painter, opt, index, &editors);
        }
        else {
            drawSingleColumnTrack(painter, opt, index, nullptr);
        }
    }
    else {
        if(index.data(PlaylistItem::Role::Column).canConvert<QPixmap>()) {
            const auto image = index.data(PlaylistItem::Role::Column).value<QPixmap>();

            if(!image.isNull()) {
                const auto imagePadding    = index.data(PlaylistItem::Role::ImagePadding).toInt();
                const auto imagePaddingTop = index.data(PlaylistItem::Role::ImagePaddingTop).toInt();

                opt.rect.adjust(imagePadding, imagePaddingTop, -imagePadding, imagePaddingTop);

                const QPixmap scaledImage
                    = Utils::scalePixmap(image, opt.rect.width(), opt.widget->devicePixelRatioF(), true);
                Gui::drawRoundedPixmap(*painter, opt.rect, Qt::AlignHCenter | Qt::AlignTop, scaledImage,
                                       cornerRadiusPercent);

                if(const auto* view = qobject_cast<const ExpandedTreeView*>(opt.widget)) {
                    if(reflectionOptions.enabled && reflectionOptions.opacity > 0 && view->isSpanning(index.column())
                       && !scaledImage.isNull()) {
                        const QRect artworkRect
                            = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignHCenter | Qt::AlignTop,
                                                  scaledImage.deviceIndependentSize().toSize(), opt.rect);
                        Gui::drawArtworkReflection(*painter, image, scaledImage, artworkRect, cornerRadiusPercent,
                                                   reflectionOptions);
                    }
                }
            }
        }
        else {
            const QRect textRect = Gui::itemViewTextRect(opt);

            const auto columnText = index.data(PlaylistItem::Role::Column).value<RichText>().blocks;

            drawTrackTextBlocks(painter, opt, textRect, columnText, Qt::AlignVCenter | opt.displayAlignment,
                                playingIcon);
        }
    }
}

void PlaylistDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    if(auto* lineEdit = qobject_cast<QLineEdit*>(editor)) {
        const bool initialised = lineEdit->property(EditorInitialisedProperty).toBool();
        if(initialised && lineEdit->hasFocus()) {
            // Prevent track updates from updating the text in the editor
            return;
        }

        QStyledItemDelegate::setEditorData(editor, index);
        lineEdit->setProperty(EditorInitialisedProperty, true);
        return;
    }

    QStyledItemDelegate::setEditorData(editor, index);
}

void PlaylistDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    opt.showDecorationSelected = true;

    const auto type = index.data(PlaylistItem::Type).toInt();
    switch(type) {
        case PlaylistItem::Track:
            paintTrack(painter, opt, index, m_artworkCornerRadius, m_reflectionOptions);
            break;
        case PlaylistItem::Header: {
            paintHeader(painter, opt, index, m_artworkCornerRadius);
            break;
        }
        case PlaylistItem::Subheader:
            paintSubheader(painter, opt, index);
            break;
        default:
            break;
    }

    painter->restore();
}

QSize PlaylistDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // Prevents the current playing row being larger than the rest
    opt.icon           = {};
    opt.decorationSize = {};

    const QWidget* widget = opt.widget;
    const QStyle* style   = widget ? widget->style() : QApplication::style();
    const int type        = index.data(PlaylistItem::Type).toInt();

    QSize size = index.data(Qt::SizeHintRole).toSize();
    const bool usesStyleBaseHeight
        = (type == PlaylistItem::Track && index.data(PlaylistItem::Role::UsesStyleBaseHeight).toBool());

    const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, opt.widget) * 5;
    const QSize hint = usesStyleBaseHeight ? Gui::itemViewTextSizeHint(opt, {size.width(), opt.fontMetrics.height()})
                                           : style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, size, widget);

    if(size.width() <= 0) {
        size.setWidth(hint.width());
    }
    if(usesStyleBaseHeight) {
        size.setHeight(hint.height() + size.height());
    }
    else if(size.height() < 1) {
        size.setHeight(hint.height());
    }

    size.setWidth(size.width() + margin);

    return size;
}

void PlaylistDelegate::setArtworkCornerRadius(int radius)
{
    m_artworkCornerRadius = std::clamp(radius, 0, 100);
}

void PlaylistDelegate::setArtworkReflection(const ReflectionOptions& options)
{
    m_reflectionOptions = options;
}

InlineTrackEditorHit PlaylistDelegate::inlineEditorAt(const QStyleOptionViewItem& option, const QModelIndex& index,
                                                      const QPoint& pos) const
{
    if(index.data(PlaylistItem::Type).toInt() != PlaylistItem::Track
       || !index.data(PlaylistItem::Role::SingleColumnMode).toBool()) {
        return {};
    }

    const auto editorData = index.data(PlaylistItem::Role::InlineEditors);
    if(!editorData.isValid()) {
        return {};
    }

    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);
    const auto editors = editorData.value<InlineTrackEditors>();
    return inlineTrackEditorAt(drawSingleColumnTrack(nullptr, opt, index, &editors), pos);
}

int PlaylistDelegate::artworkSpanHeight(int width, const QModelIndex& index) const
{
    if(!m_reflectionOptions.enabled || index.data(PlaylistItem::Type).toInt() != PlaylistItem::Track) {
        return width;
    }

    const int padding     = index.data(PlaylistItem::Role::ImagePadding).toInt();
    const int topPadding  = index.data(PlaylistItem::Role::ImagePaddingTop).toInt();
    const int coverHeight = std::max(0, width - (2 * padding));
    if(coverHeight <= 0) {
        return width;
    }

    const int reflectionHeight = Gui::artworkReflectionHeight(coverHeight, m_reflectionOptions);
    return std::max(width, topPadding + coverHeight + m_reflectionOptions.gap + reflectionHeight);
}

QRect PlaylistDelegate::subheaderContentRect(const QStyleOptionViewItem& option, const QModelIndex& index, int offset)
{
    QRect rect = option.rect.adjusted(offset, 0, -offset, 0);

    const auto* view   = qobject_cast<const PlaylistView*>(option.widget);
    const auto* header = view->header();
    if(!view || !header) {
        return rect;
    }

    const bool insetSubheaders = index.data(PlaylistItem::Role::InsetSubheadersToImageColumns).toBool();
    if(!insetSubheaders && !view->stickyArtworkIntersects(option.rect)) {
        return rect;
    }

    const auto shouldInsetColumn = [view, header, insetSubheaders, &option](int column) {
        if(column < 0 || !view->isSpanning(column)) {
            return false;
        }

        const QRect columnRect{header->sectionViewportPosition(column), option.rect.top(), header->sectionSize(column),
                               option.rect.height()};
        return insetSubheaders || view->stickyArtworkIntersects(columnRect);
    };

    int leftLogical{-1};
    int leftPos{std::numeric_limits<int>::max()};
    int leftSize{0};

    int rightLogical{-1};
    int rightPos{std::numeric_limits<int>::min()};

    for(int logical{0}; logical < header->count(); ++logical) {
        if(header->isSectionHidden(logical)) {
            continue;
        }

        const int pos  = header->sectionViewportPosition(logical);
        const int size = header->sectionSize(logical);

        if(pos < leftPos) {
            leftPos     = pos;
            leftLogical = logical;
            leftSize    = size;
        }

        if(pos > rightPos) {
            rightPos     = pos;
            rightLogical = logical;
        }
    }

    if(shouldInsetColumn(leftLogical)) {
        rect.setLeft(std::max(rect.left(), leftPos + leftSize + offset));
    }

    if(shouldInsetColumn(rightLogical)) {
        rect.setRight(std::min(rect.right(), rightPos - offset));
    }

    if(rect.right() < rect.left()) {
        rect.setRight(rect.left());
    }

    return rect;
}
} // namespace Fooyin

#include "moc_playlistdelegate.cpp"
