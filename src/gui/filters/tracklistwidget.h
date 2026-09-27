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

#pragma once

#include <gui/fywidget.h>

namespace Fooyin {
class PlaylistWidget;

namespace Filters {
class FilterController;

class TrackListWidget : public FyWidget
{
    Q_OBJECT

public:
    TrackListWidget(FilterController* controller, PlaylistWidget* playlistWidget, QWidget* parent = nullptr);
    ~TrackListWidget() override;

    [[nodiscard]] Id group() const;
    void setGroup(const Id& group);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void saveCopyLayoutData(QJsonObject& layout, LayoutCopyContext& context, bool isRoot) override;
    void loadLayoutData(const QJsonObject& layout) override;
    void finalise() override;
    void searchEvent(const SearchRequest& request) override;
    void layoutEditingMenu(QMenu* menu) override;

Q_SIGNALS:
    void groupChanged();
    void viewerDeleted();
    void requestEditConnections();

protected:
    void openConfigDialog() override;

private:
    void updateHeader();

    PlaylistWidget* m_playlistWidget;
    Id m_group;
    int m_sourceTrackCount;
    bool m_haveResult;
};
} // namespace Filters
} // namespace Fooyin
