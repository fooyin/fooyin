/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <luket@pm.me>
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
#include <gui/propertiesdialog.h>

class QJsonObject;

namespace Fooyin {
class ActionManager;
class Application;
class InfoPanel;
class LibraryManager;
class TrackSelectionController;

class InfoWidget : public FyWidget
{
    Q_OBJECT

public:
    InfoWidget(const TrackList& tracks, LibraryManager* libraryManager, ActionManager* actionManager,
               QWidget* parent = nullptr);
    InfoWidget(Application* app, ActionManager* actionManager, TrackSelectionController* selectionController,
               QWidget* parent = nullptr);
    ~InfoWidget() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;
    void finalise() override;

private:
    InfoPanel* m_panel;
};

class InfoPropertiesTab : public PropertiesTabWidget
{
    Q_OBJECT

public:
    InfoPropertiesTab(const TrackList& tracks, LibraryManager* libraryManager, ActionManager* actionManager,
                      QWidget* parent = nullptr);
    ~InfoPropertiesTab() override;

    void updateTracks(const TrackList& tracks) override;
    [[nodiscard]] bool canApply() const override;

private:
    InfoPanel* m_panel;
};
} // namespace Fooyin
