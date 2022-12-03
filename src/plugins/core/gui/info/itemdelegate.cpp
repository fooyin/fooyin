#include "itemdelegate.h"

#include "core/gui/info/infoitem.h"

#include <QPainter>
#include <utils/typedefs.h>

ItemDelegate::ItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{ }

ItemDelegate::~ItemDelegate() = default;

QSize ItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto text = index.data(Qt::DisplayRole).toString();
    auto height = option.fontMetrics.height();

    QFont textFont = option.font;
    // Set font slightly larger than actual to eliminate clipping when resizing
    textFont.setPixelSize(15);

    QFontMetrics fm{textFont};
    int width = fm.boundingRect(text).width();

    const auto type = index.data(Role::Type).value<InfoItem::Type>();

    switch(type) {
        case(InfoItem::Type::Header): {
            height += 13;
            break;
        }
        case(InfoItem::Type::Entry): {
            height += 7;
            break;
        }
    }
    return {width, height};
}

void ItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();

    QStyleOptionViewItem opt = option;
    const auto type = index.data(Role::Type).value<InfoItem::Type>();

    initStyleOption(&opt, index);

    QFont font = painter->font();
    font.setBold(true);
    font.setPixelSize(12);
    painter->setFont(font);

    switch(type) {
        case(InfoItem::Type::Header): {
            paintHeader(painter, option, index);
            break;
        }
        case(InfoItem::Type::Entry): {
            paintEntry(painter, option, index);
            break;
        }
    }
    painter->restore();
}

void ItemDelegate::paintHeader(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    const int x = option.rect.x();
    const int y = option.rect.y();
    const int width = option.rect.width();
    const int height = option.rect.height();
    const int right = x + width;
    const int centreY = y + (height / 2);

    QPen linePen = painter->pen();
    linePen.setWidth(1);
    QColor lineColour = option.palette.color(QPalette::BrightText);
    lineColour.setAlpha(40);
    linePen.setColor(lineColour);

    const QString title = index.data(Qt::DisplayRole).toString();

    QFont titleFont = painter->font();
    titleFont.setPixelSize(13);

    const QRect titleRect = QRect(x + 7, y, width, height);

    painter->setFont(titleFont);
    option.widget->style()->drawItemText(painter, titleRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
                                         painter->fontMetrics().elidedText(title, Qt::ElideRight, titleRect.width()));

    if(!title.isEmpty()) {
        painter->setPen(linePen);
        const QRect titleBound = painter->boundingRect(titleRect, Qt::AlignLeft | Qt::AlignVCenter, title);
        const QLineF headerLine((titleBound.right() + 10), centreY, (right - 10), centreY);
        painter->drawLine(headerLine);
    }
}

void ItemDelegate::paintEntry(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    const int x = option.rect.x();
    const int y = option.rect.y();
    const int width = option.rect.width();
    const int height = option.rect.height();

    const QString title = index.data(Qt::DisplayRole).toString();

    QFont titleFont = painter->font();
    titleFont.setPixelSize(12);

    const QRect titleRect = QRect(x + 8, y, width, height);

    painter->setFont(titleFont);
    option.widget->style()->drawItemText(painter, titleRect, Qt::AlignLeft | Qt::AlignVCenter, option.palette, true,
                                         painter->fontMetrics().elidedText(title, Qt::ElideRight, titleRect.width()));
}
