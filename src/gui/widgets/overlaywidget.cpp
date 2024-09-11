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

#include <gui/widgets/overlaywidget.h>

#include <QApplication>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace Fooyin {
class OverlayWidgetPrivate
{
public:
    OverlayWidgetPrivate(OverlayWidget* self, const OverlayWidget::Options& options)
        : m_self{self}
        , m_layout{new QVBoxLayout(m_self)}
        , m_options{options}
    {
        m_layout->setContentsMargins(5, 5, 5, 5);
        m_layout->setAlignment(Qt::AlignCenter);

        m_hoveredColour.setAlpha(40);
        m_selectedColour.setAlpha(100);
    }

    void handleParentChanged()
    {
        if(!m_self->parent()) {
            return;
        }

        m_xOffset = m_self->parentWidget()->width() - m_self->x();
        m_yOffset = m_self->parentWidget()->height() - m_self->y();

        m_self->parent()->installEventFilter(m_self);
        m_self->raise();
    }

    OverlayWidget* m_self;

    QVBoxLayout* m_layout;

    int m_xOffset{0};
    int m_yOffset{0};

    QPushButton* m_button{nullptr};
    QLabel* m_label{nullptr};

    bool m_hovered{false};
    bool m_selected{false};

    OverlayWidget::Options m_options;
    QColor m_colour;
    QColor m_hoveredColour{Qt::white};
    QColor m_selectedColour{Qt::white};
};

OverlayWidget::OverlayWidget(QWidget* parent)
    : OverlayWidget{None, parent}
{ }

OverlayWidget::~OverlayWidget() = default;

OverlayWidget::OverlayWidget(const Options& options, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<OverlayWidgetPrivate>(this, options)}
{
    setAttribute(Qt::WA_NoSystemBackground);

    if(options & Option::Label) {
        p->m_label = new QLabel(this);
        p->m_label->setContentsMargins(5, 5, 5, 5);
        p->m_label->setWordWrap(true);
        p->m_label->setAlignment(Qt::AlignCenter);
        p->m_label->setAutoFillBackground(true);
        p->m_layout->addWidget(p->m_label);
    }

    if(options & Option::Button) {
        p->m_button = new QPushButton(this);
        p->m_button->setAutoFillBackground(true);
        p->m_layout->addWidget(p->m_button);
    }

    if(p->m_options & Option::Resize || p->m_options & Option::Static) {
        p->handleParentChanged();
    }

    resetColour();
    hide();
}

void OverlayWidget::setOption(Option option, bool on)
{
    if(on) {
        p->m_options |= option;
    }
    else {
        p->m_options &= ~option;
    }
}

void OverlayWidget::setText(const QString& text)
{
    if(p->m_label) {
        p->m_label->setText(text);
    }
}

void OverlayWidget::setButtonText(const QString& text)
{
    if(p->m_button) {
        p->m_button->setText(text);
    }
}

QPushButton* OverlayWidget::button() const
{
    return p->m_button;
}

QLabel* OverlayWidget::label() const
{
    return p->m_label;
}

void OverlayWidget::connectOverlay(OverlayWidget* other)
{
    if(other == this) {
        return;
    }

    static auto connectOverlays = [](OverlayWidget* first, OverlayWidget* second) {
        QObject::connect(first, &OverlayWidget::clicked, second, [second]() {
            second->p->m_selected = true;
            second->update();
        });
        QObject::connect(first, &OverlayWidget::entered, second, [second]() {
            second->p->m_hovered = true;
            second->update();
        });
        QObject::connect(first, &OverlayWidget::left, second, [second]() {
            second->p->m_hovered = false;
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

    p->m_layout->addWidget(widget);

    adjustSize();
}

QColor OverlayWidget::colour() const
{
    return p->m_colour;
}

void OverlayWidget::setColour(const QColor& colour)
{
    p->m_colour = colour;
    update();
}

void OverlayWidget::resetColour()
{
    static QColor colour = QApplication::palette().color(QPalette::Highlight);
    colour.setAlpha(80);
    p->m_colour = colour;
}

void OverlayWidget::select()
{
    p->m_selected = true;
    update();
}

void OverlayWidget::deselect()
{
    p->m_selected = false;
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
            if(p->m_options & Option::Resize) {
                resize(resizeEvent->size());
            }
        }
        if(p->m_options & Option::Static) {
            move(parentWidget()->width() - p->m_xOffset, parentWidget()->height() - p->m_yOffset);
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

    p->m_xOffset = parentWidget()->width() - x();
    p->m_yOffset = parentWidget()->height() - y();
}

void OverlayWidget::enterEvent(QEnterEvent* event)
{
    QWidget::enterEvent(event);

    if(p->m_options & Selectable) {
        p->m_hovered = true;
        update();
        emit entered();
    }
}

void OverlayWidget::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);

    if(p->m_options & Selectable) {
        p->m_hovered = false;
        update();
        emit left();
    }
}

void OverlayWidget::mousePressEvent(QMouseEvent* event)
{
    QWidget::mousePressEvent(event);

    if(p->m_options & Selectable) {
        p->m_selected = true;
        update();
        emit clicked();
    }
}

void OverlayWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    QPainter painter{this};

    painter.fillRect(rect(), p->m_colour);

    if(p->m_options & Selectable) {
        painter.fillRect(rect(), p->m_selected ? p->m_selectedColour : p->m_hovered ? p->m_hoveredColour : p->m_colour);
    }
    else {
        painter.fillRect(rect(), p->m_colour);
    }
}
} // namespace Fooyin

#include "gui/widgets/moc_overlaywidget.cpp"
