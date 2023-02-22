/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "comboicon.h"

#include <utils/utils.h>

#include <QEvent>
#include <QVBoxLayout>

namespace Utils {
ComboIcon::ComboIcon(const QString& path, Attributes attributes, QWidget* parent)
    : QWidget{parent}
    , m_layout{new QVBoxLayout(this)}
    , m_label{new ClickableLabel(this)}
    , m_attributes{attributes}
    , m_currentIndex{0}
{
    setup(path);
}

ComboIcon::ComboIcon(const QString& path, QWidget* parent)
    : ComboIcon{path, {}, parent}
{ }

void ComboIcon::setup(const QString& path)
{
    const QPixmap icon{path};
    addPixmap(path, icon);

    m_label->setPixmap(icon);
    m_label->setScaledContents(true);

    m_layout->addWidget(m_label);
    m_layout->setSpacing(10);
    m_layout->setContentsMargins(0, 0, 0, 0);

    setLayout(m_layout);

    connect(m_label, &ClickableLabel::clicked, this, &ComboIcon::labelClicked);
    connect(m_label, &ClickableLabel::entered, this, &ComboIcon::entered);
}

bool ComboIcon::hasAttribute(Attribute attribute)
{
    return (m_attributes & attribute);
}

void ComboIcon::addAttribute(Attribute attribute)
{
    m_attributes |= attribute;
}

void ComboIcon::removeAttribute(Attribute attribute)
{
    m_attributes &= ~attribute;
}

void ComboIcon::addPixmap(const QString& path, const QPixmap& icon)
{
    const QPalette palette = m_label->palette();
    Icon ico;
    ico.icon = icon;
    if(hasAttribute(HasActiveIcon)) {
        ico.iconActive = Utils::changePixmapColour(icon, palette.highlight().color());
    }
    ico.iconDisabled = Utils::changePixmapColour(icon, palette.color(QPalette::Disabled, QPalette::Base));
    m_icons.emplace_back(path, ico);
}

void ComboIcon::addPixmap(const QString& path)
{
    const QPixmap pixmap{path};
    addPixmap(path, pixmap);
}

void ComboIcon::setIcon(const QString& path, bool active)
{
    if(path.isEmpty()) {
        return;
    }

    if(active) {
        addAttribute(Active);
    }
    else {
        removeAttribute(Active);
    }

    auto it = std::find_if(m_icons.cbegin(), m_icons.cend(), [path](const PathIconPair& icon) {
        return (icon.first == path);
    });

    if(it == m_icons.end()) {
        return;
    }

    auto idx       = static_cast<int>(std::distance(m_icons.cbegin(), it));
    m_currentIndex = idx;
    if(hasAttribute(HasActiveIcon) && hasAttribute(Active)) {
        m_label->setPixmap(m_icons.at(m_currentIndex).second.iconActive);
    }
    else {
        m_label->setPixmap(m_icons.at(m_currentIndex).second.icon);
    }
}

void ComboIcon::setIconEnabled(bool enable)
{
    if(enable) {
        addAttribute(Enabled);
    }
    else {
        removeAttribute(Enabled);
    }

    if(hasAttribute(Enabled)) {
        if(hasAttribute(HasActiveIcon) && hasAttribute(Active)) {
            return m_label->setPixmap(m_icons.at(m_currentIndex).second.iconActive);
        }
        return m_label->setPixmap(m_icons.at(m_currentIndex).second.icon);
    }
    return m_label->setPixmap(m_icons.at(m_currentIndex).second.iconDisabled);
}

void ComboIcon::labelClicked()
{
    if(hasAttribute(AutoShift)) {
        if(hasAttribute(Active)) {
            ++m_currentIndex;
        }
        if(m_currentIndex >= static_cast<int>(m_icons.size())) {
            m_currentIndex = 0;
            removeAttribute(Active);
            m_label->setPixmap(m_icons.at(m_currentIndex).second.icon);
        }
        else {
            addAttribute(Active);
            m_label->setPixmap(m_icons.at(m_currentIndex).second.iconActive);
        }
    }
    emit clicked(m_icons.at(m_currentIndex).first);
}

void ComboIcon::changeEvent(QEvent* event)
{
    if(event->type() == QEvent::EnabledChange) {
        setIconEnabled(isEnabled());
    }
    return QWidget::changeEvent(event);
}
} // namespace Utils
