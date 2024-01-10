/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#pragma once

#include "filterfwd.h"

#include <core/trackfwd.h>
#include <gui/fywidget.h>

namespace Fooyin {
class SettingsManager;
class AutoHeaderView;

namespace Filters {
class FilterColumnRegistry;

class FilterWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit FilterWidget(FilterColumnRegistry* columnRegistry, SettingsManager* settings, QWidget* parent = nullptr);
    ~FilterWidget() override;

    [[nodiscard]] Id group() const;
    [[nodiscard]] int index() const;
    [[nodiscard]] bool multipleColumns() const;
    [[nodiscard]] TrackList tracks() const;

    void setGroup(const Id& group);
    void setIndex(int index);
    void setTracks(const TrackList& tracks);
    void clearTracks();

    void reset(const TrackList& tracks);

    void setScrollbarEnabled(bool enabled);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;
    void finalise() override;

    void searchEvent(const QString& search) override;

    void tracksAdded(const TrackList& tracks);
    void tracksUpdated(const TrackList& tracks);
    void tracksRemoved(const TrackList& tracks);

signals:
    void doubleClicked(const QString& playlistName);
    void middleClicked(const QString& playlistName);

    void filterDeleted();
    void filterUpdated();
    void selectionChanged(const QString& playlistName);
    void requestHeaderMenu(AutoHeaderView* header, const QPoint& pos);
    void requestContextMenu(const QPoint& pos);
    void requestEditConnections();
    void requestSearch(const QString& search);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Filters
} // namespace Fooyin
