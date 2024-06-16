/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "wavebarplugin.h"

#include "settings/wavebarguisettingspage.h"
#include "settings/wavebarsettings.h"
#include "settings/wavebarsettingspage.h"
#include "wavebarconstants.h"
#include "wavebarwidget.h"
#include "waveformbuilder.h"

#include <core/engine/enginecontroller.h>
#include <core/player/playercontroller.h>
#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgetprovider.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/async.h>
#include <utils/paths.h>

#include <QMenu>
#include <QProgressDialog>

namespace {
Fooyin::DbConnection::DbParams dbConnectionParams()
{
    Fooyin::DbConnection::DbParams params;
    params.type           = QStringLiteral("QSQLITE");
    params.connectOptions = QStringLiteral("QSQLITE_OPEN_URI");
    params.filePath       = Fooyin::WaveBar::cachePath();

    return params;
}
} // namespace

namespace Fooyin::WaveBar {
struct WaveBarPlugin::Private
{
    WaveBarPlugin* m_self;

    ActionManager* m_actionManager;
    PlayerController* m_playerController;
    EngineController* m_engine;
    TrackSelectionController* m_trackSelection;
    WidgetProvider* m_widgetProvider;
    SettingsManager* m_settings;

    DbConnectionPoolPtr m_dbPool;
    std::unique_ptr<WaveformBuilder> m_waveBuilder;

    std::unique_ptr<WaveBarSettings> m_waveBarSettings;
    std::unique_ptr<WaveBarSettingsPage> m_waveBarSettingsPage;
    std::unique_ptr<WaveBarGuiSettingsPage> m_waveBarGuiSettingsPage;

    explicit Private(WaveBarPlugin* self)
        : m_self{self}
        , m_dbPool{DbConnectionPool::create(dbConnectionParams(), QStringLiteral("wavebar"))}
    { }

    FyWidget* createWavebar()
    {
        if(!m_waveBuilder) {
            m_waveBuilder = std::make_unique<WaveformBuilder>(m_engine->createDecoder(), m_dbPool, m_settings);
        }

        auto* wavebar = new WaveBarWidget(m_waveBuilder.get(), m_playerController, m_settings);

        const Track currTrack = m_playerController->currentTrack();
        if(currTrack.isValid()) {
            m_waveBuilder->generateAndScale(currTrack);
        }

        return wavebar;
    }

    void regenerateSelection(bool onlyMissing = false) const
    {
        auto selectedTracks = m_trackSelection->selectedTracks();
        if(selectedTracks.empty()) {
            return;
        }

        const Track currentTrack = m_playerController->currentTrack();
        auto currIt              = std::ranges::find(selectedTracks, currentTrack);
        if(m_waveBuilder && currIt != selectedTracks.cend()) {
            selectedTracks.erase(currIt);
            m_waveBuilder->generateAndScale(currentTrack, !onlyMissing);
        }

        if(selectedTracks.empty()) {
            return;
        }

        const auto total = static_cast<int>(selectedTracks.size());
        auto* dialog = new QProgressDialog(QStringLiteral("Generating waveform data…"), QStringLiteral("Abort"), 0,
                                           total, nullptr);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowModality(Qt::WindowModal);
        dialog->setValue(0);

        auto* builder = new WaveformBuilder(m_engine->createDecoder(), m_dbPool, m_settings, dialog);

        QObject::connect(builder, &WaveformBuilder::waveformGenerated, dialog, [dialog, builder]() {
            if(dialog->wasCanceled()) {
                delete builder;
            }

            dialog->setValue(dialog->value() + 1);
        });

        for(const Track& track : selectedTracks) {
            builder->generate(track, !onlyMissing);
        }
    }

    void removeSelection()
    {
        auto selectedTracks = m_trackSelection->selectedTracks();
        if(selectedTracks.empty()) {
            return;
        }

        Utils::asyncExec([this, selectedTracks]() {
            QStringList keys;
            for(const Track& track : selectedTracks) {
                keys.emplace_back(WaveBarDatabase::cacheKey(track));
            }

            const DbConnectionHandler dbHandler{m_dbPool};
            WaveBarDatabase waveDb;
            waveDb.initialise(DbConnectionProvider{m_dbPool});
            waveDb.initialiseDatabase();

            if(!waveDb.removeFromCache(keys)) {
                qDebug() << "[WaveBar] Unable to remove waveform data";
            }
        });
    }

    void clearCache() const
    {
        const DbConnectionHandler handler{m_dbPool};
        WaveBarDatabase waveDb;
        waveDb.initialise(DbConnectionProvider{m_dbPool});

        if(!waveDb.clearCache()) {
            qDebug() << "[WaveBar] Unable to clear cache";
        }
    }
};

WaveBarPlugin::WaveBarPlugin()
    : p{std::make_unique<Private>(this)}
{ }

WaveBarPlugin::~WaveBarPlugin()
{
    if(p->m_waveBuilder) {
        p->m_waveBuilder.reset();
    }
}

void WaveBarPlugin::initialise(const CorePluginContext& context)
{
    p->m_playerController = context.playerController;
    p->m_engine           = context.engine;
    p->m_settings         = context.settingsManager;
}

void WaveBarPlugin::initialise(const GuiPluginContext& context)
{
    p->m_actionManager  = context.actionManager;
    p->m_trackSelection = context.trackSelection;
    p->m_widgetProvider = context.widgetProvider;

    p->m_waveBarSettings        = std::make_unique<WaveBarSettings>(p->m_settings);
    p->m_waveBarSettingsPage    = std::make_unique<WaveBarSettingsPage>(p->m_settings);
    p->m_waveBarGuiSettingsPage = std::make_unique<WaveBarGuiSettingsPage>(p->m_settings);

    QObject::connect(p->m_waveBarSettingsPage.get(), &WaveBarSettingsPage::clearCache, this,
                     [this]() { p->clearCache(); });

    p->m_widgetProvider->registerWidget(
        QStringLiteral("WaveBar"), [this]() { return p->createWavebar(); }, tr("Waveform Seekbar"));
    p->m_widgetProvider->setSubMenus(QStringLiteral("WaveBar"), {tr("Controls")});

    auto* selectionMenu = p->m_actionManager->actionContainer(::Fooyin::Constants::Menus::Context::TrackSelection);
    auto* utilitiesMenu = p->m_actionManager->createMenu("Fooyin.Menu.Utilities");
    utilitiesMenu->menu()->setTitle(tr("Utilities"));
    selectionMenu->addMenu(utilitiesMenu);

    auto* regenerate = new QAction(tr("Regenerate waveform data"), this);
    QObject::connect(regenerate, &QAction::triggered, this, [this]() { p->regenerateSelection(); });
    utilitiesMenu->addAction(regenerate);

    auto* regenerateMissing = new QAction(tr("Generate missing waveform data"), this);
    QObject::connect(regenerateMissing, &QAction::triggered, this, [this]() { p->regenerateSelection(true); });
    utilitiesMenu->addAction(regenerateMissing);

    auto* removeData = new QAction(tr("Remove waveform data"), this);
    QObject::connect(removeData, &QAction::triggered, this, [this]() { p->removeSelection(); });
    utilitiesMenu->addAction(removeData);
}
} // namespace Fooyin::WaveBar

#include "moc_wavebarplugin.cpp"
