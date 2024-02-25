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

#pragma once

#include "dirproxymodel.h"

#include <gui/fywidget.h>

#include <QFileIconProvider>

class QFileSystemModel;
class QTreeView;

namespace Fooyin {
class PlaylistController;
class SettingsManager;
class TrackSelectionController;
class Playlist;

class DirBrowser : public FyWidget
{
    Q_OBJECT

public:
    explicit DirBrowser(TrackSelectionController* selectionController, PlaylistController* playlistController,
                        SettingsManager* settings, QWidget* parent = nullptr);
    ~DirBrowser() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void handleDoubleClick(const QModelIndex& index);
    void updateDir(const QString& dir);

    PlaylistController* m_playlistController;
    TrackSelectionController* m_selectionController;
    SettingsManager* m_settings;

    std::unique_ptr<QFileIconProvider> m_iconProvider;

    QTreeView* m_dirTree;
    QFileSystemModel* m_model;
    DirProxyModel* m_proxyModel;

    Playlist* m_playlist;
    QString m_playlistDir;
};
} // namespace Fooyin
