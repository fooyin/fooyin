/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "core/engine/audiooutput.h"

namespace Fy::Core::Engine {
class SdlOutput : public AudioOutput
{
public:
    SdlOutput(QObject* parent = nullptr);
    ~SdlOutput() override;

    void init(OutputContext* of) override;
    void start() override;
    int write(const char* data, int size) override;

    void setPaused(bool pause) override;

    int bufferSize() const override;
    void setBufferSize(int size) override;
    void clearBuffer() override;

private:
    struct Private;
    std::unique_ptr<SdlOutput::Private> p;
};
} // namespace Fy::Core::Engine
