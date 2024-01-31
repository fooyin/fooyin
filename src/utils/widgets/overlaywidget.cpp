/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <utils/widgets/overlaywidget.h>

#include <QApplication>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace Fooyin {
struct OverlayWidget::Private
{
    OverlayWidget* self;

    QVBoxLayout* layout;

    int xOffset{0};
    int yOffset{0};

    QPushButton* button{nullptr};
    QLabel* label{nullptr};

    bool hovered{false};
    bool selected{false};

    Options options;
    QColor colour;
    QColor hoveredColour{Qt::white};
    QColor selectedColour{Qt::white};

    Private(OverlayWidget* self_, const Options& options_)
        : self{self_}
        , layout{new QVBoxLayout(self)}
        , options{options_}
    {
        layout->setContentsMargins(5, 5, 5, 5);
        layout->setAlignment(Qt::AlignCenter);

        hoveredColour.setAlpha(40);
        selectedColour.setAlpha(100);
    }

    void handleParentChanged()
    {
        if(!self->parent()) {
            return;
        }

        xOffset = self->parentWidget()->width() - self->x();
        yOffset = self->parentWidget()->height() - self->y();

        self->parent()->installEventFilter(self);
        self->raise();
    }
};

OverlayWidget::OverlayWidget(QWidget* parent)
    : OverlayWidget{None, parent}
{ }

OverlayWidget::~OverlayWidget() = default;

OverlayWidget::OverlayWidget(const Options& options, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<Private>(this, options)}
{
    setAttribute(Qt::WA_NoSystemBackground);

    if(options & Option::Label) {
        p->label = new QLabel(this);
        p->label->setContentsMargins(5, 5, 5, 5);
        p->label->setWordWrap(true);
        p->label->setAlignment(Qt::AlignCenter);
        p->label->setAutoFillBackground(true);
        p->layout->addWidget(p->label);
    }

    if(options & Option::Button) {
        p->button = new QPushButton(this);
        p->button->setAutoFillBackground(true);
        p->layout->addWidget(p->button);
    }

    if(p->options & Option::Resize || p->options & Option::Static) {
        p->handleParentChanged();
    }

    resetColour();
    hide();
}

void OverlayWidget::setOption(Option option, bool on)
{
    if(on) {
        p->options |= option;
    }
    else {
        p->options &= ~option;
    }
}

void OverlayWidget::setText(const QString& text)
{
    if(p->label) {
        p->label->setText(text);
    }
}

void OverlayWidget::setButtonText(const QString& text)
{
    if(p->button) {
        p->button->setText(text);
    }
}

QPushButton* OverlayWidget::button() const
{
    return p->button;
}

QLabel* OverlayWidget::label() const
{
    return p->label;
}

void OverlayWidget::connectOverlay(OverlayWidget* other)
{
    if(other == this) {
        return;
    }

    static auto connectOverlays = [](OverlayWidget* first, OverlayWidget* second) {
        QObject::connect(first, &OverlayWidget::clicked, second, [second]() {
            second->p->selected = true;
            second->update();
        });
        QObject::connect(first, &OverlayWidget::entered, second, [second]() {
            second->p->hovered = true;
            second->update();
        });
        QObject::connect(first, &OverlayWidget::left, second, [second]() {
            second->p->hovered = false;
            second->update();
        });
    };

    connectOverlays(this, other);
    connectOverlays(other, this);
}

void OverlayWidget::disconnectOverlay(OverlayWidget* other)
{
    if(other == this) {
        return;
    }

    disconnect(other);
    other->disconnect(this);
}

void OverlayWidget::addWidget(QWidget* widget)
{
    widget->setParent(this);
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    p->layout->addWidget(widget);

    adjustSize();
}

QColor OverlayWidget::colour() const
{
    return p->colour;
}

void OverlayWidget::setColour(const QColor& colour)
{
    p->colour = colour;
    update();
}

void OverlayWidget::resetColour()
{
    static QColor colour = QApplication::palette().color(QPalette::Highlight);
    colour.setAlpha(80);
    p->colour = colour;
}

void OverlayWidget::select()
{
    p->selected = true;
    update();
}

void OverlayWidget::deselect()
{
    p->selected = false;
    update();
}

bool OverlayWidget::event(QEvent* event)
{
    if(event->type() == QEvent::ParentAboutToChange) {
        if(parent()) {
            parent()->removeEventFilter(this);
        }
    }
    else if(event->type() == QEvent::ParentChange) {
        p->handleParentChanged();
    }

    return QWidget::event(event);
}

bool OverlayWidget::eventFilter(QObject* watched, QEvent* event)
{
    if(watched != parent()) {
        return QWidget::eventFilter(watched, event);
    }

    if(event->type() == QEvent::Resize) {
        if(auto* resizeEvent = static_cast<QResizeEvent*>(event)) {
            if(p->options & Option::Resize) {
                resize(resizeEvent->size());
            }
        }
        if(p->options & Option::Static) {
            move(parentWidget()->width() - p->xOffset, parentWidget()->height() - p->yOffset);
        }
    }
    else if(event->type() == QEvent::ChildAdded) {
        raise();
    }

    event->accept();
    return true;
}

void OverlayWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    p->xOffset = parentWidget()->width() - x();
    p->yOffset = parentWidget()->height() - y();
}

void OverlayWidget::enterEvent(QEnterEvent* event)
{
    QWidget::enterEvent(event);

    if(p->options & Selectable) {
        p->hovered = true;
        update();
        emit entered();
    }
}

void OverlayWidget::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);

    if(p->options & Selectable) {
        p->hovered = false;
        update();
        emit left();
    }
}

void OverlayWidget::mousePressEvent(QMouseEvent* event)
{
    QWidget::mousePressEvent(event);

    if(p->options & Selectable) {
        p->selected = true;
        update();
        emit clicked();
    }
}

void OverlayWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    QPainter painter{this};

    painter.fillRect(rect(), p->colour);

    if(p->options & Selectable) {
        painter.fillRect(rect(), p->selected ? p->selectedColour : p->hovered ? p->hoveredColour : p->colour);
    }
    else {
        painter.fillRect(rect(), p->colour);
    }
}
} // namespace Fooyin

#include "utils/widgets/moc_overlaywidget.cpp"
