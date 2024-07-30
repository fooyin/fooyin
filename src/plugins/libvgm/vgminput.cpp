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

#include <QDir>
#include <QRegularExpression>

#include <player/droplayer.hpp>
#include <player/gymplayer.hpp>
#include <player/playera.hpp>
#include <player/s98player.hpp>
#include <player/vgmplayer.hpp>
#include <utils/FileLoader.h>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(VGM_INPUT, "VGMInput")

constexpr auto SampleRate = 44100;
constexpr auto Bps        = 16;
constexpr auto Channels   = 2;
constexpr auto FadeLen    = 4;
constexpr auto BufferLen  = 2048;

constexpr auto DurationFlags = (PLAYTIME_TIME_FILE | PLAYTIME_LOOP_INCL | PLAYTIME_WITH_FADE | PLAYTIME_WITH_SLNC);

namespace {
QStringList fileExtensions()
{
    static const QStringList extensions = {QStringLiteral("dro"), QStringLiteral("gym"), QStringLiteral("s98"),
                                           QStringLiteral("vgm"), QStringLiteral("vgz")};
    return extensions;
}

void configurePlayer(PlayerA* player)
{
    player->SetOutputSettings(SampleRate, Channels, Bps, BufferLen);

    PlayerA::Config config = player->GetConfiguration();
    config.masterVol       = 0x10000;
    config.fadeSmpls       = SampleRate * FadeLen;
    config.endSilenceSmpls = SampleRate / 2;
    config.pbSpeed         = 1.0;
    player->SetConfiguration(config);
}

void setLoopCount(PlayerA* player, int count)
{
    player->SetLoopCount(count);

    PlayerBase* playerBase = player->GetPlayer();
    if(playerBase->GetPlayerType() == FCC_VGM) {
        if(auto* vgmPlayer = dynamic_cast<VGMPlayer*>(playerBase)) {
            player->SetLoopCount(vgmPlayer->GetModifiedLoopCount(count));
        }
    }
}

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

    if(path.isEmpty()) {
        qCWarning(VGM_INPUT) << "ROM" << name << "required for playback but ROM directory has not been configured";
        return {};
    }

    const QDir dir{path};
    if(!dir.exists()) {
        qCWarning(VGM_INPUT) << "ROM directory does not exist:" << path;
        return {};
    }

    const auto files = dir.entryInfoList({QString::fromLatin1(name)}, QDir::Files);
    if(files.isEmpty()) {
        qCWarning(VGM_INPUT) << "Could not find ROM" << name << "in directory" << path;
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
VgmDecoder::VgmDecoder()
{
    m_format.setSampleFormat(SampleFormat::S16);
    m_format.setSampleRate(SampleRate);
    m_format.setChannelCount(Channels);
}

QStringList VgmDecoder::extensions() const
{
    return fileExtensions();
}

bool VgmDecoder::isSeekable() const
{
    return true;
}

bool VgmDecoder::trackHasChanged() const
{
    return m_changedTrack.isValid();
}

Track VgmDecoder::changedTrack() const
{
    return m_changedTrack;
}

std::optional<AudioFormat> VgmDecoder::init(const Track& track, DecoderOptions options)
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

    m_loader = {FileLoader_Init(track.filepath().toUtf8().constData()), DataLoaderDeleter()};
    if(!m_loader) {
        return {};
    }

    DataLoader_SetPreloadBytes(m_loader.get(), 0x100);
    if(DataLoader_Load(m_loader.get())) {
        return {};
    }

    if(m_mainPlayer->LoadFile(m_loader.get())) {
        return {};
    }

    const int currLoopCount{loopCount};

    if(options & UpdateTracks) {
        setLoopCount(m_mainPlayer.get(), currLoopCount == 0 ? DefaultLoopCount : currLoopCount);

        const auto duration = static_cast<uint64_t>(m_mainPlayer->GetTotalTime(DurationFlags) * 1000);
        if(track.duration() != duration) {
            m_changedTrack = track;
            m_changedTrack.setDuration(duration);
        }
    }

    setLoopCount(m_mainPlayer.get(), currLoopCount);

    return m_format;
}

void VgmDecoder::start()
{
    m_mainPlayer->Start();
}

void VgmDecoder::stop()
{
    if(m_mainPlayer) {
        m_mainPlayer->Stop();
        m_mainPlayer->UnloadFile();
        m_mainPlayer.reset();
    }
    if(m_loader) {
        m_loader.reset();
    }
    m_changedTrack = {};
}

void VgmDecoder::seek(uint64_t pos)
{
    m_mainPlayer->Seek(PLAYPOS_SAMPLE, m_format.framesForDuration(pos));
}

AudioBuffer VgmDecoder::readBuffer(size_t bytes)
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

QStringList VgmReader::extensions() const
{
    return fileExtensions();
}

bool VgmReader::canReadCover() const
{
    return false;
}

bool VgmReader::canWriteMetaData() const
{
    return false;
}

bool VgmReader::readTrack(Track& track)
{
    PlayerA mainPlayer;
    mainPlayer.RegisterPlayerEngine(new VGMPlayer());
    mainPlayer.RegisterPlayerEngine(new S98Player());
    mainPlayer.RegisterPlayerEngine(new DROPlayer());
    mainPlayer.RegisterPlayerEngine(new GYMPlayer());
    configurePlayer(&mainPlayer);

    const DataLoaderPtr loader{FileLoader_Init(track.filepath().toUtf8().constData())};
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

    const FySettings settings;

    int loopCount = settings.value(QLatin1String{LoopCountSetting}).toInt();
    if(loopCount == 0) {
        loopCount = DefaultLoopCount;
    }

    setLoopCount(&mainPlayer, loopCount);

    PlayerBase* player = mainPlayer.GetPlayer();
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

    if(settings.value(QLatin1String{GuessTrackSetting}).toBool()) {
        track.setTrackNumber(extractTrackNumber(track.filename()));
    }

    return true;
}
} // namespace Fooyin::VgmInput
