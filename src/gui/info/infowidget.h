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

#include "gui/fywidget.h"
#include "gui/info/infomodel.h"
#include "gui/info/infoview.h"

#include <QHBoxLayout>
#include <QWidget>

namespace Core {
class SettingsManager;
class Track;

namespace Player {
class PlayerManager;
} // namespace Player
} // namespace Core

namespace Gui::Widgets {
class InfoWidget : public FyWidget
{
public:
    explicit InfoWidget(QWidget* parent = nullptr);
    ~InfoWidget() override;

    bool isHeaderHidden();
    void setHeaderHidden(bool showHeader);

    bool isScrollbarHidden();
    void setScrollbarHidden(bool showScrollBar);

    bool altRowColors();
    void setAltRowColors(bool altColours);

    [[nodiscard]] QString name() const override;
    void layoutEditingMenu(Core::ActionContainer* menu) override;

protected:
    void setupUi();
    void spanHeaders();
    static void refreshTrack(Core::Track* track);

private:
    Core::SettingsManager* m_settings;
    Core::Player::PlayerManager* m_playerManager;

    QHBoxLayout* m_layout;
    InfoView m_view;
    InfoModel m_model;
};
} // namespace Gui::Widgets
