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

#include <QByteArray>
#include <QColor>
#include <QObject>
#include <QPalette>

#include <cstdint>
#include <map>

namespace Fooyin {
class CoverRepository;
class PlayerController;
class SettingsManager;

enum class ArtworkColourMode : uint8_t
{
    Off = 0,
    Accent,
    Tonal,
};

class ArtworkPaletteController : public QObject
{
    Q_OBJECT

public:
    ArtworkPaletteController(CoverRepository* covers, PlayerController* player, SettingsManager* settings,
                             QObject* parent = nullptr);

    [[nodiscard]] QPalette palette(const QPalette& base) const;

Q_SIGNALS:
    void paletteChanged();

private:
    void reloadCover();
    void setColour(const QColor& colour);

    CoverRepository* m_covers;
    PlayerController* m_player;
    ArtworkColourMode m_mode;
    quint64 m_requestId;
    QColor m_colour;
    std::map<QByteArray, QColor> m_colourCache;
};
} // namespace Fooyin
