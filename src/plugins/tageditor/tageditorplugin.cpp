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

#include <core/constants.h>

#include <gui/dialog/propertiesdialog.h>
#include <gui/guiconstants.h>
#include <gui/widgetfactory.h>

#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>

#include <QMenu>

namespace Fy::TagEditor {
void TagEditorPlugin::initialise(const Core::CorePluginContext& context)
{
    m_actionManager = context.actionManager;
    m_settings      = context.settingsManager;
}

void TagEditorPlugin::initialise(const Gui::GuiPluginContext& context)
{
    m_trackSelection   = context.trackSelection;
    m_propertiesDialog = context.propertiesDialog;
    m_factory          = context.widgetFactory;

    //    m_factory->registerClass<TagEditorWidget>(
    //        "TagEditor",
    //        [this]() {
    //            return new TagEditorWidget(m_trackSelection, m_settings);
    //        },
    //        "Tag Editor");

    m_propertiesDialog->insertTab(0, "Metadata", [this]() {
        return new TagEditorWidget(m_trackSelection, m_settings);
    });
}

void TagEditorPlugin::shutdown() { }
} // namespace Fy::TagEditor
