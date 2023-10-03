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

#pragma once

#include <core/models/trackfwd.h>

#include <gui/fywidget.h>

namespace Fy {
namespace Utils {
class SettingsManager;
}

namespace Filters {
struct LibraryFilter;

class FilterWidget : public Gui::Widgets::FyWidget
{
    Q_OBJECT

public:
    explicit FilterWidget(Utils::SettingsManager* settings,
                          QWidget* parent = nullptr);
    ~FilterWidget() override;

    void setupConnections();

    void changeFilter(const LibraryFilter& filter);
    void reset(const Core::TrackList& tracks);

    void setScrollbarEnabled(bool enabled);

    void customHeaderMenuRequested(QPoint pos);

    [[nodiscard]] QString name() const override;
    void saveLayout(QJsonArray& array) override;
    void loadLayout(const QJsonObject& object) override;

signals:
    void doubleClicked(const QString& playlistName);
    void middleClicked(const QString& playlistName);

    void selectionChanged(const LibraryFilter& filter, const QString& playlistName);
    void filterDeleted(const LibraryFilter& filter);
    void requestFieldChange(const LibraryFilter& filter, const QString& field);
    void requestHeaderMenu(const LibraryFilter& filter, QPoint pos);
    void requestContextMenu(const LibraryFilter& filter, QPoint pos);

    void tracksAdded(const Core::TrackList& tracks);
    void tracksUpdated(const Core::TrackList& tracks);
    void tracksRemoved(const Core::TrackList& tracks);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Filters
} // namespace Fy
