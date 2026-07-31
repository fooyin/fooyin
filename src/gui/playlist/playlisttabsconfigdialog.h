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

#include "playlisttabs.h"

#include <gui/configdialog.h>

class QCheckBox;
class QComboBox;

namespace Fooyin {
class PlaylistTabsConfigDialog : public WidgetConfigDialog<PlaylistTabs, PlaylistTabs::ConfigData>
{
    Q_OBJECT

public:
    explicit PlaylistTabsConfigDialog(PlaylistTabs* playlistTabs, QWidget* parent = nullptr);

protected:
    [[nodiscard]] PlaylistTabs::ConfigData config() const override;
    void setConfig(const PlaylistTabs::ConfigData& config) override;
    void mergeExternalConfig(const PlaylistTabs::ConfigData& previous,
                             const PlaylistTabs::ConfigData& current) override;

private:
    QComboBox* m_position;
    QCheckBox* m_expand;
    QCheckBox* m_showAddButton;
    QCheckBox* m_showClearButton;
    QCheckBox* m_showCloseButton;
    QCheckBox* m_closeOnMiddleClick;
};
} // namespace Fooyin
