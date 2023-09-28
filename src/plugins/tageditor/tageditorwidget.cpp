/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "tageditorwidget.h"

#include "tageditormodel.h"

#include <core/library/musiclibrary.h>

#include <utils/settings/settingsmanager.h>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QTableView>

namespace Fy::TagEditor {
TagEditorWidget::TagEditorWidget(Gui::TrackSelectionController* trackSelection, Utils::SettingsManager* settings,
                                 QWidget* parent)
    : FyWidget{parent}
    , m_trackSelection{trackSelection}
    , m_settings{settings}
    , m_view{new QTableView(this)}
    , m_model{new TagEditorModel(m_trackSelection, m_settings, this)}
{
    setObjectName(TagEditorWidget::name());
    setWindowFlags(Qt::CustomizeWindowHint | Qt::Dialog);
    resize(600, 720);
    setMinimumSize(300, 400);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    m_view->setModel(m_model);

    m_view->horizontalHeader()->setStretchLastSection(true);
    m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_view->horizontalHeader()->setSectionsClickable(false);
    m_view->verticalHeader()->setVisible(false);
    m_view->resizeColumnsToContents();
    m_view->resizeRowsToContents();
}

QString TagEditorWidget::name() const
{
    return "Tag Editor";
}

QString TagEditorWidget::layoutName() const
{
    return "TagEditor";
}
} // namespace Fy::TagEditor
