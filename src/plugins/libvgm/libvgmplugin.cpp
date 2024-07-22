/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "libvgmplugin.h"

#include <player/droplayer.hpp>
#include <player/gymplayer.hpp>
#include <player/playera.hpp>
#include <player/s98player.hpp>
#include <player/vgmplayer.hpp>
#include <utils/FileLoader.h>
#include <utils/MemoryLoader.h>

// TODO: Make configurable
// Note: Some of these will change the duration of the track,
// so we'll need a way to update the saved duration so seekbars
// function normally
constexpr auto SampleRate = 44100;
constexpr auto Bps        = 16;
constexpr auto Channels   = 2;
constexpr auto FadeLen    = 4;
constexpr auto LoopCount  = 2;
constexpr auto BufferLen  = 2048;

namespace {
struct DataLoaderDeleter
{
    void operator()(DATA_LOADER* loader) const noexcept
    {
        if(loader) {
            DataLoader_Deinit(loader);
        }
    }
};
using DataLoaderPtr = std::unique_ptr<DATA_LOADER, DataLoaderDeleter>;

void configurePlayer(PlayerA* player)
{
    player->SetOutputSettings(SampleRate, Channels, Bps, BufferLen);

    PlayerA::Config config = player->GetConfiguration();
    config.masterVol       = 0x10000;
    config.loopCount       = LoopCount;
    config.fadeSmpls       = SampleRate * FadeLen;
    config.endSilenceSmpls = SampleRate / 2;
    config.pbSpeed         = 1.0;
    player->SetConfiguration(config);
}
} // namespace

namespace Fooyin::Gme {
class LibvgmInput : public AudioInput
{
public:
    [[nodiscard]] QStringList supportedExtensions() const override;
    [[nodiscard]] bool canReadCover() const override;
    [[nodiscard]] bool canWriteMetaData() const override;
    [[nodiscard]] bool isSeekable() const override;

    bool init(const QString& source) override;
    void start() override;
    void stop() override;

    void seek(uint64_t pos) override;

    AudioBuffer readBuffer(size_t bytes) override;

    [[nodiscard]] bool readMetaData(Track& track) override;

    [[nodiscard]] AudioFormat format() const override;
    [[nodiscard]] Error error() const override;

private:
    AudioFormat m_format;
    DataLoaderPtr m_loader;
    std::unique_ptr<PlayerA> m_mainPlayer;
};

QStringList LibvgmInput::supportedExtensions() const
{
    return {QStringLiteral("dro"), QStringLiteral("gym"), QStringLiteral("s98"), QStringLiteral("vgm"),
            QStringLiteral("vgz")};
}

bool LibvgmInput::canReadCover() const
{
    return false;
}

bool LibvgmInput::canWriteMetaData() const
{
    return false;
}

bool LibvgmInput::isSeekable() const
{
    return true;
}

bool LibvgmInput::init(const QString& source)
{
    m_mainPlayer = std::make_unique<PlayerA>();
    m_mainPlayer->RegisterPlayerEngine(new VGMPlayer());
    m_mainPlayer->RegisterPlayerEngine(new S98Player());
    m_mainPlayer->RegisterPlayerEngine(new DROPlayer());
    m_mainPlayer->RegisterPlayerEngine(new GYMPlayer());
    configurePlayer(m_mainPlayer.get());

    m_loader = {FileLoader_Init(source.toUtf8().constData()), DataLoaderDeleter()};
    if(!m_loader) {
        return false;
    }

    DataLoader_SetPreloadBytes(m_loader.get(), 0x100);
    if(DataLoader_Load(m_loader.get())) {
        return false;
    }

    if(m_mainPlayer->LoadFile(m_loader.get())) {
        return false;
    }

    PlayerBase* player = m_mainPlayer->GetPlayer();
    if(player->GetPlayerType() == FCC_VGM) {
        if(auto* vgmPlayer = dynamic_cast<VGMPlayer*>(player)) {
            m_mainPlayer->SetLoopCount(vgmPlayer->GetModifiedLoopCount(LoopCount));
        }
    }

    m_format.setSampleFormat(SampleFormat::S16);
    m_format.setSampleRate(SampleRate);
    m_format.setChannelCount(Channels);

    return true;
}

void LibvgmInput::start()
{
    m_mainPlayer->Start();
}

void LibvgmInput::stop()
{
    if(m_mainPlayer) {
        m_mainPlayer->Stop();
        m_mainPlayer->UnloadFile();
        m_mainPlayer.reset();
    }
    if(m_loader) {
        m_loader.reset();
    }
}

void LibvgmInput::seek(uint64_t pos)
{
    m_mainPlayer->Seek(PLAYPOS_SAMPLE, m_format.framesForDuration(pos));
}

AudioBuffer LibvgmInput::readBuffer(size_t bytes)
{
    if(m_mainPlayer->GetState() & PLAYSTATE_END) {
        return {};
    }

    const auto startTime = static_cast<uint64_t>(m_mainPlayer->GetCurTime(PLAYTIME_TIME_PBK) * 1000);

    AudioBuffer buffer{m_format, startTime};
    buffer.resize(bytes);

    const int frames = m_format.framesForBytes(static_cast<int>(bytes));
    int framesWritten{0};
    while(framesWritten < frames) {
        const int framesToWrite = std::min(frames - framesWritten, BufferLen);
        const int bufferPos     = m_format.bytesForFrames(framesWritten);
        m_mainPlayer->Render(m_format.bytesForFrames(framesToWrite), buffer.data() + bufferPos);
        framesWritten += framesToWrite;
    }

    return buffer;
}

bool LibvgmInput::readMetaData(Track& track)
{
    PlayerA mainPlayer;
    mainPlayer.RegisterPlayerEngine(new VGMPlayer());
    mainPlayer.RegisterPlayerEngine(new S98Player());
    mainPlayer.RegisterPlayerEngine(new DROPlayer());
    mainPlayer.RegisterPlayerEngine(new GYMPlayer());
    configurePlayer(&mainPlayer);

    const DataLoaderPtr loader{FileLoader_Init(track.filepath().toUtf8().constData()), DataLoaderDeleter()};
    if(!loader) {
        return false;
    }

    DataLoader_SetPreloadBytes(loader.get(), 0x100);
    if(DataLoader_Load(loader.get())) {
        return false;
    }

    if(mainPlayer.LoadFile(loader.get())) {
        return false;
    }

    PlayerBase* player = mainPlayer.GetPlayer();

    const auto durFlags = (PLAYTIME_TIME_FILE | PLAYTIME_LOOP_INCL | PLAYTIME_WITH_FADE);
    track.setDuration(static_cast<uint64_t>(mainPlayer.GetTotalTime(durFlags) * 1000));
    track.setSampleRate(static_cast<int>(player->GetSampleRate()));
    track.setBitDepth(Bps);
    track.setChannels(Channels);

    const auto* tagList = player->GetTags();
    for(const auto* tag = tagList; *tag; tag += 2) {
        if(!strcmp(tag[0], "TITLE")) {
            track.setTitle(QString::fromLocal8Bit(tag[1]));
        }
        else if(!strcmp(tag[0], "ARTIST")) {
            track.setArtists({QString::fromLocal8Bit(tag[1])});
        }
        else if(!strcmp(tag[0], "GAME")) {
            track.setAlbum(QString::fromLocal8Bit(tag[1]));
        }
        else if(!strcmp(tag[0], "DATE")) {
            track.setDate(QString::fromLocal8Bit(tag[1]));
        }
        else if(!strcmp(tag[0], "GENRE")) {
            track.setGenres({QString::fromLocal8Bit(tag[1])});
        }
        else if(!strcmp(tag[0], "COMMENT")) {
            track.setComment({QString::fromLocal8Bit(tag[1])});
        }
        else {
            track.addExtraTag(QString::fromLocal8Bit(tag[0]).toUpper(), QString::fromLocal8Bit(tag[1]));
        }
    }

    track.setCodec(QStringLiteral("VGM"));

    return true;
}

AudioFormat LibvgmInput::format() const
{
    return m_format;
}

AudioInput::Error LibvgmInput::error() const
{
    return {};
}

QString LibvgmPlugin::name() const
{
    return QStringLiteral("Libvgm");
}

InputCreator LibvgmPlugin::inputCreator() const
{
    return []() {
        return std::make_unique<LibvgmInput>();
    };
}
} // namespace Fooyin::Gme

#include "moc_libvgmplugin.cpp"
