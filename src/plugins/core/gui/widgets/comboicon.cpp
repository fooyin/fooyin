/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include <QVBoxLayout>
#include <utility>
#include <utils/utils.h>

ComboIcon::ComboIcon(const QString& path, bool autoShift, QWidget* parent)
    : QWidget{parent}
    , m_layout{new QVBoxLayout(this)}
    , m_label{new ClickableLabel(this)}
    , m_autoShift{autoShift}
    , m_active{false}
    , m_currentIndex{0}
{
    setup(path);
}

ComboIcon::ComboIcon(const QString& path, QWidget* parent)
    : ComboIcon{path, false, parent}
{ }

ComboIcon::~ComboIcon() = default;

void ComboIcon::setup(const QString& path)
{
    QPixmap icon{path};
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

void ComboIcon::addPixmap(const QString& path, const QPixmap& icon)
{
    QPalette palette = m_label->palette();
    Icon ico;
    ico.icon = icon;
    ico.iconActive = Util::changePixmapColour(icon, palette.highlight().color());
    m_icons.emplace_back(path, ico);
}

void ComboIcon::addPixmap(const QString& path)
{
    QPixmap pixmap{path};
    addPixmap(path, pixmap);
}

void ComboIcon::setIcon(const QString& path, bool active)
{
    if(path.isEmpty()) {
        return;
    }

    auto it = std::find_if(m_icons.cbegin(), m_icons.cend(), [this, path](const PathIconPair& icon) {
        return (icon.first == path);
    });

    if(it == m_icons.end()) {
        return;
    }

    auto idx = static_cast<int>(std::distance(m_icons.cbegin(), it));
    m_currentIndex = idx;
    if(active) {
        m_label->setPixmap(m_icons.at(m_currentIndex).second.iconActive);
    }
    else {
        m_label->setPixmap(m_icons.at(m_currentIndex).second.icon);
    }
}

void ComboIcon::labelClicked()
{
    if(m_autoShift) {
        if(m_active) {
            ++m_currentIndex;
        }
        if(m_currentIndex >= m_icons.size()) {
            m_currentIndex = 0;
            m_active = false;
            m_label->setPixmap(m_icons.at(m_currentIndex).second.icon);
        }
        else {
            m_active = true;
            m_label->setPixmap(m_icons.at(m_currentIndex).second.iconActive);
        }
    }
    emit clicked(m_icons.at(m_currentIndex).first);
}
