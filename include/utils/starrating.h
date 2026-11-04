/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "fyutils_export.h"

#include <utils/ratingsymbols.h>

#include <QColor>
#include <QMetaType>
#include <QSharedDataPointer>
#include <QSize>
#include <QVariant>

#include <array>
#include <optional>

class QFont;
class QPainter;
class QPalette;
class QRect;

namespace Fooyin {
using RatingStarColours = std::array<QColor, 5>;

class StarRatingPrivate;

class FYUTILS_EXPORT StarRating
{
public:
    enum class EditMode : uint8_t
    {
        Editable,
        ReadOnly,
    };

    StarRating();
    StarRating(float rating, int maxStarCount);
    StarRating(float rating, int maxStarCount, int scale);
    StarRating(float rating, int maxStarCount, int scale, const RatingStarColours& colours);
    StarRating(float rating, int maxStarCount, int scale, const RatingStarColours& colours, const QColor& unratedColour,
               std::optional<RatingStarSymbols> symbols = {});
    ~StarRating();

    StarRating(const StarRating& other);
    StarRating& operator=(const StarRating& other);
    StarRating(StarRating&& other) noexcept;
    StarRating& operator=(StarRating&& other) noexcept;

    [[nodiscard]] float rating() const;
    [[nodiscard]] int maxStarCount() const;
    [[nodiscard]] int starScale() const;
    [[nodiscard]] Qt::Orientation orientation() const;

    void setRating(float rating);
    void setMaxStarCount(int maxStarCount);
    void setStarScale(int scale);
    void setOrientation(Qt::Orientation orientation);
    void setSymbolFont(const QFont& font);

    void paint(QPainter* painter, const QRect& rect, const QPalette& palette, EditMode mode,
               Qt::Alignment alignment = Qt::AlignLeft, bool selected = false) const;
    [[nodiscard]] QSize sizeHint() const;

    operator QVariant() const
    {
        return QVariant::fromValue(*this);
    }

private:
    QSharedDataPointer<StarRatingPrivate> p;
};
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::StarRating)
