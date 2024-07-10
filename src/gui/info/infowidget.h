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

#include <QWidget>

namespace Fooyin {
class InfoView;
class InfoModel;
class PlayerController;
class SettingsManager;
class TrackSelectionController;

class InfoWidget : public PropertiesTabWidget
{
    Q_OBJECT

public:
    explicit InfoWidget(PlayerController* playerController, TrackSelectionController* selectionController,
                        SettingsManager* settings, QWidget* parent = nullptr);
    ~InfoWidget() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void resetModel();
    void resetView();

    TrackSelectionController* m_selectionController;
    PlayerController* m_playerController;
    SettingsManager* m_settings;

    InfoView* m_view;
    InfoModel* m_model;
    SelectionDisplay m_displayOption;
    int m_scrollPos;
};
} // namespace Fooyin
