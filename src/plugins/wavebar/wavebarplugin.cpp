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
    WaveBarPlugin* self;

    ActionManager* actionManager;
    PlayerController* playerController;
    EngineController* engine;
    TrackSelectionController* trackSelection;
    WidgetProvider* widgetProvider;
    SettingsManager* settings;

    DbConnectionPoolPtr dbPool;
    std::unique_ptr<WaveformBuilder> waveBuilder;

    std::unique_ptr<WaveBarSettings> waveBarSettings;
    std::unique_ptr<WaveBarSettingsPage> waveBarSettingsPage;
    std::unique_ptr<WaveBarGuiSettingsPage> waveBarGuiSettingsPage;

    explicit Private(WaveBarPlugin* self_)
        : self{self_}
        , dbPool{DbConnectionPool::create(dbConnectionParams(), QStringLiteral("wavebar"))}
    { }

    FyWidget* createWavebar()
    {
        if(!waveBuilder) {
            waveBuilder = std::make_unique<WaveformBuilder>(engine->createDecoder(), dbPool, settings);
        }

        auto* wavebar = new WaveBarWidget(waveBuilder.get(), playerController, settings);

        const Track currTrack = playerController->currentTrack();
        if(currTrack.isValid()) {
            waveBuilder->generateAndScale(currTrack);
        }

        return wavebar;
    }

    void regenerateSelection(bool onlyMissing = false) const
    {
        auto selectedTracks = trackSelection->selectedTracks();
        if(selectedTracks.empty()) {
            return;
        }

        const Track currentTrack = playerController->currentTrack();
        auto currIt              = std::ranges::find(selectedTracks, currentTrack);
        if(waveBuilder && currIt != selectedTracks.cend()) {
            selectedTracks.erase(currIt);
            waveBuilder->generateAndScale(currentTrack, !onlyMissing);
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

        auto* builder = new WaveformBuilder(engine->createDecoder(), dbPool, settings, dialog);

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
        auto selectedTracks = trackSelection->selectedTracks();
        if(selectedTracks.empty()) {
            return;
        }

        Utils::asyncExec([this, selectedTracks]() {
            QStringList keys;
            for(const Track& track : selectedTracks) {
                keys.emplace_back(WaveBarDatabase::cacheKey(track));
            }

            const DbConnectionHandler dbHandler{dbPool};
            WaveBarDatabase waveDb;
            waveDb.initialise(DbConnectionProvider{dbPool});
            waveDb.initialiseDatabase();

            if(!waveDb.removeFromCache(keys)) {
                qDebug() << "[WaveBar] Unable to remove waveform data";
            }
        });
    }

    void clearCache() const
    {
        const DbConnectionHandler handler{dbPool};
        WaveBarDatabase waveDb;
        waveDb.initialise(DbConnectionProvider{dbPool});

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
    if(p->waveBuilder) {
        p->waveBuilder.reset();
    }
}

void WaveBarPlugin::initialise(const CorePluginContext& context)
{
    p->playerController = context.playerController;
    p->engine           = context.engine;
    p->settings         = context.settingsManager;
}

void WaveBarPlugin::initialise(const GuiPluginContext& context)
{
    p->actionManager  = context.actionManager;
    p->trackSelection = context.trackSelection;
    p->widgetProvider = context.widgetProvider;

    p->waveBarSettings        = std::make_unique<WaveBarSettings>(p->settings);
    p->waveBarSettingsPage    = std::make_unique<WaveBarSettingsPage>(p->settings);
    p->waveBarGuiSettingsPage = std::make_unique<WaveBarGuiSettingsPage>(p->settings);

    QObject::connect(p->waveBarSettingsPage.get(), &WaveBarSettingsPage::clearCache, this,
                     [this]() { p->clearCache(); });

    p->widgetProvider->registerWidget(
        QStringLiteral("WaveBar"), [this]() { return p->createWavebar(); }, tr("Waveform Seekbar"));
    p->widgetProvider->setSubMenus(QStringLiteral("WaveBar"), {tr("Controls")});

    auto* selectionMenu = p->actionManager->actionContainer(::Fooyin::Constants::Menus::Context::TrackSelection);
    auto* utilitiesMenu = p->actionManager->createMenu("Fooyin.Menu.Utilities");
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
