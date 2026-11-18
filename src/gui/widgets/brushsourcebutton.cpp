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

#include <gui/widgets/brushsourcebutton.h>

#include "brushpickerdialog.h"
#include "swatchbutton.h"

#include <QAbstractButton>
#include <QHBoxLayout>
#include <QPainter>
#include <QPointer>
#include <QResizeEvent>
#include <QStyle>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
class BrushValueButton : public SwatchButton
{
public:
    explicit BrushValueButton(QWidget* parent)
        : SwatchButton{parent}
    {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        if(m_swatchOnly) {
            return SwatchButton::sizeHint();
        }
        return QSize{fontMetrics().horizontalAdvance(text()) + 48, fontMetrics().height() + 6};
    }

    void setSwatchOnly(bool swatchOnly)
    {
        m_swatchOnly = swatchOnly;
        updateGeometry();
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        if(m_swatchOnly) {
            SwatchButton::paintEvent(event);
            return;
        }

        QPainter painter{this};
        paintBackground(painter);

        const QRect content = rect().adjusted(4, 0, -4, 0);
        const QRect swatch{content.left(), content.center().y() - 6, 16, 12};
        icon().paint(&painter, QStyle::visualRect(layoutDirection(), rect(), swatch));

        const QFontMetrics fm{fontMetrics()};

        const int editWidth = fm.horizontalAdvance(u"…"_s);
        const QRect label   = content.adjusted(22, 0, -editWidth - 6, 0);

        const auto colour = palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText);
        painter.setPen(colour);

        painter.drawText(QStyle::visualRect(layoutDirection(), rect(), label),
                         QStyle::visualAlignment(layoutDirection(), Qt::AlignVCenter | Qt::AlignLeading),
                         fm.elidedText(text(), Qt::ElideRight, std::max(0, label.width())));

        const QRect edit{content.right() - editWidth + 1, content.top(), editWidth, content.height()};
        painter.drawText(QStyle::visualRect(layoutDirection(), rect(), edit), Qt::AlignCenter, u"…"_s);
        paintFocus(painter);
    }

private:
    bool m_swatchOnly{false};
};
} // namespace

class BrushSourceButtonPrivate
{
public:
    BrushSourceButtonPrivate(BrushSourceButton* self, bool gradients)
        : m_self{self}
        , m_button{new BrushValueButton(m_self)}
        , m_palette{m_self->palette()}
        , m_gradients{gradients}
    { }

    void updateButton();
    void openPicker();

    BrushSourceButton* m_self;
    BrushValueButton* m_button;
    QPointer<BrushPickerDialog> m_dialog;
    BrushSource m_value{QPalette::Window};
    QPalette m_palette;
    QPalette::ColorGroup m_group{QPalette::Active};
    bool m_gradients;
    bool m_directionVisible{true};
    bool m_swatchOnly{false};
};

void BrushSourceButtonPrivate::updateButton()
{
    m_button->setText(brushSourceName(m_value));

    const QSize size = m_swatchOnly ? (m_self->size() - QSize{8, 4}).expandedTo(QSize{16, 12}) : QSize{16, 12};
    m_button->setIcon(brushSourceIcon(m_value, m_palette, m_group, m_self->devicePixelRatioF(), size));

    m_button->setToolTip(BrushSourceButton::tr("Edit ‘%1’").arg(brushSourceName(m_value)));
}

void BrushSourceButtonPrivate::openPicker()
{
    if(m_dialog) {
        m_dialog->raise();
        m_dialog->activateWindow();
        return;
    }

    const auto original{m_value};

    auto* dialog = new BrushPickerDialog(m_value, m_palette, m_group, m_gradients, m_self);
    m_dialog     = dialog;

    dialog->setDirectionControlVisible(m_directionVisible);

    QObject::connect(dialog, &BrushPickerDialog::valueChanged, m_self, [this](const BrushSource& brush) {
        m_self->setValue(brush, m_palette, m_group);
        Q_EMIT m_self->valueChanged();
    });
    QObject::connect(dialog, &QDialog::finished, m_self, [this, original](int result) {
        const QPointer guard{m_self};
        if(result != QDialog::Accepted) {
            m_value = original;
            updateButton();
        }
        Q_EMIT m_self->editingFinished(result == QDialog::Accepted);
        if(guard) {
            m_dialog = nullptr;
            m_button->setFocus();
        }
    });

    const QPointer guard{m_self};
    Q_EMIT m_self->editingStarted();
    if(guard) {
        dialog->open();
    }
}

BrushSourceButton::BrushSourceButton(QWidget* parent, bool gradients)
    : QWidget{parent}
    , p{std::make_unique<BrushSourceButtonPrivate>(this, gradients)}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(p->m_button);

    setFocusProxy(p->m_button);

    QObject::connect(p->m_button, &QAbstractButton::clicked, this, &BrushSourceButton::openPicker);
    p->updateButton();
}

BrushSourceButton::~BrushSourceButton() = default;

const BrushSource& BrushSourceButton::value() const
{
    return p->m_value;
}

bool BrushSourceButton::choosingSource() const
{
    return !p->m_dialog.isNull();
}

void BrushSourceButton::setValue(const BrushSource& brush, const QPalette& palette, QPalette::ColorGroup group)
{
    const bool paletteChanged = p->m_palette != palette || p->m_group != group;

    p->m_value   = brush;
    p->m_palette = palette;
    p->m_group   = group;
    p->updateButton();

    if(p->m_dialog && paletteChanged) {
        p->m_dialog->setColourPalette(palette, group);
    }
}

void BrushSourceButton::setDirectionControlVisible(bool visible)
{
    p->m_directionVisible = visible;
    if(p->m_dialog) {
        p->m_dialog->setDirectionControlVisible(visible);
    }
}

void BrushSourceButton::setSwatchOnly(bool swatchOnly)
{
    p->m_swatchOnly = swatchOnly;
    p->m_button->setSwatchOnly(swatchOnly);
    p->updateButton();
    updateGeometry();
}

void BrushSourceButton::openPicker()
{
    p->openPicker();
}

void BrushSourceButton::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if(p->m_swatchOnly) {
        p->updateButton();
    }
}
} // namespace Fooyin
