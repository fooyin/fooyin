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

#include "radiobrowserdialog.h"

#include "radiobrowsercontroller.h"
#include "radiobrowserwidget.h"
#include "radiosearch.h"
#include "radiostationstore.h"

#include <core/coresettings.h>
#include <utils/settings/settingsmanager.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QTabWidget>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

constexpr auto DialogState    = "RadioBrowser/DialogState"_L1;
constexpr auto MyStationsPage = "MyStations"_L1;
constexpr auto SearchPage     = "Search"_L1;

namespace Fooyin::RadioBrowser {
RadioBrowserDialog::RadioBrowserDialog(std::shared_ptr<NetworkAccessManager> network,
                                       std::shared_ptr<PlaylistLoader> playlistLoader, SettingsManager* settings,
                                       PlayerController* playerController, RadioStationStore* store,
                                       ActionManager* actionManager, TrackSelectionController* trackSelection,
                                       QWidget* parent)
    : QDialog{parent}
    , m_settings{settings}
    , m_tabs{new QTabWidget(this)}
    , m_searchFilterBar{new RadioSearch(settings, this)}
    , m_searchController{new RadioBrowserController(network, settings, playerController, playlistLoader, store, this)}
    , m_savedStationsController{new RadioBrowserController(std::move(network), settings, playerController,
                                                           std::move(playlistLoader), store, this)}
    , m_searchWidget{new RadioBrowserWidget(m_searchController, actionManager, trackSelection, settings, true, this)}
    , m_savedStationsWidget{
          new RadioBrowserWidget(m_savedStationsController, actionManager, trackSelection, settings, false, this)}
{
    setWindowTitle(tr("Radio Browser"));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_tabs);

    auto* searchPage       = new QWidget(this);
    auto* searchPageLayout = new QVBoxLayout(searchPage);
    searchPageLayout->setContentsMargins({});
    searchPageLayout->addWidget(m_searchFilterBar);
    searchPageLayout->addWidget(m_searchWidget, 1);

    m_savedStationsWidget->setApplySearchOnLoad(false);
    m_savedStationsWidget->setFilterBarAllowed(false);
    m_searchWidget->setFilterBarToggleAllowed(false);
    m_searchWidget->connectFilterBar(m_searchFilterBar);

    m_tabs->addTab(m_savedStationsWidget, tr("My Stations"));
    m_tabs->addTab(searchPage, tr("Search"));

    loadState();

    m_searchFilterBar->finalise();
    m_searchWidget->finalise();
    m_searchWidget->showFilterBar();
    m_savedStationsWidget->finalise();

    m_savedStationsWidget->browseSavedStations();
}

QSize RadioBrowserDialog::sizeHint() const
{
    return {900, 560};
}

void RadioBrowserDialog::done(const int value)
{
    saveState();
    QDialog::done(value);
}

void RadioBrowserDialog::saveState()
{
    QJsonObject layout;
    QJsonObject search;
    QJsonObject savedStations;

    m_searchWidget->saveLayoutData(search);
    m_searchFilterBar->saveLayoutData(search);
    m_savedStationsWidget->saveLayoutData(savedStations);

    layout["Geometry"_L1]    = QString::fromUtf8(saveGeometry().toBase64());
    layout["CurrentPage"_L1] = QString{m_tabs->currentIndex() == 1 ? SearchPage : MyStationsPage};
    layout["Search"_L1]      = search;
    layout["MyStations"_L1]  = savedStations;
    const QByteArray state   = QJsonDocument{layout}.toJson(QJsonDocument::Compact).toBase64();

    FyStateSettings stateSettings;
    stateSettings.setValue(DialogState, state);
}

void RadioBrowserDialog::loadState()
{
    const FyStateSettings stateSettings;
    const QByteArray state = stateSettings.value(DialogState).toByteArray();
    if(state.isEmpty()) {
        return;
    }

    const QJsonObject layout = QJsonDocument::fromJson(QByteArray::fromBase64(state)).object();
    if(layout.isEmpty()) {
        return;
    }

    if(layout.contains("Search"_L1)) {
        const QJsonObject search = layout.value("Search"_L1).toObject();
        m_searchWidget->loadLayoutData(search);
        m_searchFilterBar->loadLayoutData(search);
    }
    if(layout.contains("MyStations"_L1)) {
        m_savedStationsWidget->loadLayoutData(layout.value("MyStations"_L1).toObject());
    }
    if(layout.contains("CurrentPage"_L1)) {
        m_tabs->setCurrentIndex(layout.value("CurrentPage"_L1).toString() == SearchPage ? 1 : 0);
    }
    if(layout.contains("Geometry"_L1)) {
        restoreGeometry(QByteArray::fromBase64(layout.value("Geometry"_L1).toString().toUtf8()));
    }
}
} // namespace Fooyin::RadioBrowser

#include "moc_radiobrowserdialog.cpp"
