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

class QHBoxLayout;
class QLabel;

namespace Core {
namespace Library {
class MusicLibrary;
}
namespace Player {
class PlayerManager;
}
} // namespace Core

namespace Gui::Widgets {
class CoverWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit CoverWidget(Core::Library::MusicLibrary* library, Core::Player::PlayerManager* playerManager,
                         QWidget* parent = nullptr);
    ~CoverWidget() override;

    [[nodiscard]] QString name() const override;

protected:
    void setupUi();
    void resizeEvent(QResizeEvent* event) override;

    void reloadCover();

private:
    Core::Library::MusicLibrary* m_library;
    Core::Player::PlayerManager* m_playerManager;

    QHBoxLayout* m_layout;
    QLabel* m_coverLabel;
    QString m_coverPath;
    QPixmap m_cover;
    bool m_hasCover;
};
} // namespace Gui::Widgets
