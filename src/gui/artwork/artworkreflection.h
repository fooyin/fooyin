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

class QPainter;
class QPixmap;
class QRect;

namespace Fooyin::Gui {
struct ArtworkReflectionOptions
{
    bool enabled{false};
    int height{33};
    int opacity{35};
    int gap{3};

    bool operator==(const ArtworkReflectionOptions&) const = default;
};

int artworkReflectionHeight(int artworkHeight, const ArtworkReflectionOptions& options);
void drawArtworkReflection(QPainter& painter, const QPixmap& source, const QPixmap& scaledImage,
                           const QRect& artworkRect, int cornerRadiusPercent, const ArtworkReflectionOptions& options);
} // namespace Fooyin::Gui
