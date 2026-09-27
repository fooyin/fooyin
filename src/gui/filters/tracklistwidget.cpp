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

#include "tracklistwidget.h"

#include "filtercontroller.h"

#include "playlist/playlistwidget.h"

#include <QAction>
#include <QJsonObject>
#include <QMenu>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin::Filters {
TrackListWidget::TrackListWidget(FilterController* controller, PlaylistWidget* playlistWidget, QWidget* parent)
    : FyWidget{parent}
    , m_playlistWidget{playlistWidget}
    , m_sourceTrackCount{0}
    , m_haveResult{false}
{
    setObjectName(TrackListWidget::name());
    setFeature(ExclusiveSearch);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_playlistWidget);

    m_playlistWidget->setConfigDialogTitle(tr("Track Viewer Settings"));
    m_playlistWidget->setHeaderText(tr("Tracks"));

    QObject::connect(controller, &FilterController::filterGroupChanged, this,
                     [this](const Id& group, const TrackList& tracks, bool hasActiveFilters) {
                         if(group == m_group) {
                             m_sourceTrackCount = hasActiveFilters ? static_cast<int>(tracks.size()) : 0;
                             m_haveResult       = true;
                             m_playlistWidget->setTracks(hasActiveFilters ? tracks : TrackList{});
                         }
                     });
    QObject::connect(controller, &FilterController::filterGroupRemoved, this, [this](const Id& group) {
        if(group == m_group) {
            m_sourceTrackCount = 0;
            m_haveResult       = true;
            m_playlistWidget->setTracks({});
        }
    });
    QObject::connect(m_playlistWidget, &PlaylistWidget::headerMenuAboutToShow, this, [this](QMenu* menu) {
        auto* manageGroups = menu->addAction(tr("Manage filter groups…"));
        QObject::connect(manageGroups, &QAction::triggered, this, &TrackListWidget::requestEditConnections);
    });
    QObject::connect(m_playlistWidget->model(), &PlaylistModel::playlistLoaded, this, &TrackListWidget::updateHeader);
}

TrackListWidget::~TrackListWidget()
{
    Q_EMIT viewerDeleted();
}

Id TrackListWidget::group() const
{
    return m_group;
}

void TrackListWidget::setGroup(const Id& group)
{
    if(std::exchange(m_group, group) != group) {
        m_sourceTrackCount = 0;
        m_haveResult       = false;
        m_playlistWidget->setHeaderText(tr("Tracks"));
        m_playlistWidget->setTracks({});

        Q_EMIT groupChanged();
    }
}

void TrackListWidget::updateHeader()
{
    if(!m_haveResult) {
        return;
    }

    const int trackCount = m_playlistWidget->trackCount();
    const QString header = trackCount == m_sourceTrackCount
                             ? tr("%Ln track(s)", nullptr, trackCount)
                             : tr("%1 of %Ln track(s)", nullptr, m_sourceTrackCount).arg(trackCount);
    m_playlistWidget->setHeaderText(header);
}

QString TrackListWidget::name() const
{
    return tr("Track Viewer");
}

QString TrackListWidget::layoutName() const
{
    return u"TrackViewer"_s;
}

void TrackListWidget::saveLayoutData(QJsonObject& layout)
{
    m_playlistWidget->saveLayoutData(layout);

    layout["Group"_L1] = m_group.name();
}

void TrackListWidget::saveCopyLayoutData(QJsonObject& layout, LayoutCopyContext& context, bool isRoot)
{
    FyWidget::saveCopyLayoutData(layout, context, isRoot);

    if(!isRoot) {
        const QString group = layout.value("Group"_L1).toString();
        if(!group.isEmpty()) {
            layout["Group"_L1] = context.mappedString(u"Fooyin.Filters.FilterGroup"_s, group);
        }
    }
}

void TrackListWidget::loadLayoutData(const QJsonObject& layout)
{
    m_playlistWidget->loadLayoutData(layout);

    if(layout.contains("Group"_L1)) {
        setGroup(Id{layout.value("Group"_L1).toString()});
    }
}

void TrackListWidget::finalise()
{
    m_playlistWidget->finalise();
}

void TrackListWidget::searchEvent(const SearchRequest& request)
{
    m_playlistWidget->searchEvent(request);
}

void TrackListWidget::layoutEditingMenu(QMenu* menu)
{
    auto* editConnections = menu->addAction(tr("Manage filter groups…"));
    QObject::connect(editConnections, &QAction::triggered, this, &TrackListWidget::requestEditConnections);
}

void TrackListWidget::openConfigDialog()
{
    m_playlistWidget->openConfigDialog(tr("Track Viewer Settings"));
}
} // namespace Fooyin::Filters

#include "moc_tracklistwidget.cpp"
