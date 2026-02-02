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

#include <core/engine/fadingdefs.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
[[nodiscard]] constexpr double halfPi() noexcept
{
    return std::numbers::pi_v<double> * 0.5;
}
} // namespace

namespace Fooyin::Engine {
double shape01(double progress, FadeCurve curve) noexcept
{
    const double p = std::clamp(progress, 0.0, 1.0);
    const double x = halfPi() * p;

    switch(curve) {
        case FadeCurve::Linear:
            return p;
        case FadeCurve::Sine:
            return std::sin(x);
        case FadeCurve::Cosine:
            return 1.0 - std::cos(x);
        case FadeCurve::Logarithmic:
            return std::log10(1.0 + (9.0 * p));
        case FadeCurve::Exponential:
            return (std::pow(10.0, p) - 1.0) / 9.0;
        case FadeCurve::SmootherStep:
            return p * p * p * (p * ((p * 6.0) - 15.0) + 10.0);
    }

    return p;
}

double gain01(double progress, FadeCurve curve, FadeDirection direction) noexcept
{
    const double p = std::clamp(progress, 0.0, 1.0);
    return (direction == FadeDirection::In) ? shape01(p, curve) : shape01(1.0 - p, curve);
}

QDataStream& operator<<(QDataStream& stream, const FadeSpec& fading)
{
    stream << fading.in;
    stream << fading.out;
    stream << fading.curve;

    return stream;
}

QDataStream& operator>>(QDataStream& stream, FadeSpec& fading)
{
    stream >> fading.in;
    stream >> fading.out;
    stream >> fading.curve;

    return stream;
}

QDataStream& operator<<(QDataStream& stream, const FadingValues& fading)
{
    stream << fading.pause;
    stream << fading.stop;

    return stream;
}

QDataStream& operator>>(QDataStream& stream, FadingValues& fading)
{
    stream >> fading.pause;
    stream >> fading.stop;

    return stream;
}

QDataStream& operator<<(QDataStream& stream, const CrossfadingValues& fading)
{
    stream << fading.manualChange;
    stream << fading.autoChange;
    stream << fading.seek;

    return stream;
}

QDataStream& operator>>(QDataStream& stream, CrossfadingValues& fading)
{
    stream >> fading.manualChange;
    stream >> fading.autoChange;
    if(!stream.atEnd()) {
        stream >> fading.seek;
    }

    return stream;
}
} // namespace Fooyin::Engine
