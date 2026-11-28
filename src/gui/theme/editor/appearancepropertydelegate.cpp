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

#include "appearancepropertydelegate.h"

#include "appearancepropertymodel.h"
#include "widgets/swatchbutton.h"

#include <gui/brushsource.h>
#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <gui/widgets/brushsourcebutton.h>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QToolButton>

#include <memory>
#include <optional>
#include <utility>

namespace Fooyin {
namespace {
class PropertyEditor : public QWidget
{
    Q_OBJECT

public:
    explicit PropertyEditor(QWidget* parent)
        : QWidget{parent}
        , reset{new QToolButton(this)}
    {
        setAutoFillBackground(true);
        setBackgroundRole(QPalette::Base);

        reset->setIconSize(QSize{12, 12});
        reset->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
        reset->setFocusPolicy(Qt::TabFocus);
    }

    struct Edit
    {
        int role;
        QVariant value;
    };

    QWidget* input{nullptr};
    QToolButton* reset;
    QToolButton* inheritance{nullptr};
    std::optional<Edit> pendingEdit;
};
} // namespace

QWidget* AppearancePropertyDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& /*option*/,
                                                  const QModelIndex& index) const
{
    auto* editor = new PropertyEditor(parent);
    auto* self   = const_cast<AppearancePropertyDelegate*>(this);

    const auto entry = index.data(AppearancePropertyModel::PropertyRole).value<AppearanceProperty>();
    switch(entry.type) {
        case AppearanceProperty::Type::Group: {
            auto* summary = new QLabel(editor);
            summary->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            summary->setFocusPolicy(Qt::StrongFocus);
            editor->input = summary;
            break;
        }
        case AppearanceProperty::Type::Number: {
            auto* number = new QDoubleSpinBox(editor);
            number->setFrame(false);
            number->setDecimals(entry.decimals);
            number->setRange(entry.minimum, entry.maximum);
            number->setSingleStep(entry.decimals == 0 ? 1.0 : 0.5);
            number->setSuffix(entry.suffix);
            number->setSpecialValueText(entry.specialValueText);

            QObject::connect(number, &QDoubleSpinBox::valueChanged, self,
                             [self, editor]() { Q_EMIT self->commitData(editor); });

            number->findChild<QLineEdit*>()->installEventFilter(self);
            editor->input = number;
            break;
        }
        case AppearanceProperty::Type::Colour:
        case AppearanceProperty::Type::Brush: {
            auto* colour = new BrushSourceButton(editor, entry.type == AppearanceProperty::Type::Brush);
            QObject::connect(colour, &BrushSourceButton::valueChanged, self,
                             [self, editor] { Q_EMIT self->commitData(editor); });
            struct BrushEditSnapshot
            {
                QJsonValue overrideValue;
                BrushSource effectiveValue;
            };
            const auto snapshot = std::make_shared<BrushEditSnapshot>();
            const QPersistentModelIndex target{index};
            QObject::connect(colour, &BrushSourceButton::editingStarted, editor, [target, colour, snapshot] {
                snapshot->overrideValue  = target.data(AppearancePropertyModel::OverrideValueRole).value<QJsonValue>();
                snapshot->effectiveValue = colour->value();
            });
            QObject::connect(colour, &BrushSourceButton::editingFinished, self,
                             [target, colour, snapshot, self, editor](bool accepted) {
                                 if(target.isValid() && (!accepted || colour->value() == snapshot->effectiveValue)) {
                                     editor->pendingEdit
                                         = PropertyEditor::Edit{.role  = AppearancePropertyModel::OverrideValueRole,
                                                                .value = QVariant::fromValue(snapshot->overrideValue)};
                                     Q_EMIT self->commitData(editor);
                                 }
                             });
            colour->focusProxy()->installEventFilter(self);
            editor->input = colour;
            break;
        }
        case AppearanceProperty::Type::Choice: {
            auto* combo = new QComboBox(editor);
            combo->setFrame(false);
            for(const auto& [label, value] : entry.choices) {
                combo->addItem(label, value);
            }
            QObject::connect(combo, &QComboBox::currentIndexChanged, self,
                             [self, editor]() { Q_EMIT self->commitData(editor); });
            editor->input = combo;
            break;
        }
        case AppearanceProperty::Type::Boolean: {
            auto* check = new QCheckBox(editor);
            QObject::connect(check, &QCheckBox::toggled, self, [self, editor]() { Q_EMIT self->commitData(editor); });
            editor->input = check;
            break;
        }
        case AppearanceProperty::Type::OptionalBoolean:
        case AppearanceProperty::Type::FontBold: {
            auto* check = new QCheckBox(editor);
            check->setTristate(true);
            QObject::connect(check, &QCheckBox::clicked, self, [self, editor]() { Q_EMIT self->commitData(editor); });
            editor->input = check;
            break;
        }
    }

    editor->input->setToolTip(entry.toolTip);
    editor->input->installEventFilter(self);
    editor->reset->installEventFilter(self);

    const auto resetText
        = (entry.inheritanceGroups.empty() ? tr("Reset ‘%1’ to default") : tr("Reset ‘%1’ to inherited value"))
              .arg(entry.label);
    editor->reset->setToolTip(resetText);

    QObject::connect(editor->reset, &QToolButton::clicked, self, [self, editor]() {
        editor->pendingEdit = PropertyEditor::Edit{.role = AppearancePropertyModel::ResetRole, .value = {}};
        Q_EMIT self->commitData(editor);
        editor->input->setFocus();
    });

    auto* layout = new QHBoxLayout(editor);
    layout->setContentsMargins({});
    layout->setSpacing(2);
    layout->addWidget(editor->input, 1);

    if(index.data(AppearancePropertyModel::CanInheritRole).toBool()) {
        editor->inheritance = new QToolButton(editor);
        editor->inheritance->setCheckable(true);
        editor->inheritance->setFocusPolicy(Qt::TabFocus);
        editor->inheritance->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
        editor->inheritance->setIconSize(QSize{12, 12});
        editor->inheritance->installEventFilter(self);

        QObject::connect(editor->inheritance, &QToolButton::clicked, self, [self, editor] {
            editor->pendingEdit = PropertyEditor::Edit{.role = AppearancePropertyModel::PinRole, .value = {}};
            Q_EMIT self->commitData(editor);
            editor->input->setFocus();
        });

        layout->addWidget(editor->inheritance);
    }

    layout->addWidget(editor->reset);

    editor->setFocusProxy(editor->input);
    return editor;
}

void AppearancePropertyDelegate::setEditorData(QWidget* widget, const QModelIndex& index) const
{
    const auto* editor = static_cast<PropertyEditor*>(widget);

    const QSignalBlocker blocker{editor->input};

    if(auto* summary = qobject_cast<QLabel*>(editor->input)) {
        summary->setText(index.data(Qt::DisplayRole).toString());
        summary->setToolTip(index.data(Qt::ToolTipRole).toString());
    }
    else if(auto* number = qobject_cast<QDoubleSpinBox*>(editor->input)) {
        if(number->value() != index.data(Qt::EditRole).toDouble()) {
            number->setValue(index.data(Qt::EditRole).toDouble());
        }
    }
    else if(auto* colour = qobject_cast<BrushSourceButton*>(editor->input)) {
        const auto value = readBrushSource(QJsonValue::fromVariant(index.data(Qt::EditRole)), {});
        if(value) {
            const auto entry = index.data(AppearancePropertyModel::PropertyRole).value<AppearanceProperty>();
            colour->setValue(*value, index.data(AppearancePropertyModel::ColourPaletteRole).value<QPalette>(),
                             entry.colourGroup);
        }
    }
    else if(auto* combo = qobject_cast<QComboBox*>(editor->input)) {
        combo->setCurrentIndex(combo->findData(index.data(Qt::EditRole)));
    }
    else if(auto* check = qobject_cast<QCheckBox*>(editor->input)) {
        check->setCheckState(static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt()));
    }

    Gui::setThemeIcon(editor->reset, Constants::Icons::Reset);
    editor->reset->setEnabled(index.data(AppearancePropertyModel::ResetRole).toBool());

    if(editor->inheritance) {
        const bool inherited = index.data(AppearancePropertyModel::InheritedRole).toBool();
        editor->inheritance->setChecked(inherited);
        editor->inheritance->setIcon(Gui::iconFromTheme(Constants::Icons::Inherited, editor->palette()));
        editor->inheritance->setToolTip(inherited ? tr("Keep this value independent of Normal")
                                                  : tr("Use the inherited value"));
    }
}

void AppearancePropertyDelegate::setModelData(QWidget* widget, QAbstractItemModel* model,
                                              const QModelIndex& index) const
{
    auto* editor = static_cast<PropertyEditor*>(widget);

    // Consume the request before model notifications can update the editor or trigger another commit.
    if(const auto edit = std::exchange(editor->pendingEdit, std::nullopt)) {
        model->setData(index, edit->value, edit->role);
        return;
    }

    if(const auto* number = qobject_cast<QDoubleSpinBox*>(editor->input)) {
        model->setData(index, number->value(), Qt::EditRole);
    }
    else if(const auto* colour = qobject_cast<BrushSourceButton*>(editor->input)) {
        model->setData(index, brushSourceToJson(colour->value()).toVariantMap(), Qt::EditRole);
    }
    else if(const auto* combo = qobject_cast<QComboBox*>(editor->input)) {
        model->setData(index, combo->currentData(), Qt::EditRole);
    }
    else if(const auto* check = qobject_cast<QCheckBox*>(editor->input)) {
        model->setData(index, check->checkState(), Qt::CheckStateRole);
    }
}

void AppearancePropertyDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                                                      const QModelIndex& /*index*/) const
{
    editor->setGeometry(option.rect.adjusted(0, 0, 0, -1));
}

QSize AppearancePropertyDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    return QStyledItemDelegate::sizeHint(option, index).expandedTo(QSize{0, option.fontMetrics.height() + 10});
}

void AppearancePropertyDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                       const QModelIndex& index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    auto colour = option.palette.color(QPalette::Mid);
    colour.setAlphaF(colour.alphaF() * 0.2F);

    painter->save();
    painter->setPen(colour);
    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
    if(index.column() == 0) {
        painter->drawLine(option.rect.topRight(), option.rect.bottomRight());
    }
    painter->restore();
}

bool AppearancePropertyDelegate::eventFilter(QObject* watched, QEvent* event)
{
    const auto* view = qobject_cast<QAbstractItemView*>(parent());
    if(!view) {
        return false;
    }

    auto* widget = qobject_cast<QWidget*>(watched);
    while(widget && widget->parentWidget() != view->viewport()) {
        widget = widget->parentWidget();
    }
    if(!widget) {
        return QStyledItemDelegate::eventFilter(watched, event);
    }

    auto* editor = qobject_cast<PropertyEditor*>(widget);
    if(!editor) {
        return false;
    }

    const auto* colour = qobject_cast<BrushSourceButton*>(editor->input);
    if(event->type() == QEvent::FocusOut && colour && colour->choosingSource()) {
        return false;
    }

    if(event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        const bool backwards
            = key->key() == Qt::Key_Backtab || (key->key() == Qt::Key_Tab && (key->modifiers() & Qt::ShiftModifier));
        if(backwards && watched == editor->reset) {
            (editor->inheritance ? static_cast<QWidget*>(editor->inheritance) : editor->input)
                ->setFocus(Qt::BacktabFocusReason);
            return true;
        }
        if(backwards && watched == editor->inheritance) {
            editor->input->setFocus(Qt::BacktabFocusReason);
            return true;
        }
        if(key->key() == Qt::Key_Tab && !backwards && watched != editor->reset) {
            if(editor->inheritance && watched != editor->inheritance) {
                editor->inheritance->setFocus(Qt::TabFocusReason);
                return true;
            }
            if(editor->reset->isEnabled()) {
                editor->reset->setFocus(Qt::TabFocusReason);
                return true;
            }
        }
    }

    return QStyledItemDelegate::eventFilter(editor, event);
}

void AppearancePropertyDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
{
    QStyledItemDelegate::initStyleOption(option, index);
    if(index.column() != 1) {
        return;
    }

    const auto entry     = index.data(AppearancePropertyModel::PropertyRole).value<AppearanceProperty>();
    const bool inherited = index.data(AppearancePropertyModel::InheritedRole).toBool();
    const auto role      = option->state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text;
    const double ratio   = option->widget ? option->widget->devicePixelRatioF() : 1.0;

    QIcon inheritanceIcon;
    if(inherited) {
        QPixmap pixmap{QSize{12, 12} * ratio};
        pixmap.setDevicePixelRatio(ratio);
        pixmap.fill(Qt::transparent);

        QPainter painter{&pixmap};
        painter.setOpacity(0.65);

        const auto icon = Gui::iconFromTheme(Constants::Icons::Inherited, option->palette);
        Gui::drawItemViewIcon(&painter, *option, icon, QRect{0, 0, 12, 12}, Qt::AlignCenter, role);
        painter.end();
        inheritanceIcon = QIcon{pixmap};
    }

    if(entry.type == AppearanceProperty::Type::Colour || entry.type == AppearanceProperty::Type::Brush) {
        const auto colour = readBrushSource(QJsonValue::fromVariant(index.data(Qt::EditRole)), {});
        if(colour) {
            const QSize size{inherited ? 32 : 16, 12};

            QPixmap pixmap{size * ratio};
            pixmap.setDevicePixelRatio(ratio);
            pixmap.fill(Qt::transparent);

            QPainter painter{&pixmap};
            brushSourceIcon(*colour, index.data(AppearancePropertyModel::ColourPaletteRole).value<QPalette>(),
                            entry.colourGroup, ratio)
                .paint(&painter, QRect{0, 0, 16, 12});
            if(inherited) {
                inheritanceIcon.paint(&painter, QRect{20, 0, 12, 12});
            }
            option->icon               = QIcon{pixmap};
            option->decorationSize     = size;
            option->decorationPosition = QStyleOptionViewItem::Left;
        }
    }
    else if(inherited) {
        option->icon               = std::move(inheritanceIcon);
        option->decorationSize     = QSize{12, 12};
        option->decorationPosition = QStyleOptionViewItem::Right;
    }

    if(!option->icon.isNull()) {
        option->features |= QStyleOptionViewItem::HasDecoration;
    }
}
} // namespace Fooyin

#include "appearancepropertydelegate.moc"
#include "moc_appearancepropertydelegate.cpp"
