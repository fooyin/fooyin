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

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/searchcontroller.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsmanager.h>

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>

constexpr auto Placeholder = "Search library...";

namespace Fooyin {
SearchWidget::SearchWidget(ActionManager* actionManager, SearchController* controller, SettingsManager* settings,
                           QWidget* parent)
    : FyWidget{parent}
    , m_actionManager{actionManager}
    , m_controller{controller}
    , m_settings{settings}
    , m_searchBox{new QLineEdit(this)}
    , m_searchContext{new WidgetContext(this, Context{Constants::Context::Search}, this)}
{
    setObjectName(SearchWidget::name());

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_searchBox->setPlaceholderText(Placeholder);
    m_searchBox->setClearButtonEnabled(true);

    layout->addWidget(m_searchBox);

    QObject::connect(m_searchBox, &QLineEdit::textChanged, m_controller, &SearchController::searchChanged);

    m_actionManager->addContextObject(m_searchContext);

    auto* editMenu = m_actionManager->actionContainer(Constants::Menus::Edit);

    auto* clear = new QAction(tr("&Clear"), this);
    editMenu->addAction(actionManager->registerAction(clear, Constants::Actions::Clear, m_searchContext->context()));
    QObject::connect(clear, &QAction::triggered, m_searchBox, &QLineEdit::clear);

    auto* selectAll = new QAction(tr("&Select All"), this);
    auto* selectAllCommand
        = m_actionManager->registerAction(selectAll, Constants::Actions::SelectAll, m_searchContext->context());
    selectAllCommand->setDefaultShortcut(QKeySequence::SelectAll);
    editMenu->addAction(selectAllCommand, Actions::Groups::Two);
    QObject::connect(selectAll, &QAction::triggered, m_searchBox, &QLineEdit::selectAll);
}

QString SearchWidget::name() const
{
    return QStringLiteral("Search Bar");
}

QString SearchWidget::layoutName() const
{
    return QStringLiteral("SearchBar");
}

void SearchWidget::keyPressEvent(QKeyEvent* event)
{
    //    const auto key = e->key();
    //    if(key == Qt::Key_Enter || key == Qt::Key_Return) {
    //            m_library->prepareTracks();
    //    }
    // m_searchBox->setFocusPolicy(Qt::StrongFocus);
    //    m_searchBox->setFocus();
    //    m_searchBox->setText(e->text());
    QWidget::keyPressEvent(event);
}

void SearchWidget::contextMenuEvent(QContextMenuEvent* event)
{
    if(!m_settings->value<Settings::Gui::LayoutEditing>()) {
        QWidget::contextMenuEvent(event);
    }
}
} // namespace Fooyin

#include "moc_searchwidget.cpp"
