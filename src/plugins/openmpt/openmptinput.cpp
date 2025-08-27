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

#include "openmptdefs.h"

#include <utils/settings/settingsmanager.h>

#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(OPENMPT, "fy.openmpt")

using namespace Qt::StringLiterals;

// TODO: Make configurable
constexpr auto SampleRate  = 44100;
constexpr auto Channels    = 2;
constexpr auto BufferLen   = 1024UL;
constexpr auto RepeatCount = 0;

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

void setupModule(Fooyin::SettingsManager* settings, openmpt::module* module)
{
    using namespace Fooyin::Settings::OpenMpt;

    module->set_render_param(openmpt::module::RENDER_MASTERGAIN_MILLIBEL,
                             static_cast<int32_t>(settings->value<Gain>() * 100));
    module->set_render_param(openmpt::module::RENDER_STEREOSEPARATION_PERCENT, settings->value<Separation>());
    module->set_render_param(openmpt::module::RENDER_INTERPOLATIONFILTER_LENGTH,
                             settings->value<InterpolationFilter>());
    module->set_render_param(openmpt::module::RENDER_VOLUMERAMPING_STRENGTH, settings->value<VolumeRamping>());
    module->ctl_set_boolean("render.resampler.emulate_amiga", settings->value<EmulateAmiga>());
}
} // namespace

namespace Fooyin::OpenMpt {
OpenMptDecoder::OpenMptDecoder(SettingsManager* settings)
    : m_settings{settings}
    , m_eof{false}
{
    m_format.setSampleFormat(SampleFormat::F32);
    m_format.setChannelCount(Channels);
    m_format.setSampleRate(SampleRate);
}

QStringList OpenMptDecoder::extensions() const
{
    return fileExtensions();
}

bool OpenMptDecoder::isSeekable() const
{
    return true;
}

std::optional<AudioFormat> OpenMptDecoder::init(const AudioSource& source, const Track& track, DecoderOptions options)
{
    std::vector<char> data(static_cast<std::size_t>(source.device->size()));
    source.device->read(data.data(), source.device->size());

    try {
        std::map<std::string, std::string> ctls;
        ctls["seek.sync_samples"] = "1";

        m_module = std::make_unique<openmpt::module>(data, std::clog, ctls);

        int repeat = RepeatCount;
        if(options & NoLooping || options & NoInfiniteLooping) {
            repeat = 0;
        }
        m_module->set_repeat_count(repeat);
        setupModule(m_settings, m_module.get());
        m_module->select_subsong(track.subsong());
    }
    catch(...) {
        qCWarning(OPENMPT) << "Failed to open" << track.filepath();
        return {};
    }

    return m_format;
}

void OpenMptDecoder::stop()
{
    if(m_module) {
        m_module.reset();
    }
    m_eof = false;
}

void OpenMptDecoder::seek(uint64_t pos)
{
    m_module->set_position_seconds(static_cast<double>(pos) / 1000);
}

AudioBuffer OpenMptDecoder::readBuffer(size_t bytes)
{
    if(m_eof) {
        return {};
    }

    AudioBuffer buffer{m_format, static_cast<uint64_t>(m_module->get_position_seconds() * 1000)};
    buffer.resize(bytes);

    const auto frames = static_cast<size_t>(m_format.framesForBytes(static_cast<int>(bytes)));

    size_t framesWritten{0};
    while(framesWritten < frames) {
        const size_t framesToWrite = std::min<size_t>(frames - framesWritten, BufferLen);
        const size_t readCount     = m_module->read_interleaved_stereo(
            SampleRate, framesToWrite, reinterpret_cast<float*>(buffer.data()) + (framesWritten * 2));
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

OpenMptReader::OpenMptReader(SettingsManager* settings)
    : m_settings{settings}
    , m_subsongCount{1}
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

bool OpenMptReader::init(const AudioSource& source)
{
    std::vector<char> data(static_cast<size_t>(source.device->size()));
    source.device->peek(data.data(), source.device->size());

    try {
        const std::map<std::string, std::string> ctls;
        m_module = std::make_unique<openmpt::module>(data, std::clog, ctls);
        setupModule(m_settings, m_module.get());

        m_subsongCount = m_module->get_num_subsongs();
    }
    catch(...) {
        qCWarning(OPENMPT) << "Failed to open" << source.filepath;
        return {};
    }

    return true;
}

bool OpenMptReader::readTrack(const AudioSource& /*source*/, Track& track)
{
    try {
        const std::map<std::string, std::string> ctls;

        const int subsong = track.subsong();
        m_module->select_subsong(subsong);

        track.setDuration(static_cast<uint64_t>(m_module->get_duration_seconds() * 1000));

        const auto subsongNames = m_module->get_subsong_names();
        if(std::cmp_less(subsong, subsongNames.size())) {
            track.setTitle(QString::fromUtf8(subsongNames.at(subsong)));
        }

        const std::vector<std::string> keys = m_module->get_metadata_keys();

        for(const auto& key : keys) {
            const QString tag   = QString::fromUtf8(key);
            const QString value = QString::fromUtf8(m_module->get_metadata(key));

            if(tag == "title"_L1 && track.title().isEmpty()) {
                track.setTitle(value);
            }
            else if(tag == "album"_L1) {
                track.setAlbum(value);
            }
            else if(tag == "artist"_L1) {
                track.setArtists({value});
            }
            else if(tag == "date"_L1) {
                track.setDate(value);
            }
            else if(tag == "year"_L1) {
                track.setYear(value.toInt());
            }
            else if(tag == "genre"_L1) {
                track.setGenres({value});
            }
            else if(tag == "track number"_L1) {
                track.setTrackNumber(value);
            }
            else if(tag == "comments"_L1) {
                track.setComment(value);
            }
            else {
                track.addExtraTag(tag, value);
            }
        }

        auto getModuleNames = [&track](const QString& field, const std::vector<std::string>& names) {
            for(auto it = names.cbegin(); it != names.cend(); ++it) {
                if(!it->empty()) {
                    const int index = static_cast<int>(std::distance(names.cbegin(), it));
                    track.addExtraTag(field + u"%1"_s.arg(index, 2, 10, QLatin1Char('0')), QString::fromUtf8(*it));
                }
            }
        };

        getModuleNames(u"CHAN"_s, m_module->get_channel_names());
        getModuleNames(u"PATT"_s, m_module->get_pattern_names());
        getModuleNames(u"INST"_s, m_module->get_instrument_names());
        getModuleNames(u"SMPL"_s, m_module->get_sample_names());

        track.setExtraProperty(u"MOD_CHANNELS"_s, QString::number(m_module->get_num_channels()));
        track.setExtraProperty(u"MOD_INSTRUMENTS"_s, QString::number(m_module->get_num_instruments()));
        track.setExtraProperty(u"MOD_ORDERS"_s, QString::number(m_module->get_num_orders()));
        track.setExtraProperty(u"MOD_PATTERNS"_s, QString::number(m_module->get_num_patterns()));
        track.setExtraProperty(u"MOD_SAMPLES"_s, QString::number(m_module->get_num_samples()));
    }
    catch(...) {
        qCWarning(OPENMPT) << "Failed to open" << track.filepath();
        return {};
    }

    track.setSampleRate(SampleRate);
    track.setChannels(Channels);
    track.setBitDepth(32);
    track.setEncoding(u"Synthesized"_s);

    return true;
}
} // namespace Fooyin::OpenMpt
