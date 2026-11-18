/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <gui/widgets/colourbutton.h>

#include <gui/widgets/colourpickerdialog.h>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QPointer>
#include <QPushButton>

namespace Fooyin {
namespace {
class ColourSwatch : public QPushButton
{
public:
    explicit ColourSwatch(const QColor& colour, QWidget* parent = nullptr)
        : QPushButton{parent}
        , m_colour{colour}
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    [[nodiscard]] QColor colour() const
    {
        return m_colour;
    }

    void setColour(const QColor& colour)
    {
        m_colour = colour;
        update();
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        return {20, 20};
    }

    [[nodiscard]] QSize minimumSizeHint() const override
    {
        return {20, 20};
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPushButton::paintEvent(event);

        QPainter painter{this};
        if(m_colour.alpha() < 255) {
            for(int y{0}; y < height(); y += 4) {
                for(int x{0}; x < width(); x += 4) {
                    painter.fillRect(QRect{x, y, 4, 4},
                                     ((x + y) / 4) % 2 ? QColor{160, 160, 160} : QColor{220, 220, 220});
                }
            }
        }
        painter.fillRect(rect(), m_colour);

        if(!isEnabled()) {
            QColor overlay = palette().color(QPalette::Window);
            overlay.setAlpha(160);
            painter.fillRect(rect(), overlay);
        }

        const auto colourGroup = isEnabled() ? QPalette::Active : QPalette::Disabled;
        painter.setPen({palette().color(colourGroup, QPalette::Text), 2});
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }

private:
    QColor m_colour;
};
} // namespace

class ColourButtonPrivate
{
public:
    ColourButtonPrivate(ColourButton* self, const QString& text, const QColor& colour)
        : m_self{self}
        , m_checkBox{new QCheckBox(text, m_self)}
        , m_swatch{new ColourSwatch(colour, m_self)}
        , m_dialog{nullptr}
    { }

    void pickColour();
    void updateButtonState();

    ColourButton* m_self;
    bool m_changed{false};
    QCheckBox* m_checkBox;
    ColourSwatch* m_swatch;
    QPointer<ColourPickerDialog> m_dialog;
};

void ColourButtonPrivate::pickColour()
{
    if(m_dialog) {
        return;
    }

    auto* dialog = new ColourPickerDialog(m_self->colour(), m_self);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_dialog = dialog;

    QObject::connect(
        dialog, &ColourPickerDialog::colourSelected, m_self,
        [this](const QColor& chosenColour) {
            const QPointer guard{m_self};
            if(chosenColour != m_self->colour()) {
                m_changed = true;
                m_self->setColour(chosenColour);
            }
            if(guard) {
                Q_EMIT m_self->colourSelected(chosenColour);
            }
        },
        Qt::QueuedConnection);
    QObject::connect(
        dialog, &QDialog::finished, m_self,
        [this, picker = QPointer{dialog}] {
            if(m_dialog == picker) {
                m_dialog = nullptr;
                m_swatch->setFocus();
            }
        },
        Qt::QueuedConnection);
    dialog->open();
}

void ColourButtonPrivate::updateButtonState()
{
    m_swatch->setEnabled(m_self->isChecked());
}

ColourButton::ColourButton(QWidget* parent)
    : ColourButton{{}, {}, false, parent}
{ }

ColourButton::ColourButton(bool checkable, QWidget* parent)
    : ColourButton{{}, {}, checkable, parent}
{ }

ColourButton::ColourButton(const QColor& colour, QWidget* parent)
    : ColourButton{{}, colour, false, parent}
{ }

ColourButton::ColourButton(const QString& text, const QColor& colour, QWidget* parent)
    : ColourButton{text, colour, false, parent}
{ }

ColourButton::ColourButton(const QString& text, bool checkable, QWidget* parent)
    : ColourButton{text, {}, checkable, parent}
{ }

ColourButton::ColourButton(const QString& text, const QColor& colour, bool checkable, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<ColourButtonPrivate>(this, text, colour)}
{
    p->m_checkBox->setVisible(checkable);
    setFocusProxy(p->m_swatch);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(p->m_checkBox);
    layout->addWidget(p->m_swatch, 1);

    QObject::connect(p->m_swatch, &QPushButton::clicked, this, [this]() {
        const QPointer guard{this};
        Q_EMIT clicked();
        if(guard) {
            p->pickColour();
        }
    });
    QObject::connect(p->m_checkBox, &QCheckBox::toggled, this, [this](bool checked) {
        p->updateButtonState();
        Q_EMIT toggled(checked);
    });

    p->updateButtonState();
}

ColourButton::~ColourButton() = default;

QColor ColourButton::colour() const
{
    return p->m_swatch->colour();
}

bool ColourButton::colourChanged() const
{
    return p->m_changed;
}

bool ColourButton::choosingColour() const
{
    return !p->m_dialog.isNull();
}

bool ColourButton::isCheckable() const
{
    return !p->m_checkBox->isHidden();
}

bool ColourButton::isChecked() const
{
    return !isCheckable() || p->m_checkBox->isChecked();
}

int ColourButton::labelWidthHint() const
{
    return p->m_checkBox->sizeHint().width();
}

QString ColourButton::text() const
{
    return p->m_checkBox->text();
}

QSize ColourButton::swatchSize() const
{
    return p->m_swatch->size();
}

void ColourButton::setSwatchSize(const QSize& size)
{
    p->m_swatch->setFixedSize(size);
    updateGeometry();
}

void ColourButton::setColour(const QColor& colour)
{
    if(p->m_swatch->colour() == colour) {
        return;
    }

    p->m_swatch->setColour(colour);
    Q_EMIT colourUpdated(colour);
}

void ColourButton::setCheckable(bool checkable)
{
    p->m_checkBox->setToolTip(toolTip());
    p->m_checkBox->setVisible(checkable);
    p->updateButtonState();
    updateGeometry();
}

void ColourButton::setChecked(bool checked)
{
    p->m_checkBox->setChecked(checked);
}

void ColourButton::setLabelWidth(int width)
{
    p->m_checkBox->setMinimumWidth(std::max(width, labelWidthHint()));
    updateGeometry();
}

void ColourButton::setText(const QString& text)
{
    p->m_checkBox->setText(text);
    updateGeometry();
}

void ColourButton::setToolTip(const QString& text)
{
    QWidget::setToolTip(text);
    p->m_checkBox->setToolTip(text);
    p->m_swatch->setToolTip(text);
}

} // namespace Fooyin

#include "gui/widgets/moc_colourbutton.cpp"
