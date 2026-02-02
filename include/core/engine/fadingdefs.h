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

#include "fycore_export.h"

#include <QDataStream>
#include <QMetaType>

#include <cstdint>

namespace Fooyin::Engine {
//! Fade envelope curve used by output and transition fades.
enum class FadeCurve : uint8_t
{
    Linear = 0,
    Sine,
    Cosine,
    Logarithmic,
    Exponential,
    SmootherStep
};

enum class FadeDirection : uint8_t
{
    In = 0,
    Out
};

//! Returns unit-interval curve shape S(p) where S(0)=0 and S(1)=1.
FYCORE_EXPORT double shape01(double progress, FadeCurve curve) noexcept;
//! Applies fade direction policy using same-feel semantics: in=S(p), out=S(1-p).
FYCORE_EXPORT double gain01(double progress, FadeCurve curve, FadeDirection direction) noexcept;

//! Fade-in/fade-out durations and envelope curve.
struct FadeSpec
{
    int in{1000};
    int out{1000};
    FadeCurve curve{FadeCurve::Linear};
};

//! Fading policy group used for transport operations.
struct FadingValues
{
    FadeSpec pause;
    FadeSpec stop;
};

//! Crossfade policy for manual/automatic track changes and seek transitions.
struct CrossfadingValues
{
    FadeSpec manualChange;
    FadeSpec autoChange;
    FadeSpec seek{80, 80, FadeCurve::Linear};
};

FYCORE_EXPORT QDataStream& operator<<(QDataStream& stream, const FadeSpec& fading);
FYCORE_EXPORT QDataStream& operator>>(QDataStream& stream, FadeSpec& fading);
FYCORE_EXPORT QDataStream& operator<<(QDataStream& stream, const FadingValues& fading);
FYCORE_EXPORT QDataStream& operator>>(QDataStream& stream, FadingValues& fading);
FYCORE_EXPORT QDataStream& operator<<(QDataStream& stream, const CrossfadingValues& fading);
FYCORE_EXPORT QDataStream& operator>>(QDataStream& stream, CrossfadingValues& fading);
} // namespace Fooyin::Engine

Q_DECLARE_METATYPE(Fooyin::Engine::FadeCurve)
Q_DECLARE_METATYPE(Fooyin::Engine::FadingValues)
Q_DECLARE_METATYPE(Fooyin::Engine::CrossfadingValues)
