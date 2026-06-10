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

#include "radiobrowserplugin.h"

#include "radiobrowsercontextmenu.h"
#include "radiobrowsercontroller.h"
#include "radiobrowserdialog.h"
#include "radiobrowserwidget.h"
#include "radioguidewidget.h"
#include "radiosearch.h"
#include "radiostationstore.h"

#include <gui/editablelayout.h>
#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <gui/layoutprovider.h>
#include <gui/settings/context/staticcontextmenupage.h>
#include <gui/widgetprovider.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/fypaths.h>

#include <QAction>

using namespace Qt::StringLiterals;

namespace Fooyin::RadioBrowser {
namespace {
DbConnection::DbParams dbConnectionParams()
{
    DbConnection::DbParams params;
    params.type           = u"QSQLITE"_s;
    params.connectOptions = u"QSQLITE_OPEN_URI"_s;
    params.filePath       = Utils::sharePath() + u"/radiobrowser.db"_s;

    return params;
}
} // namespace

void RadioBrowserPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_settings         = context.settingsManager;
    m_network          = context.networkAccess;
    m_playlistLoader   = context.playlistLoader;

    m_dbPool = DbConnectionPool::create(dbConnectionParams(), u"radiobrowser"_s);

    m_store = new RadioStationStore(m_dbPool, this);
    m_controller
        = new RadioBrowserController(m_network, m_settings, m_playerController, m_playlistLoader, m_store, this);
}

void RadioBrowserPlugin::initialise(const GuiPluginContext& context)
{
    m_widgetProvider = context.widgetProvider;
    m_editableLayout = context.editableLayout;
    m_actionManager  = context.actionManager;
    m_trackSelection = context.trackSelection;

    new StaticContextMenuPage(
        m_settings,
        makeStaticContextMenuDescriptor(
            RadioBrowserContextMenu::PageId,
            {.context = "RadioBrowserWidget", .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Radio Browser")},
            {.context    = "RadioBrowserWidget",
             .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget",
                                             "Unchecked items will be hidden from the radio browser context menu.")},
            RadioBrowserContextMenu::DefaultItems,
            ContextMenuSettings::makeFileStringListReader(m_settings, RadioBrowserContextMenu::DisabledSectionsKey,
                                                          RadioBrowserContextMenu::defaultDisabledSections()),
            ContextMenuSettings::makeFileStringListWriter(m_settings, RadioBrowserContextMenu::DisabledSectionsKey),
            ContextMenuSettings::makeFileStringListReader(m_settings, RadioBrowserContextMenu::LayoutKey),
            ContextMenuSettings::makeFileStringListWriter(m_settings, RadioBrowserContextMenu::LayoutKey),
            RadioBrowserContextMenu::defaultDisabledSections()),
        this);

    auto* viewMenu         = m_actionManager->actionContainer(Constants::Menus::View);
    auto* showRadioBrowser = new QAction(tr("Radio &Browser"), this);
    showRadioBrowser->setStatusTip(tr("Open the Radio Browser window"));
    Gui::setThemeIcon(showRadioBrowser, Constants::Icons::Rss);
    auto* showRadioBrowserCmd = m_actionManager->registerAction(showRadioBrowser, "RadioBrowser.ShowDialog");
    showRadioBrowserCmd->setCategories({tr("View")});
    viewMenu->addAction(showRadioBrowserCmd);
    QObject::connect(showRadioBrowser, &QAction::triggered, this, &RadioBrowserPlugin::showRadioBrowserDialog);

    m_widgetProvider->registerWidget(
        u"RadioBrowser"_s,
        [this, actionManager = context.actionManager, trackSelection = context.trackSelection]() {
            const bool guideOwnsInitialBrowse = !m_editableLayout->findWidgetsByType<RadioGuideWidget>().empty();
            auto* browser = new RadioBrowserWidget(m_controller, actionManager, trackSelection, m_settings,
                                                   !guideOwnsInitialBrowse);
            QObject::connect(browser, &QObject::destroyed, this, &RadioBrowserPlugin::scheduleRelinkRadioWidgets);
            scheduleRelinkRadioWidgets();
            return browser;
        },
        tr("Radio Browser"));
    m_widgetProvider->setSubMenus(u"RadioBrowser"_s, {tr("Internet")});
    m_widgetProvider->setLimit(u"RadioBrowser"_s, 1);
    m_widgetProvider->registerWidget(
        u"RadioSearch"_s,
        [this]() {
            auto* search = new RadioSearch(m_settings);
            QObject::connect(search, &QObject::destroyed, this, &RadioBrowserPlugin::scheduleRelinkRadioWidgets);
            scheduleRelinkRadioWidgets();
            return search;
        },
        tr("Radio Search"));
    m_widgetProvider->setSubMenus(u"RadioSearch"_s, {tr("Internet")});
    m_widgetProvider->setLimit(u"RadioSearch"_s, 1);
    m_widgetProvider->registerWidget(
        u"RadioGuide"_s, [this]() { return new RadioGuideWidget(m_controller, m_settings); }, tr("Radio Guide"));
    m_widgetProvider->setSubMenus(u"RadioGuide"_s, {tr("Internet")});
    m_widgetProvider->setLimit(u"RadioGuide"_s, 1);

    QObject::connect(m_editableLayout, &EditableLayout::layoutChanged, this,
                     &RadioBrowserPlugin::scheduleRelinkRadioWidgets);
    scheduleRelinkRadioWidgets();

    registerLayouts(*context.layoutProvider);
}

void RadioBrowserPlugin::registerLayouts(LayoutProvider& layoutProvider)
{
    static const QString jsonStart
        = uR"json({"Name":"Radio","Widgets":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAACAAAFFgAAABYA/////wEAAAACAA==",
                "Widgets":[{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAACAAAA1gAABRoA/////wEAAAABAA==",
                "Widgets":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAAEAAABugAAANUAAABWAAAAGgD/////AQAAAAIA",
                "Widgets":[{"RadioGuide":{}},{"ArtworkPanel":{}},{"ScriptDisplay":{"HorizontalAlignment": 4,"Script":"$if(%isstopped%,\n<b>)json"_s;
    const QString playbackStopped = tr("Playback stopped");
    static const QString jsonEnd
        = uR"json(</b>,\n<sized=1><b>$if2(%station%,$if(%streamtitle%,,%title%))</b></size>\n$crlf()\n$if2(%streamtitle%,[$join( - ,%artist%,%title%)])\n)"}},
            {"PlayerControls":{"ShowNext":false,"ShowPrevious":false}},{"VolumeControls":{}}]}},
            {"SplitterVertical":{"State":"AAAA/wAAAAEAAAACAAAAHAAABOIA/////wEAAAACAA==","Widgets":[{"RadioSearch":{}},{"RadioBrowser":{}}]}}]}},{"StatusBar":{}}]}}]})json"_s;

    const QString fullJson = jsonStart + playbackStopped + jsonEnd;
    layoutProvider.registerLayout(fullJson.toUtf8());
}

void RadioBrowserPlugin::scheduleRelinkRadioWidgets()
{
    QMetaObject::invokeMethod(this, &RadioBrowserPlugin::relinkRadioWidgets, Qt::QueuedConnection);
}

void RadioBrowserPlugin::relinkRadioWidgets()
{
    const auto browsers = m_editableLayout->findWidgetsByType<RadioBrowserWidget>();
    if(browsers.empty()) {
        return;
    }

    const auto searches = m_editableLayout->findWidgetsByType<RadioSearch>();
    const auto guides   = m_editableLayout->findWidgetsByType<RadioGuideWidget>();

    auto* search = searches.empty() ? nullptr : searches.front();
    for(auto* browser : browsers) {
        browser->connectFilterBar(search);
        browser->setApplySearchOnLoad(guides.empty());
    }
}

void RadioBrowserPlugin::showRadioBrowserDialog()
{
    auto* dialog = new RadioBrowserDialog(m_network, m_playlistLoader, m_settings, m_playerController, m_store,
                                          m_actionManager, m_trackSelection);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
} // namespace Fooyin::RadioBrowser

#include "moc_radiobrowserplugin.cpp"
