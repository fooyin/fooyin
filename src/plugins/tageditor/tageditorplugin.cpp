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

#include "tageditorplugin.h"

#include "settings/tageditorfieldregistry.h"
#include "settings/tageditorpage.h"
#include "tageditorpanel.h"
#include "tageditorpropertiestab.h"
#include "tagfilldialog.h"

#include <core/engine/audioloader.h>
#include <core/library/musiclibrary.h>
#include <gui/guiconstants.h>
#include <gui/plugins/guiplugincontext.h>
#include <gui/propertiesdialog.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgetprovider.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QMainWindow>
#include <QMenu>

using namespace Qt::StringLiterals;

namespace Fooyin::TagEditor {
namespace {
bool canWriteTracks(const TrackList& tracks, const std::shared_ptr<AudioLoader>& audioLoader)
{
    return !tracks.empty() && std::ranges::all_of(tracks, [&audioLoader](const Track& track) {
        return !track.hasCue() && !track.isInArchive() && audioLoader->canWriteMetadata(track);
    });
}
} // namespace

void TagEditorPlugin::initialise(const CorePluginContext& context)
{
    m_settings    = context.settingsManager;
    m_library     = context.library;
    m_audioLoader = context.audioLoader;

    m_registry = new TagEditorFieldRegistry(m_settings, this);
}

void TagEditorPlugin::initialise(const GuiPluginContext& context)
{
    m_actionManager    = context.actionManager;
    m_trackSelection   = context.trackSelection;
    m_propertiesDialog = context.propertiesDialog;
    m_widgetProvider   = context.widgetProvider;

    m_fieldsPage = new TagEditorFieldsPage(m_registry, m_settings, this);

    m_widgetProvider->registerWidget(u"TagEditor"_s, [this]() { return createPanel(); }, tr("Tag Editor"));

    m_propertiesDialog->insertTab(0, u"Metadata"_s, [this](const TrackList& tracks) { return createEditor(tracks); });

    auto* autoFillValuesAction = new QAction(tr("Automatically fill values…"), this);
    QObject::connect(autoFillValuesAction, &QAction::triggered, this, [this]() {
        const auto* selection = m_trackSelection->selectedSelection();
        if(!selection || !canWriteTracks(selection->tracks, m_audioLoader)) {
            return;
        }

        openFillDialog(selection->tracks, Utils::getMainWindow(), [this](const FillValuesResult& result) {
            if(!result.tracks.empty()) {
                m_library->writeTrackMetadata(result.tracks);
            }
        });
    });

    m_trackSelection->registerTrackContextAction(
        this, TrackContextMenuArea::Track, Constants::Menus::Context::Tagging, "TagEditor.AutoFillValues",
        autoFillValuesAction->text(),
        [this, autoFillValuesAction](QMenu* menu, const TrackSelection& selection) {
            if(selection.tracks.empty()) {
                return;
            }

            autoFillValuesAction->setEnabled(canWriteTracks(selection.tracks, m_audioLoader));
            menu->addAction(autoFillValuesAction);
        },
        Constants::Menus::Context::TaggingRating);
}

TagEditorPropertiesTab* TagEditorPlugin::createEditor(const TrackList& tracks)
{
    auto* tagEditor = new TagEditorPropertiesTab(m_actionManager, m_registry, m_settings);
    tagEditor->setReadOnly(!canWriteTracks(tracks, m_audioLoader));
    tagEditor->setTracks(tracks);
    QObject::connect(tagEditor, &TagEditorPropertiesTab::trackMetadataChanged, m_library,
                     &MusicLibrary::writeTrackMetadata);
    QObject::connect(tagEditor, &TagEditorPropertiesTab::trackStatsChanged, m_library,
                     [this](const TrackList& changedTracks) { m_library->updateTrackStats(changedTracks); });
    return tagEditor;
}

TagEditorPanel* TagEditorPlugin::createPanel()
{
    return new TagEditorPanel(m_actionManager, m_registry, m_library, m_audioLoader, m_trackSelection, m_settings);
}
} // namespace Fooyin::TagEditor

#include "moc_tageditorplugin.cpp"
