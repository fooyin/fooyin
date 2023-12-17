/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/library/musiclibrary.h>
#include <gui/plugins/guiplugincontext.h>
#include <gui/propertiesdialog.h>
#include <gui/widgetfactory.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>

#include <QMenu>

namespace Fooyin::TagEditor {
struct TagEditorPlugin::Private
{
    ActionManager* actionManager;
    MusicLibrary* library;
    TrackSelectionController* trackSelection;
    PropertiesDialog* propertiesDialog;
    WidgetFactory* factory;
    SettingsManager* settings;
};

TagEditorPlugin::TagEditorPlugin()
    : p{std::make_unique<Private>()}
{ }

TagEditorPlugin::~TagEditorPlugin() = default;

void TagEditorPlugin::initialise(const CorePluginContext& context)
{
    p->settings = context.settingsManager;
    p->library  = context.library;
}

void TagEditorPlugin::initialise(const GuiPluginContext& context)
{
    p->actionManager    = context.actionManager;
    p->trackSelection   = context.trackSelection;
    p->propertiesDialog = context.propertiesDialog;
    p->factory          = context.widgetFactory;

    //    p->factory->registerClass<TagEditorWidget>(
    //        "TagEditor",
    //        [this]() {
    //            return new TagEditorWidget(p->trackSelection, p->library, p->settings);
    //        },
    //        "Tag Editor");

    p->propertiesDialog->insertTab(0, QStringLiteral("Metadata"), [this]() {
        auto* tagEditor = new TagEditorWidget(p->actionManager, p->trackSelection, p->settings);
        QObject::connect(tagEditor, &TagEditorWidget::trackMetadataChanged, p->library,
                         &MusicLibrary::updateTrackMetadata);
        return tagEditor;
    });
}
} // namespace Fooyin::TagEditor

#include "moc_tageditorplugin.cpp"
