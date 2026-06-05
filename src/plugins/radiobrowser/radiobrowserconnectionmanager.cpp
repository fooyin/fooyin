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

#include "radiobrowserconnectionmanager.h"

#include "radiobrowserwidget.h"
#include "radiosearch.h"

namespace Fooyin::RadioBrowser {
namespace {
int ancestorDistance(const QWidget* widget, const QWidget* ancestor)
{
    int distance{0};

    while(widget) {
        if(widget == ancestor) {
            return distance;
        }
        widget = widget->parentWidget();
        ++distance;
    }

    return -1;
}

int ancestryMatchScore(const QWidget* lhs, const QWidget* rhs)
{
    if(!lhs || !rhs || lhs->window() != rhs->window()) {
        return -1;
    }

    int bestScore{-1};
    for(const QWidget* ancestor = lhs; ancestor; ancestor = ancestor->parentWidget()) {
        const int lhsDistance = ancestorDistance(lhs, ancestor);
        const int rhsDistance = ancestorDistance(rhs, ancestor);
        if(rhsDistance < 0) {
            continue;
        }

        const int score = lhsDistance + rhsDistance;
        if(bestScore < 0 || score < bestScore) {
            bestScore = score;
        }
    }

    return bestScore;
}
} // namespace

RadioBrowserConnectionManager::RadioBrowserConnectionManager(QObject* parent)
    : QObject{parent}
{ }

void RadioBrowserConnectionManager::registerBrowser(RadioBrowserWidget* browser)
{
    if(!browser) {
        return;
    }

    cleanup();

    if(std::ranges::none_of(m_browsers, [browser](const auto& item) { return item == browser; })) {
        m_browsers.emplace_back(browser);
        QObject::connect(browser, &QObject::destroyed, this, [this]() { scheduleRelink(); });
    }

    scheduleRelink();
}

void RadioBrowserConnectionManager::registerFilterBar(RadioSearch* filterBar)
{
    if(!filterBar) {
        return;
    }

    cleanup();

    if(std::ranges::none_of(m_filterBars, [filterBar](const auto& item) { return item == filterBar; })) {
        m_filterBars.emplace_back(filterBar);
        QObject::connect(filterBar, &QObject::destroyed, this, [this]() { scheduleRelink(); });
    }

    scheduleRelink();
}

void RadioBrowserConnectionManager::relink()
{
    cleanup();

    for(const auto& browser : m_browsers) {
        if(!browser) {
            continue;
        }

        RadioSearch* matchedFilterBar{nullptr};
        int bestScore{-1};

        for(const auto& filterBar : m_filterBars) {
            if(filterBar) {
                const int score = ancestryMatchScore(browser, filterBar);
                if(score >= 0 && (bestScore < 0 || score < bestScore)) {
                    matchedFilterBar = filterBar;
                    bestScore        = score;
                }
            }
        }

        browser->connectFilterBar(matchedFilterBar);
    }
}

void RadioBrowserConnectionManager::cleanup()
{
    std::erase_if(m_browsers, [](const auto& browser) { return !browser; });
    std::erase_if(m_filterBars, [](const auto& filterBar) { return !filterBar; });
}

void RadioBrowserConnectionManager::scheduleRelink()
{
    QMetaObject::invokeMethod(this, &RadioBrowserConnectionManager::relink, Qt::QueuedConnection);
}
} // namespace Fooyin::RadioBrowser

#include "moc_radiobrowserconnectionmanager.cpp"
