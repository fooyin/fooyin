/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "playlistorganiser.h"

#include <gui/configdialog.h>

class QCheckBox;

namespace Fooyin {
class ColourButton;
class ScriptLineEdit;

class PlaylistOrganiserConfigDialog : public WidgetConfigDialog<PlaylistOrganiser, PlaylistOrganiser::ConfigData>
{
    Q_OBJECT

public:
    explicit PlaylistOrganiserConfigDialog(PlaylistOrganiser* organiser, QWidget* parent = nullptr);

protected:
    void setConfig(const PlaylistOrganiser::ConfigData& config) override;
    [[nodiscard]] PlaylistOrganiser::ConfigData config() const override;

private:
    ScriptLineEdit* m_leftScript;
    ScriptLineEdit* m_rightScript;
    ColourButton* m_playingTextColour;
    ColourButton* m_playingBackgroundColour;
};
} // namespace Fooyin
