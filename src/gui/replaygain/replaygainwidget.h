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

#include <gui/propertiesdialog.h>

namespace Fooyin {
class MusicLibrary;
class ReplayGainModel;
class ReplayGainView;

class ReplayGainWidget : public PropertiesTabWidget
{
    Q_OBJECT

public:
    ReplayGainWidget(MusicLibrary* library, const TrackList& tracks, bool readOnly, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void apply() override;

private:
    MusicLibrary* m_library;
    ReplayGainView* m_view;
    ReplayGainModel* m_model;
};
} // namespace Fooyin
