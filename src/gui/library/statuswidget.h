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

#include <QWidget>

class QHBoxLayout;

namespace Fy {

namespace Utils {
class ClickableLabel;
}

namespace Core {
namespace Library {
class MusicLibrary;
}
namespace Player {
class PlayerManager;
enum PlayState : int;
} // namespace Player
} // namespace Core

namespace Gui::Widgets {
class StatusWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit StatusWidget(Core::Library::MusicLibrary* library, Core::Player::PlayerManager* playerManager,
                          QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;

signals:
    void clicked();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void labelClicked();
    void stateChanged(Core::Player::PlayState state);
    void scanProgressChanged(int progress);

    Core::Library::MusicLibrary* m_library;
    Core::Player::PlayerManager* m_playerManager;

    QHBoxLayout* m_layout;
    Utils::ClickableLabel* m_iconLabel;
    QPixmap m_icon;
    Utils::ClickableLabel* m_playing;
};
} // namespace Gui::Widgets
} // namespace Fy
