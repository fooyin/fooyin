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

#include "filtermanager.h"

#include <core/plugins/pluginmanager.h>
#include <gui/guisettings.h>

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>

namespace Filters {
SearchWidget::SearchWidget(FilterManager* manager, Core::SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_manager{manager}
    , m_settings{settings}
    , m_defaultText{"Search library..."}
{
    setObjectName("Search Bar");

    setupUi();

    m_settings->subscribe<Gui::Settings::LayoutEditing>(this, &SearchWidget::searchBoxContextMenu);
    connect(m_searchBox, &QLineEdit::textChanged, this, &SearchWidget::textChanged);
    connect(this, &SearchWidget::searchChanged, m_manager, &FilterManager::searchChanged);
}

QString SearchWidget::name() const
{
    return "Search";
}

SearchWidget::~SearchWidget() = default;

void SearchWidget::setupUi()
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(m_defaultText);
    m_searchBox->setClearButtonEnabled(true);
    searchBoxContextMenu(m_settings->value<Gui::Settings::LayoutEditing>());

    m_layout->addWidget(m_searchBox);
}

void SearchWidget::keyPressEvent(QKeyEvent* e)
{
    const auto key = e->key();
    if(key == Qt::Key_Enter || key == Qt::Key_Return) {
        if(m_manager->tracksHaveFiltered()) {
            // return m_library->prepareTracks();
        }
    }
    // m_searchBox->setFocusPolicy(Qt::StrongFocus);
    m_searchBox->setFocus();
    m_searchBox->setText(e->text());
    QWidget::keyPressEvent(e);
}

void SearchWidget::contextMenuEvent(QContextMenuEvent* event)
{
    Q_UNUSED(event)
}

void SearchWidget::searchBoxContextMenu(bool editing)
{
    if(!editing) {
        m_searchBox->setContextMenuPolicy(Qt::DefaultContextMenu);
    }
    else {
        m_searchBox->setContextMenuPolicy(Qt::NoContextMenu);
    }
}

void SearchWidget::textChanged(const QString& text)
{
    emit searchChanged(text);
}
} // namespace Filters
