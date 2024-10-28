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

#include "internalguisettings.h"

#include <gui/propertiesdialog.h>

#include <QBasicTimer>
#include <QWidget>

namespace Fooyin {
class Application;
class InfoFilterModel;
class InfoModel;
class InfoView;
class LibraryManager;
class PlayerController;
class SettingsManager;
class TrackSelectionController;

class InfoWidget : public PropertiesTabWidget
{
    Q_OBJECT

public:
    InfoWidget(const TrackList& tracks, LibraryManager* libraryManager, QWidget* parent = nullptr);
    InfoWidget(Application* app, TrackSelectionController* selectionController, QWidget* parent = nullptr);
    ~InfoWidget() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

    [[nodiscard]] bool canApply() const override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    void resetModel();
    void resetView();

    TrackSelectionController* m_selectionController;
    PlayerController* m_playerController;
    SettingsManager* m_settings;

    InfoView* m_view;
    InfoFilterModel* m_proxyModel;
    InfoModel* m_model;
    QBasicTimer m_resetTimer;
    SelectionDisplay m_displayOption;
    int m_scrollPos;
};
} // namespace Fooyin
