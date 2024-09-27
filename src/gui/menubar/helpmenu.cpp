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

#include "helpmenu.h"

#include "dialog/aboutdialog.h"

#include <gui/guiconstants.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QDesktopServices>
#include <QIcon>
#include <QUrl>

namespace {
void showAboutDialog()
{
    auto* aboutDialog = new Fooyin::AboutDialog();
    aboutDialog->setAttribute(Qt::WA_DeleteOnClose);
    aboutDialog->show();
}
} // namespace

namespace Fooyin {
HelpMenu::HelpMenu(ActionManager* actionManager, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
{
    auto* helpMenu = m_actionManager->actionContainer(Constants::Menus::Help);

    auto* quickStart = new QAction(tr("&Quick start"), this);
    quickStart->setStatusTip(tr("Open the quick start guide"));
    QObject::connect(quickStart, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QStringLiteral("https://docs.fooyin.org/en/latest/quick-start/quick-start.html"));
    });

    auto* scripting = new QAction(tr("&Scripting help"), this);
    scripting->setStatusTip(tr("Open the scripting documentation"));
    QObject::connect(scripting, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QStringLiteral("https://docs.fooyin.org/en/latest/scripting/basics.html"));
    });

    auto* faq = new QAction(tr("&Frequently asked questions"), this);
    faq->setStatusTip(tr("Open the FAQ"));
    QObject::connect(faq, &QAction::triggered, this,
                     []() { QDesktopServices::openUrl(QStringLiteral("https://www.fooyin.org/faq")); });

    auto* about = new QAction(Utils::iconFromTheme(Constants::Icons::Fooyin), tr("&About"), this);
    about->setStatusTip(tr("Open the about dialog"));
    QObject::connect(about, &QAction::triggered, this, showAboutDialog);

    helpMenu->addAction(quickStart);
    helpMenu->addAction(scripting);
    helpMenu->addAction(faq);
    helpMenu->addAction(about);
}
} // namespace Fooyin

#include "moc_helpmenu.cpp"
