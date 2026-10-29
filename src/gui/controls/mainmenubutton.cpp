/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "mainmenubutton.h"

#include "gui/menubar/mainmenubar.h"

#include <gui/widgets/toolbutton.h>

#include <QHBoxLayout>
#include <QMenu>

using namespace Qt::StringLiterals;

namespace Fooyin {
MainMenuButton::MainMenuButton(MainMenuBar* mainMenu, QAction* action, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->setSpacing(0);

    auto* button = new ToolButton(settings, this);
    auto* menu   = new QMenu(button);

    button->setDefaultAction(action);
    button->setMenu(menu);
    button->setPopupMode(QToolButton::InstantPopup);
    layout->addWidget(button);

    QObject::connect(menu, &QMenu::aboutToShow, this, [mainMenu, menu]() { mainMenu->populateMenu(menu); });
}

QString MainMenuButton::name() const
{
    return tr("Main Menu Button");
}

QString MainMenuButton::layoutName() const
{
    return u"MainMenuButton"_s;
}
} // namespace Fooyin

#include "moc_mainmenubutton.cpp"
