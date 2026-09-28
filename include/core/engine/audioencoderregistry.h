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

#include <core/engine/audioencoder.h>

#include <optional>
#include <vector>

namespace Fooyin {
class FYCORE_EXPORT AudioEncoderRegistry
{
public:
    enum class BackendPriority : int8_t
    {
        Fallback = -100,
        Normal   = 0,
    };

    void addEncoderBackend(const QString& id, const QString& name, EncoderCreator creator,
                           BackendPriority priority = BackendPriority::Normal);

    [[nodiscard]] std::vector<AudioEncoderInfo> availableEncoders() const;
    [[nodiscard]] std::vector<AudioEncoderInfo> preferredEncoders() const;
    [[nodiscard]] std::optional<AudioEncoderInfo> encoderInfo(const QString& encoderId) const;
    [[nodiscard]] std::unique_ptr<AudioEncoder> createEncoder(const QString& encoderId) const;

    void reset();

private:
    struct Backend
    {
        QString id;
        QString name;
        EncoderCreator creator;
        BackendPriority priority{BackendPriority::Normal};
    };

    std::vector<Backend> m_backends;
};
} // namespace Fooyin
