/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include <QWidget>

namespace Gui::Widgets {
class VolumeControl : public QWidget
{
    Q_OBJECT

public:
    explicit VolumeControl(QWidget* parent = nullptr);
    ~VolumeControl() override;

    void setupUi();

    void updateVolume(double value);
    void mute();

signals:
    void volumeUp();
    void volumeDown();
    void volumeChanged(double value);

protected:
    void showVolumeMenu();

private:
    struct Private;
    std::unique_ptr<VolumeControl::Private> p;
};
} // namespace Gui::Widgets
