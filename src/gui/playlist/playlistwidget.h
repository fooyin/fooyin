/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "gui/fywidget.h"

namespace Fooyin {
class ActionManager;
struct CorePluginContext;
class CoverProvider;
class MusicLibrary;
class PlaylistInteractor;
class PlaylistWidgetPrivate;
class SettingsManager;

class PlaylistWidget : public FyWidget
{
    Q_OBJECT

public:
    PlaylistWidget(ActionManager* actionManager, PlaylistInteractor* playlistInteractor, CoverProvider* coverProvider,
                   const CorePluginContext& core, QWidget* parent = nullptr);
    ~PlaylistWidget() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;
    void finalise() override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    std::unique_ptr<PlaylistWidgetPrivate> p;
};
} // namespace Fooyin
