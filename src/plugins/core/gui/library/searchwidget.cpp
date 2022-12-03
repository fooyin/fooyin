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

#include "searchwidget.h"

#include "core/library/musiclibrary.h"
#include "core/settings/settings.h"

#include <pluginsystem/pluginmanager.h>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>

struct SearchWidget::Private
{
    Settings* settings;
    QHBoxLayout* layout;
    QLineEdit* searchBox;
    const QString defaultText = "Search library...";

    Library::MusicLibrary* library;

    explicit Private()
        : settings(PluginSystem::object<Settings>())
        , library(PluginSystem::object<Library::MusicLibrary>())
    { }
};

SearchWidget::SearchWidget(QWidget* parent)
    : Widget(parent)
    , p(std::make_unique<Private>())
{
    setObjectName("Search Bar");

    setupUi();

    connect(p->settings, &Settings::layoutEditingChanged, this, &SearchWidget::searchBoxContextMenu);
    connect(p->searchBox, &QLineEdit::textChanged, this, &SearchWidget::textChanged);
    connect(this, &SearchWidget::searchChanged, p->library, &Library::MusicLibrary::searchChanged);
}

QString SearchWidget::name() const
{
    return "Search";
}

void SearchWidget::layoutEditingMenu(QMenu* menu)
{
    Q_UNUSED(menu)
}

SearchWidget::~SearchWidget() = default;

void SearchWidget::setupUi()
{
    p->layout = new QHBoxLayout(this);
    p->layout->setContentsMargins(0, 0, 0, 0);

    p->searchBox = new QLineEdit(this);
    p->searchBox->setPlaceholderText(p->defaultText);
    p->searchBox->setClearButtonEnabled(true);
    searchBoxContextMenu(p->settings->value(Settings::Setting::LayoutEditing).toBool());

    p->layout->addWidget(p->searchBox);
}

void SearchWidget::keyPressEvent(QKeyEvent* e)
{
    const auto key = e->key();
    if(key == Qt::Key_Enter || key == Qt::Key_Return) {
        if(p->library->tracksHaveFiltered()) {
            return p->library->prepareTracks();
        }
    }
    // p->searchBox->setFocusPolicy(Qt::StrongFocus);
    p->searchBox->setFocus();
    p->searchBox->setText(e->text());
    QWidget::keyPressEvent(e);
}

void SearchWidget::contextMenuEvent(QContextMenuEvent* event)
{
    Q_UNUSED(event)
}

void SearchWidget::searchBoxContextMenu(bool editing)
{
    if(!editing) {
        p->searchBox->setContextMenuPolicy(Qt::DefaultContextMenu);
    }
    else {
        p->searchBox->setContextMenuPolicy(Qt::NoContextMenu);
    }
}

void SearchWidget::textChanged(const QString& text)
{
    emit searchChanged(text);
}
