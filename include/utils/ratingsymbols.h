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

#include <QString>

namespace Fooyin {
struct RatingStarSymbols
{
    QString fullStarSymbol;
    QString halfStarSymbol;
    QString emptyStarSymbol;
};

[[nodiscard]] inline const RatingStarSymbols& defaultRatingStarSymbols()
{
    static const RatingStarSymbols Symbols{
        .fullStarSymbol  = QString{QChar{u'\u2605'}},
        .halfStarSymbol  = QString{QChar{u'\u2BEA'}},
        .emptyStarSymbol = QString{QChar{u'\u2606'}},
    };
    return Symbols;
}

[[nodiscard]] inline const QString& defaultRatingFullStarSymbol()
{
    return defaultRatingStarSymbols().fullStarSymbol;
}

[[nodiscard]] inline const QString& defaultRatingHalfStarSymbol()
{
    return defaultRatingStarSymbols().halfStarSymbol;
}

[[nodiscard]] inline const QString& defaultRatingEmptyStarSymbol()
{
    return defaultRatingStarSymbols().emptyStarSymbol;
}
} // namespace Fooyin
