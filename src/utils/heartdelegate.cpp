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

#include <utils/heartdelegate.h>

#include <QApplication>
#include <QEvent>
#include <QPainter>

namespace Fooyin {
QModelIndex HeartDelegate::hoveredIndex() const
{
    return m_hoverIndex;
}

void HeartDelegate::setHoverIndex(const QModelIndex& index)
{
    m_hoverIndex = index;
}

void HeartDelegate::setHoverIndex(const QModelIndex& index, const QModelIndexList& selected)
{
    if(m_hoverIndex != index) {
        m_hoverLoved = !index.data().value<HeartValue>().loved();
    }
    m_hoverIndex = index;
    m_selected   = selected;
}

void HeartDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);

    const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

    if(!index.data().canConvert<HeartValue>()) {
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
        return;
    }

    auto heartValue        = index.data().value<HeartValue>();
    const bool mixedValues = index.data(MixedValues).toBool();
    const bool selected    = opt.state.testFlag(QStyle::State_Selected);

    const bool hover = m_hoverIndex.isValid()
                    && (m_hoverIndex == index || (m_selected.contains(m_hoverIndex) && m_selected.contains(index)));

    if(hover) {
        heartValue.setLoved(m_hoverLoved);
    }

    heartValue.paint(painter, opt.rect, opt.palette, HeartValue::EditMode::ReadOnly, opt.displayAlignment, selected);

    if(mixedValues && !hover) {
        painter->save();

        QFont font{opt.font};
        font.setPointSizeF(std::max(7.0, font.pointSizeF() - 1.0));
        painter->setFont(font);

        QColor textColour = selected ? opt.palette.highlightedText().color() : opt.palette.text().color();
        textColour.setAlpha(170);
        painter->setPen(textColour);

        int starX           = opt.rect.x();
        const int starWidth = heartValue.sizeHint().width();
        if(opt.displayAlignment & Qt::AlignHCenter) {
            starX += (opt.rect.width() - starWidth) / 2;
        }
        else if(opt.displayAlignment & Qt::AlignRight) {
            starX += opt.rect.width() - starWidth;
        }

        const QRect mixedRect = opt.rect.adjusted((starX - opt.rect.x()) + starWidth + 4, 0, 0, 0);
        //: Indicates that the selected tracks have different loved values in the tag editor.
        painter->drawText(mixedRect, Qt::AlignLeft | Qt::AlignVCenter, tr("mixed"));

        painter->restore();
    }
}

QSize HeartDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if(index.data().canConvert<HeartValue>()) {
        const auto heartValue = index.data().value<HeartValue>();
        return heartValue.sizeHint();
    }

    return QStyledItemDelegate::sizeHint(option, index);
}

QWidget* HeartDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& /*option*/,
                                     const QModelIndex& index) const
{
    const auto align = static_cast<Qt::Alignment>(index.data(Qt::TextAlignmentRole).toInt());
    auto* editor     = new HeartEditor(align, parent);
    QObject::connect(editor, &HeartEditor::editingFinished, this, &HeartDelegate::finishEditing);
    return editor;
}

void HeartDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    if(auto* heartEditor = qobject_cast<HeartEditor*>(editor)) {
        heartEditor->setValue(index.data().value<HeartValue>());
    }
    else {
        QStyledItemDelegate::setEditorData(editor, index);
    }
}

void HeartDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    if(const auto* heartEditor = qobject_cast<HeartEditor*>(editor)) {
        model->setData(index, QVariant::fromValue(heartEditor->loved()));
    }
}

void HeartDelegate::finishEditing()
{
    auto* editor = qobject_cast<HeartEditor*>(sender());
    Q_EMIT commitData(editor);
    Q_EMIT closeEditor(editor);
}
} // namespace Fooyin
