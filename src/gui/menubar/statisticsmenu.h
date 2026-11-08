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

#pragma once

#include <QObject>
#include <QPointer>

namespace Fooyin {
class ActionManager;
class Application;
class StatisticsDialog;
class StatisticsService;
class YearInReviewDialog;

class StatisticsMenu : public QObject
{
    Q_OBJECT

public:
    StatisticsMenu(Application* core, ActionManager* actionManager, QObject* parent = nullptr);

    //! Opens the year in review for @p year, or the latest year when it's 0.
    void showYearInReview(int year = 0);

private:
    void showCharts();

    StatisticsService* m_service;
    QPointer<StatisticsDialog> m_chartsDialog;
    QPointer<YearInReviewDialog> m_yearDialog;
};
} // namespace Fooyin
