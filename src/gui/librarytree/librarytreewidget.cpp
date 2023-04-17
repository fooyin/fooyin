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

#include "librarytreewidget.h"

#include "gui/guisettings.h"
#include "librarytreemodel.h"

#include <core/library/musiclibrary.h>

#include <utils/settings/settingsmanager.h>

#include <QTreeView>
#include <QVBoxLayout>

namespace Fy::Gui::Widgets {

LibraryTreeWidget::LibraryTreeWidget(Core::Library::MusicLibrary* library, Utils::SettingsManager* settings,
                                     QWidget* parent)
    : FyWidget{parent}
    , m_library{library}
    , m_settings{settings}
    , m_layout{new QVBoxLayout(this)}
    , m_trackTree{new QTreeView(this)}
    , m_model{new LibraryTreeModel(this)}
{
    setObjectName(LibraryTreeWidget::name());

    m_layout->setContentsMargins(0, 0, 0, 0);
    m_trackTree->setUniformRowHeights(true);
    m_trackTree->setModel(m_model);
    m_layout->addWidget(m_trackTree);

    m_model->setGroupScript(m_settings->value<Settings::LibraryTreeGrouping>());
    m_settings->subscribe<Settings::LibraryTreeGrouping>(this, &LibraryTreeWidget::groupingChanged);

    reset();

    QObject::connect(m_library, &Core::Library::MusicLibrary::tracksLoaded, this, &LibraryTreeWidget::reset);
    QObject::connect(m_library, &Core::Library::MusicLibrary::tracksSorted, this, &LibraryTreeWidget::reset);
    QObject::connect(m_library, &Core::Library::MusicLibrary::tracksDeleted, this, &LibraryTreeWidget::reset);
    QObject::connect(m_library, &Core::Library::MusicLibrary::tracksAdded, this, &LibraryTreeWidget::reset);
    QObject::connect(m_library, &Core::Library::MusicLibrary::libraryRemoved, this, &LibraryTreeWidget::reset);
    QObject::connect(m_library, &Core::Library::MusicLibrary::libraryChanged, this, &LibraryTreeWidget::reset);
}

QString LibraryTreeWidget::name() const
{
    return "Track Tree";
}

QString LibraryTreeWidget::layoutName() const
{
    return "TrackTree";
}

void LibraryTreeWidget::groupingChanged(const QString& script)
{
    m_model->setGroupScript(script);
    reset();
}

void LibraryTreeWidget::reset()
{
    m_model->reload(m_library->tracks());
}

} // namespace Fy::Gui::Widgets
