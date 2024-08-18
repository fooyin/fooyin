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

#include "tageditorwidget.h"

#include <core/engine/audioloader.h>
#include <core/library/musiclibrary.h>
#include <gui/plugins/guiplugincontext.h>
#include <gui/propertiesdialog.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgetprovider.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>

#include <QMenu>

namespace Fooyin::TagEditor {
void TagEditorPlugin::initialise(const CorePluginContext& context)
{
    m_settings    = context.settingsManager;
    m_library     = context.library;
    m_audioLoader = context.audioLoader;
}

void TagEditorPlugin::initialise(const GuiPluginContext& context)
{
    m_actionManager    = context.actionManager;
    m_trackSelection   = context.trackSelection;
    m_propertiesDialog = context.propertiesDialog;
    m_widgetProvider   = context.widgetProvider;

    // m_widgetProvider->registerWidget(
    //     QStringLiteral("TagEditor"), [this]() { return createEditor(); }, QStringLiteral("Tag Editor"));

    m_propertiesDialog->insertTab(0, QStringLiteral("Metadata"),
                                  [this](const TrackList& tracks) { return createEditor(tracks); });
}

void TagEditorPlugin::shutdown()
{
    m_audioLoader->destroyThreadInstance();
}

TagEditorWidget* TagEditorPlugin::createEditor(const TrackList& tracks)
{
    const bool canWrite = std::ranges::all_of(tracks, [this](const Track& track) {
        return !track.hasCue() && !track.isInArchive() && m_audioLoader->canWriteMetadata(track);
    });

    auto* tagEditor = new TagEditorWidget(m_actionManager, m_settings);
    tagEditor->setReadOnly(!canWrite);
    tagEditor->setTracks(tracks);
    QObject::connect(tagEditor, &TagEditorWidget::trackMetadataChanged, m_library, &MusicLibrary::writeTrackMetadata);
    QObject::connect(tagEditor, &TagEditorWidget::trackStatsChanged, m_library,
                     [this](const TrackList& changedTracks) { m_library->updateTrackStats(changedTracks); });
    return tagEditor;
}
} // namespace Fooyin::TagEditor

#include "moc_tageditorplugin.cpp"
