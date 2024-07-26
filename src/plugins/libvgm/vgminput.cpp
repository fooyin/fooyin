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

#include "vgminput.h"

#include "vgminputdefs.h"

#include <QRegularExpression>

#include <player/droplayer.hpp>
#include <player/gymplayer.hpp>
#include <player/playera.hpp>
#include <player/s98player.hpp>
#include <player/vgmplayer.hpp>
#include <utils/FileLoader.h>

#include <QDir>

// TODO: Make configurable
// Note: Some of these will change the duration of the track,
// so we'll need a way to update the saved duration so seekbars
// function normally
constexpr auto SampleRate = 44100;
constexpr auto Bps        = 16;
constexpr auto Channels   = 2;
constexpr auto FadeLen    = 4;
constexpr auto BufferLen  = 2048;

constexpr auto DurationFlags = (PLAYTIME_TIME_FILE | PLAYTIME_LOOP_INCL | PLAYTIME_WITH_FADE | PLAYTIME_WITH_SLNC);

namespace {
int extractTrackNumber(const QString& filename)
{
    static const QRegularExpression regex(QStringLiteral(R"(^(\d+))"));
    const QRegularExpressionMatch match = regex.match(filename);
    return match.hasMatch() ? match.captured(1).toInt() : -1;
}

QString findRomFile(const char* name)
{
    const Fooyin::FySettings settings;
    const auto path = settings.value(QLatin1String{Fooyin::VgmInput::RomPathSetting}).toString();

    const QDir dir{path};
    if(!dir.exists()) {
        qWarning() << "[VGMInput] ROM directory does not exist:" << path;
        return {};
    }

    const auto files = dir.entryInfoList({QString::fromLatin1(name)}, QDir::Files);
    if(files.isEmpty()) {
        qWarning() << "[VGMInput] Could not find ROM" << name << "in directory" << path;
        return {};
    }

    return files.front().absoluteFilePath();
}

DATA_LOADER* requestFileCallback(void* /*userParam*/, PlayerBase* /*player*/, const char* fileName)
{
    using namespace Fooyin::VgmInput;

    DataLoaderPtr loader;

    if(const QString romFile = findRomFile(fileName); !romFile.isEmpty()) {
        loader = {FileLoader_Init(romFile.toUtf8().constData()), DataLoaderDeleter()};
    }
    else {
        loader = {FileLoader_Init(fileName), DataLoaderDeleter()};
    }

    if(DataLoader_Load(loader.get()) == 0) {
        return loader.release();
    }

    return nullptr;
}
} // namespace

namespace Fooyin::VgmInput {
VgmInput::VgmInput()
{
    m_format.setSampleFormat(SampleFormat::S16);
    m_format.setSampleRate(SampleRate);
    m_format.setChannelCount(Channels);
}

void VgmInput::configurePlayer(PlayerA* player) const
{
    player->SetOutputSettings(SampleRate, Channels, Bps, BufferLen);

    PlayerA::Config config = player->GetConfiguration();
    config.masterVol       = 0x10000;
    config.fadeSmpls       = SampleRate * FadeLen;
    config.endSilenceSmpls = SampleRate / 2;
    config.pbSpeed         = 1.0;
    player->SetConfiguration(config);
}

QStringList VgmInput::supportedExtensions() const
{
    return {QStringLiteral("dro"), QStringLiteral("gym"), QStringLiteral("s98"), QStringLiteral("vgm"),
            QStringLiteral("vgz")};
}

bool VgmInput::canReadCover() const
{
    return false;
}

bool VgmInput::canWriteMetaData() const
{
    return false;
}

bool VgmInput::isSeekable() const
{
    return true;
}

bool VgmInput::init(const QString& source, DecoderOptions options)
{
    m_mainPlayer = std::make_unique<PlayerA>();
    m_mainPlayer->RegisterPlayerEngine(new VGMPlayer());
    m_mainPlayer->RegisterPlayerEngine(new S98Player());
    m_mainPlayer->RegisterPlayerEngine(new DROPlayer());
    m_mainPlayer->RegisterPlayerEngine(new GYMPlayer());
    m_mainPlayer->SetFileReqCallback(requestFileCallback, nullptr);
    configurePlayer(m_mainPlayer.get());

    int loopCount = m_settings.value(QLatin1String{LoopCountSetting}, DefaultLoopCount).toInt();
    if(options & NoLooping) {
        loopCount = 1;
    }
    if(options & NoInfiniteLooping && loopCount == 0) {
        loopCount = DefaultLoopCount;
    }

    m_mainPlayer->SetLoopCount(loopCount);

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
            m_mainPlayer->SetLoopCount(vgmPlayer->GetModifiedLoopCount(loopCount));
        }
    }

    return true;
}

void VgmInput::start()
{
    m_mainPlayer->Start();
}

void VgmInput::stop()
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

void VgmInput::seek(uint64_t pos)
{
    m_mainPlayer->Seek(PLAYPOS_SAMPLE, m_format.framesForDuration(pos));
}

AudioBuffer VgmInput::readBuffer(size_t bytes)
{
    const auto state = m_mainPlayer->GetState();

    if(state & PLAYSTATE_FIN) {
        return {};
    }

    const auto startTime = static_cast<uint64_t>(m_mainPlayer->GetCurTime(DurationFlags) * 1000);

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

bool VgmInput::readMetaData(Track& track)
{
    PlayerA mainPlayer;
    mainPlayer.RegisterPlayerEngine(new VGMPlayer());
    mainPlayer.RegisterPlayerEngine(new S98Player());
    mainPlayer.RegisterPlayerEngine(new DROPlayer());
    mainPlayer.RegisterPlayerEngine(new GYMPlayer());
    configurePlayer(&mainPlayer);

    int loopCount = m_settings.value(QLatin1String{LoopCountSetting}).toInt();
    if(loopCount == 0) {
        loopCount = DefaultLoopCount;
    }

    mainPlayer.SetLoopCount(loopCount);

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
    if(player->GetPlayerType() == FCC_VGM) {
        if(auto* vgmPlayer = dynamic_cast<VGMPlayer*>(player)) {
            mainPlayer.SetLoopCount(vgmPlayer->GetModifiedLoopCount(loopCount));
        }
    }

    track.setDuration(static_cast<uint64_t>(mainPlayer.GetTotalTime(DurationFlags) * 1000));
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

    if(m_settings.value(QLatin1String{GuessTrackSetting}).toBool()) {
        track.setTrackNumber(extractTrackNumber(track.filename()));
    }

    track.setCodec(QStringLiteral("VGM"));

    return true;
}

AudioFormat VgmInput::format() const
{
    return m_format;
}

AudioInput::Error VgmInput::error() const
{
    return {};
}
} // namespace Fooyin::VgmInput
