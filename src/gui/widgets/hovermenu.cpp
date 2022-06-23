#include "hovermenu.h"

HoverMenu::HoverMenu(QWidget* parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::CustomizeWindowHint | Qt::Dialog);

    connect(&m_timer, &QTimer::timeout, this, &HoverMenu::closeMenu);
}

HoverMenu::~HoverMenu() = default;

void HoverMenu::leaveEvent(QEvent* event)
{
    emit mouseLeft();
    QWidget::leaveEvent(event);
}

void HoverMenu::showEvent(QShowEvent* event)
{
    // Close after 1 second
    m_timer.start(1000);
    QDialog::showEvent(event);
}

void HoverMenu::closeMenu()
{
    if(this->underMouse() || parentWidget()->underMouse()) {
        // Close as soon as mouse leaves
        return m_timer.start();
    }
    m_timer.stop();
    accept();
}
