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

#include "spectrogramconfig.h"

#include <QWidget>

namespace Fooyin::Spectrogram {
class SpectrogramView : public QWidget
{
public:
    explicit SpectrogramView(QWidget* parent = nullptr)
        : QWidget{parent}
    { }

    virtual void applyConfig(const SpectrogramConfig& config) = 0;

    virtual bool cancelSeeking()
    {
        return false;
    }
};
} // namespace Fooyin::Spectrogram
