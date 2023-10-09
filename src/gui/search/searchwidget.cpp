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

#include "searchwidget.h"

#include <gui/guisettings.h>
#include <gui/searchcontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>

namespace Fy::Gui::Widgets {
constexpr auto Placeholder = "Search library...";

SearchWidget::SearchWidget(SearchController* controller, Utils::SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_controller{controller}
    , m_settings{settings}
    , m_searchBox{new QLineEdit(this)}
{
    setObjectName("Search Bar");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_searchBox->setPlaceholderText(Placeholder);
    m_searchBox->setClearButtonEnabled(true);

    layout->addWidget(m_searchBox);

    connect(m_searchBox, &QLineEdit::textChanged, m_controller, &SearchController::searchChanged);
}

QString SearchWidget::name() const
{
    return "Search";
}

void SearchWidget::keyPressEvent(QKeyEvent* e)
{
    //    const auto key = e->key();
    //    if(key == Qt::Key_Enter || key == Qt::Key_Return) {
    //            m_library->prepareTracks();
    //    }
    // m_searchBox->setFocusPolicy(Qt::StrongFocus);
    //    m_searchBox->setFocus();
    //    m_searchBox->setText(e->text());
    QWidget::keyPressEvent(e);
}

void SearchWidget::contextMenuEvent(QContextMenuEvent* event)
{
    if(!m_settings->value<Gui::Settings::LayoutEditing>()) {
        return QWidget::contextMenuEvent(event);
    }
}
} // namespace Fy::Gui::Widgets
