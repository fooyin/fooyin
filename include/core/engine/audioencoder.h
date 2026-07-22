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

#include <core/engine/audiobuffer.h>
#include <core/engine/conversion/conversiondefs.h>

#include <QString>

#include <functional>
#include <vector>

namespace Fooyin {
struct EncoderParameterRange
{
    double minimum{0.0};
    double maximum{0.0};
    double step{1.0};

    [[nodiscard]] bool isValid() const
    {
        return maximum > minimum && step > 0.0;
    }
};

struct EncoderCapabilities
{
    std::vector<EncoderMode> modes;
    EncoderParameterRange bitrateKbps;
    EncoderParameterRange quality;
    EncoderParameterRange compressionLevel;
};

struct AudioEncoderInfo
{
    QString id;
    QString backendId;
    QString name;
    QString description;
    EncoderProfile profile;
    EncoderCapabilities capabilities{};
    std::function<int(const EncoderProfile&)> estimateBitrateKbps;
    bool supportsMetadata{false};
    bool supportsPictures{false};
};

class FYCORE_EXPORT AudioEncoder
{
public:
    struct Result
    {
        bool ok{false};
        QString error;

        static Result success()
        {
            return {.ok = true, .error = {}};
        }

        static Result failure(QString message)
        {
            return {.ok = false, .error = std::move(message)};
        }
    };

    virtual ~AudioEncoder() = default;

    [[nodiscard]] virtual std::vector<AudioEncoderInfo> availableEncoders() const = 0;

    virtual Result init(const QString& outputPath, const AudioFormat& inputFormat, const AudioEncoderSettings& settings)
        = 0;
    virtual Result write(const AudioBuffer& buffer) = 0;
    virtual Result finish()                         = 0;
};

using EncoderCreator = std::function<std::unique_ptr<AudioEncoder>()>;
} // namespace Fooyin
