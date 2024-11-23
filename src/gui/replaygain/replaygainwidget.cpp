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

#include "replaygainwidget.h"

#include "replaygaindelegate.h"
#include "replaygainmodel.h"
#include "replaygainview.h"

#include <core/library/musiclibrary.h>
#include <utils/actions/actionmanager.h>

#include <QHeaderView>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
ReplayGainWidget::ReplayGainWidget(MusicLibrary* library, const TrackList& tracks, bool readOnly, QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_library{library}
    , m_view{new ReplayGainView(this)}
    , m_model{new ReplayGainModel(readOnly, this)}
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    m_view->setItemDelegate(new ReplayGainDelegate(this));
    m_view->setModel(m_model);
    m_model->resetModel(tracks);

    if(tracks.size() == 1) {
        m_view->header()->setSectionResizeMode(QHeaderView::Stretch);
    }
    else {
        m_view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        for(int i{1}; i < m_view->header()->count(); ++i) {
            m_view->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        }
    }
}

QString ReplayGainWidget::name() const
{
    return tr("ReplayGain Editor");
}

QString ReplayGainWidget::layoutName() const
{
    return u"ReplayGainEditor"_s;
}

void ReplayGainWidget::apply()
{
    const TrackList changedTracks = m_model->applyChanges();
    if(!changedTracks.empty()) {
        m_library->writeTrackMetadata(changedTracks);
    }
}
} // namespace Fooyin

#include "moc_replaygainwidget.cpp"
