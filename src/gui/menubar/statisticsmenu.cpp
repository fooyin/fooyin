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

#include "statisticsmenu.h"

#include "statistics/statisticsdialog.h"
#include "statistics/statisticsservice.h"
#include "statistics/yearinreviewdialog.h"

#include <core/application.h>
#include <core/library/musiclibrary.h>
#include <core/playhistory/playhistory.h>
#include <gui/guiconstants.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/utils.h>

#include <QAction>
#include <QMainWindow>

namespace Fooyin {
StatisticsMenu::StatisticsMenu(Application* core, ActionManager* actionManager, QObject* parent)
    : QObject{parent}
    , m_service{new StatisticsService(core->playHistory(), core->library(), this)}
{
    auto* statisticsMenu = actionManager->actionContainer(Constants::Menus::Statistics);

    const QStringList category{tr("Statistics")};

    auto* charts = new QAction(tr("&Listening statistics"), this);
    charts->setStatusTip(tr("Open charts of your listening history"));
    auto* chartsCmd = actionManager->registerAction(charts, Constants::Actions::ListeningCharts);
    chartsCmd->setCategories(category);
    statisticsMenu->addAction(chartsCmd);
    QObject::connect(charts, &QAction::triggered, this, &StatisticsMenu::showCharts);

    auto* yearInReview = new QAction(tr("&Year in review"), this);
    yearInReview->setStatusTip(tr("Open a summary of your listening over the year"));
    auto* yearCmd = actionManager->registerAction(yearInReview, Constants::Actions::YearInReview);
    yearCmd->setCategories(category);
    statisticsMenu->addAction(yearCmd);
    QObject::connect(yearInReview, &QAction::triggered, this, [this]() { showYearInReview(); });
}

void StatisticsMenu::showCharts()
{
    if(m_chartsDialog) {
        m_chartsDialog->show();
        m_chartsDialog->raise();
        m_chartsDialog->activateWindow();
        return;
    }

    m_chartsDialog = new StatisticsDialog(m_service, Utils::getMainWindow());
    m_chartsDialog->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(m_chartsDialog, &StatisticsDialog::yearInReviewRequested, this,
                     [this](int year) { showYearInReview(year); });
    m_chartsDialog->show();
}

void StatisticsMenu::showYearInReview(int year)
{
    if(!m_yearDialog) {
        m_yearDialog = new YearInReviewDialog(m_service, Utils::getMainWindow());
        m_yearDialog->setAttribute(Qt::WA_DeleteOnClose);
    }

    m_yearDialog->showYear(year);
    m_yearDialog->show();
    m_yearDialog->raise();
    m_yearDialog->activateWindow();
}
} // namespace Fooyin

#include "moc_statisticsmenu.cpp"
