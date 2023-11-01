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

namespace Fy::Gui::Widgets {
SearchWidget::SearchWidget(Utils::ActionManager* actionManager, SearchController* controller,
                           Utils::SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_actionManager{actionManager}
    , m_controller{controller}
    , m_settings{settings}
    , m_searchBox{new QLineEdit(this)}
    , m_searchContext{new Utils::WidgetContext(this, Utils::Context{Constants::Context::Search}, this)}
{
    setObjectName("Search Bar");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_searchBox->setPlaceholderText(Placeholder);
    m_searchBox->setClearButtonEnabled(true);

    layout->addWidget(m_searchBox);

    QObject::connect(m_searchBox, &QLineEdit::textChanged, m_controller, &SearchController::searchChanged);

    m_actionManager->addContextObject(m_searchContext);

    auto* editMenu = m_actionManager->actionContainer(Constants::Menus::Edit);

    auto* selectAll = new QAction(tr("Select All"), this);
    auto* selectAllCommand
        = m_actionManager->registerAction(selectAll, Constants::Actions::SelectAll, m_searchContext->context());
    selectAllCommand->setDefaultShortcut(QKeySequence::SelectAll);
    editMenu->addAction(selectAllCommand, Utils::Actions::Groups::Two);

    QObject::connect(selectAll, &QAction::triggered, this, [this]() { m_searchBox->selectAll(); });
}

QString SearchWidget::name() const
{
    return QStringLiteral("Search");
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
    if(!m_settings->value<Gui::Settings::LayoutEditing>()) {
        QWidget::contextMenuEvent(event);
    }
}
} // namespace Fy::Gui::Widgets
