/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "openmptinput.h"

#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(OPENMPT, "OpenMPT")

// TODO: Make configurable
constexpr auto SampleRate          = 44100;
constexpr auto Channels            = 2;
constexpr auto BufferLen           = 1024UL;
constexpr auto RepeatCount         = 5;
constexpr auto VolumeRamping       = -1;
constexpr auto StereoSeperation    = 100;
constexpr auto InterpolationFilter = 0;
constexpr auto EmulateAmiga        = true;

namespace {
QStringList fileExtensions()
{
    QStringList extensions;

    const auto openExts = openmpt::get_supported_extensions();
    for(const auto& ext : openExts) {
        extensions.emplace_back(QString::fromLocal8Bit(ext));
    }

    return extensions;
}

void setupModule(openmpt::module* module)
{
    module->set_render_param(openmpt::module::RENDER_STEREOSEPARATION_PERCENT, StereoSeperation);
    module->set_render_param(openmpt::module::RENDER_INTERPOLATIONFILTER_LENGTH, InterpolationFilter);
    module->set_render_param(openmpt::module::RENDER_VOLUMERAMPING_STRENGTH, VolumeRamping);
    module->ctl_set_boolean("render.resampler.emulate_amiga", EmulateAmiga);
}
} // namespace

namespace Fooyin::OpenMpt {
OpenMptInput::OpenMptInput()
    : m_eof{false}
{
    // Assume 16bit PCM
    m_format.setSampleFormat(SampleFormat::F32);
    m_format.setChannelCount(Channels);
    m_format.setSampleRate(SampleRate);
}

QStringList OpenMptInput::extensions() const
{
    return fileExtensions();
}

bool OpenMptInput::isSeekable() const
{
    return true;
}

std::optional<AudioFormat> OpenMptInput::init(const Track& track, DecoderOptions options)
{
    QFile file{track.filepath()};
    if(!file.open(QIODevice::ReadOnly)) {
        qCWarning(OPENMPT) << "Unable to open" << track.filepath();
        return {};
    }

    std::vector<char> data(static_cast<std::size_t>(file.size()));
    file.read(data.data(), file.size());

    try {
        std::map<std::string, std::string> ctls;
        ctls["seek.sync_samples"] = "1";

        m_module = std::make_unique<openmpt::module>(data, std::clog, ctls);

        int repeat = RepeatCount;
        if(options & NoLooping || options & NoInfiniteLooping) {
            repeat = 0;
        }
        m_module->set_repeat_count(repeat);
        setupModule(m_module.get());

        m_module->select_subsong(track.subsong());
    }
    catch(...) {
        qCWarning(OPENMPT) << "Failed to open" << track.filepath();
        return {};
    }

    return m_format;
}

void OpenMptInput::stop()
{
    if(m_module) {
        m_module.reset();
    }
    m_eof = false;
}

void OpenMptInput::seek(uint64_t pos)
{
    m_module->set_position_seconds(static_cast<double>(pos) / 1000);
}

AudioBuffer OpenMptInput::readBuffer(size_t bytes)
{
    if(m_eof) {
        return {};
    }

    AudioBuffer buffer{m_format, static_cast<uint64_t>(m_module->get_position_seconds() * 1000)};
    buffer.resize(bytes);

    const auto frames = static_cast<size_t>(m_format.framesForBytes(static_cast<int>(bytes)));

    size_t framesWritten{0};
    while(framesWritten < frames) {
        const size_t framesToWrite = std::min(frames - framesWritten, BufferLen);
        const size_t readCount     = m_module->read_interleaved_stereo(
            SampleRate, framesToWrite, std::bit_cast<float*>(buffer.data()) + framesWritten * 2);
        if(readCount == 0) {
            return {};
        }

        framesWritten += readCount;

        if(readCount < framesToWrite) {
            m_eof = true;
            break;
        }
    }

    return buffer;
}

OpenMptReader::OpenMptReader()
    : m_subsongCount{1}
{ }

QStringList OpenMptReader::extensions() const
{
    return fileExtensions();
}

bool OpenMptReader::canReadCover() const
{
    return false;
}

bool OpenMptReader::canWriteMetaData() const
{
    return false;
}

int OpenMptReader::subsongCount() const
{
    return m_subsongCount;
}

bool OpenMptReader::init(const QString& source)
{
    QFile file{source};
    if(!file.open(QIODevice::ReadOnly)) {
        qCWarning(OPENMPT) << "Unable to open" << source;
        return false;
    }

    std::vector<char> data(static_cast<std::size_t>(file.size()));
    file.read(data.data(), file.size());

    try {
        const std::map<std::string, std::string> ctls;
        m_module = std::make_unique<openmpt::module>(data, std::clog, ctls);
        setupModule(m_module.get());

        m_subsongCount = m_module->get_num_subsongs();
    }
    catch(...) {
        qCWarning(OPENMPT) << "Failed to open" << source;
        return {};
    }

    return true;
}

bool OpenMptReader::readTrack(Track& track)
{
    QFile file{track.filepath()};
    if(!file.open(QIODevice::ReadOnly)) {
        qCWarning(OPENMPT) << "Unable to open" << track.filepath();
        return false;
    }

    std::vector<char> data(static_cast<std::size_t>(file.size()));
    file.read(data.data(), file.size());

    try {
        const std::map<std::string, std::string> ctls;

        const int subsong = track.subsong();
        m_module->select_subsong(subsong);

        track.setDuration(static_cast<uint64_t>(m_module->get_duration_seconds() * 1000));

        const auto names = m_module->get_subsong_names();
        if(std::cmp_less(subsong, names.size())) {
            track.setTitle(QString::fromLocal8Bit(names.at(subsong)));
        }

        const std::vector<std::string> keys = m_module->get_metadata_keys();

        for(const auto& key : keys) {
            const QString tag   = QString::fromLocal8Bit(key);
            const QString value = QString::fromLocal8Bit(m_module->get_metadata(key));

            if(tag == u"type") {
                continue;
            }

            if(tag == u"title" && track.title().isEmpty()) {
                track.setTitle(value);
            }
            else if(tag == u"album") {
                track.setAlbum(value);
            }
            else if(tag == u"artist") {
                track.setArtists({value});
            }
            else if(tag == u"year") {
                track.setDate(value);
            }
            else if(tag == u"genre") {
                track.setGenres({value});
            }
            else if(tag == u"track number") {
                track.setTrackNumber(value.toInt());
            }
            else if(tag == u"comments") {
                track.setComment(value);
            }
            else if(tag == u"long_type") {
                track.setCodec(value);
            }
            else {
                track.addExtraTag(tag, value);
            }
        }
    }
    catch(...) {
        qCWarning(OPENMPT) << "Failed to open" << track.filepath();
        return {};
    }

    track.setSampleRate(SampleRate);
    track.setChannels(Channels);
    track.setBitDepth(32);

    return true;
}
} // namespace Fooyin::OpenMpt
