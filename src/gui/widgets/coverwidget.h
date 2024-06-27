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

#include <core/track.h>
#include <gui/fywidget.h>

#include <QPixmap>

class QLabel;
class QTimer;

namespace Fooyin {
class CoverProvider;
class PlayerController;
class SettingsManager;
class TagLoader;
class TrackSelectionController;

class CoverWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit CoverWidget(PlayerController* playerController, TrackSelectionController* trackSelection,
                         std::shared_ptr<TagLoader> tagLoader, SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void rescaleCover() const;
    void reloadCover();

    PlayerController* m_playerController;
    TrackSelectionController* m_trackSelection;
    SettingsManager* m_settings;
    CoverProvider* m_coverProvider;

    CoverDisplay m_displayOption;
    Track::Cover m_coverType;
    bool m_keepAspectRatio;
    QTimer* m_resizeTimer;

    QLabel* m_coverLabel;
    QPixmap m_cover;
};
} // namespace Fooyin
