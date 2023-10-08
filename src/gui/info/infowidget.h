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

#include <gui/propertiesdialog.h>

#include <QHBoxLayout>
#include <QWidget>

class QTreeView;

namespace Fy {

namespace Utils {
class SettingsManager;
}

namespace Core {
class Track;

namespace Player {
class PlayerManager;
} // namespace Player
} // namespace Core

namespace Gui {
class TrackSelectionController;

namespace Widgets::Info {
class InfoModel;

class InfoWidget : public PropertiesTabWidget
{
public:
    explicit InfoWidget(Core::Player::PlayerManager* playerManager, TrackSelectionController* selectionController,
                        Utils::SettingsManager* settings, QWidget* parent = nullptr);

    bool isHeaderHidden();
    void setHeaderHidden(bool showHeader);

    bool isScrollbarHidden();
    void setScrollbarHidden(bool showScrollBar);

    bool altRowColors();
    void setAltRowColors(bool altColours);

    [[nodiscard]] QString name() const override;
    void layoutEditingMenu(Utils::ActionContainer* menu) override;

protected:
    void setupUi();
    void spanHeaders();

private:
    Core::Player::PlayerManager* m_playerManager;
    Utils::SettingsManager* m_settings;

    QHBoxLayout* m_layout;
    QTreeView* m_view;
    InfoModel* m_model;
};
} // namespace Widgets::Info
} // namespace Gui
} // namespace Fy
