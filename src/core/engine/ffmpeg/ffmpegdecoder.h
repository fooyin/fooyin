/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include "ffmpegworker.h"

class AVFormatContext;

namespace Fooyin {
class AudioFormat;
class Codec;
class AudioBuffer;

class Decoder : public EngineWorker
{
    Q_OBJECT

public:
    explicit Decoder(QObject* parent = nullptr);
    ~Decoder() override;

    void run(AVFormatContext* context, Codec* codec, const AudioFormat& format);
    void reset() override;
    void kill() override;

public slots:
    void onBufferProcessed();

signals:
    void audioBufferDecoded(const AudioBuffer& buffer);

protected:
    bool canDoNextStep() const override;
    int timerInterval() const override;
    void doNextStep() override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
