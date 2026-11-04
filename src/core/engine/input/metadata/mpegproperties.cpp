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

#include "mpegproperties.h"

#include <core/track.h>

#include <QtEndian>

#include <array>
#include <map>

using namespace Qt::StringLiterals;

namespace Fooyin::Metadata {
namespace {
enum VbrMethod : uint8_t
{
    Unknown  = 0,
    CBR      = 1,
    ABR      = 2,
    VBR1     = 3,
    VBR2     = 4,
    VBR3     = 5,
    VBR4     = 6,
    CBR2Pass = 8,
    ABR2Pass = 9,
};

enum XingFlag : uint8_t
{
    Frames   = 1,
    Bytes    = 2,
    TOC      = 4,
    VBRScale = 8,
};
} // namespace

void readMpegProperties(QByteArrayView data, Track& track)
{
    // Reference: http://gabriel.mp3-tech.org/mp3infotag.html

    if(data.size() < 4) {
        return;
    }

    const auto header       = qFromBigEndian<quint32>(data.data());
    const auto version      = (header >> 19) & 3;
    const auto bitrateIndex = (header >> 12) & 15;
    const auto rateIndex    = (header >> 10) & 3;
    if((header & 0xffe00000U) != 0xffe00000U || ((header >> 17) & 3) != 1 || version == 1 || bitrateIndex == 0
       || bitrateIndex == 15 || rateIndex == 3) {
        return;
    }

    static constexpr std::array rates{44100, 48000, 32000};
    static constexpr std::array bitrates{0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
    static constexpr std::array lowBitrates{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160};

    const int rate         = rates[rateIndex] / (version == 3 ? 1 : version == 2 ? 2 : 4);
    const int bitrate      = version == 3 ? bitrates[bitrateIndex] : lowBitrates[bitrateIndex];
    const auto frameLength = ((version == 3 ? 144000 : 72000) * bitrate / rate) + ((header >> 9) & 1);

    data = data.first(std::min(data.size(), static_cast<qsizetype>(frameLength)));

    static const std::array modes{u"stereo"_s, u"joint stereo"_s, u"dual channel"_s, u"mono"_s};
    const auto mode = (header >> 6) & 3;
    track.setExtraProperty(u"MP3_STEREO_MODE"_s, modes[mode]);

    // Xing follows the MPEG header, VBRI has a fixed offset
    const qsizetype offset = 4 + (version == 3 ? (mode == 3 ? 17 : 32) : (mode == 3 ? 9 : 17));
    if(data.size() < offset + 8) {
        return;
    }

    const auto marker = data.sliced(offset, 4);
    const bool isCbr  = marker == "Info";
    if(!isCbr && marker != "Xing") {
        if(data.size() >= 40 && data.sliced(36, 4) == "VBRI") {
            track.setCodecProfile(u"VBR"_s);
        }
        return;
    }

    track.setCodecProfile(isCbr ? u"CBR"_s : u"VBR"_s);
    const auto flags = qFromBigEndian<quint32>(data.data() + offset + 4);
    if((flags & ~15) != 0) {
        return;
    }

    qsizetype encoderOffset = offset + 8;
    if(flags & Frames) {
        if(data.size() < encoderOffset + 4) {
            return;
        }
        if(qFromBigEndian<quint32>(data.data() + encoderOffset) > 0) {
            track.setExtraProperty(u"MP3_ACCURATE_LENGTH"_s, u"yes"_s);
        }
        encoderOffset += 4;
    }
    if(flags & Bytes) {
        encoderOffset += 4;
    }
    if(flags & TOC) {
        encoderOffset += 100;
    }
    if(flags & VBRScale) {
        encoderOffset += 4;
    }

    if(data.size() < encoderOffset + 9) {
        return;
    }

    const auto encoderData = data.sliced(encoderOffset, 9);
    if(!std::ranges::all_of(encoderData, [](unsigned char ch) { return ch == 0 || (ch >= 32 && ch <= 126); })) {
        return;
    }

    auto encoder = QString::fromLatin1(encoderData.data(), encoderData.size()).simplified().remove('\0'_L1);
    if(encoder.endsWith(u'.')) {
        encoder.chop(1);
    }
    if(!encoder.isEmpty()) {
        track.setTool(encoder);
    }

    const bool hasLameTag = encoderData.startsWith("LAME") || encoderData.startsWith("L3.99")
                         || encoderData.startsWith("Lavc") || encoderData.startsWith("Lavf");
    if(!hasLameTag || data.size() < encoderOffset + 24) {
        return;
    }

    const auto delayPadding = qFromBigEndian<quint32>(data.data() + encoderOffset + 20) & 0xFFFFFF;
    track.setExtraProperty(u"ENC_DELAY"_s, QString::number(delayPadding >> 12));
    track.setExtraProperty(u"ENC_PADDING"_s, QString::number(delayPadding & 0xFFF));

    if(isCbr || data.size() < encoderOffset + 28) {
        return;
    }

    const auto vbrMethod = static_cast<VbrMethod>(data[encoderOffset + 9] & 0x0F);
    const auto preset    = qFromBigEndian<quint16>(data.data() + encoderOffset + 26) & 0x07FF;

    QString codecProfile;
    switch(vbrMethod) {
        case CBR:
        case CBR2Pass:
            codecProfile = u"CBR"_s;
            break;
        case ABR:
        case ABR2Pass:
            codecProfile = u"ABR"_s;
            break;
        case VBR1:
        case VBR2:
        case VBR3:
        case VBR4:
        case Unknown:
        default:
            // Assume VBR if we've got this far
            codecProfile = u"VBR"_s;
            break;
    }

    static const std::map<int, QString> presets{
        {8, u"8"_s},
        {320, u"320"_s},
        {410, u"V9"_s},
        {420, u"V8"_s},
        {430, u"V7"_s},
        {440, u"V6"_s},
        {450, u"V5"_s},
        {460, u"V4"_s},
        {470, u"V3"_s},
        {480, u"V2"_s},
        {490, u"V1"_s},
        {500, u"V0"_s},
        {1000, u"R3mix"_s},
        {1001, u"Standard"_s},
        {1002, u"Extreme"_s},
        {1003, u"Insane"_s},
        {1004, u"Standard Fast"_s},
        {1005, u"Extreme Fast"_s},
        {1006, u"Medium"_s},
        {1007, u"Medium Fast"_s},
    };

    if(!codecProfile.isEmpty() && presets.contains(preset)) {
        codecProfile += " "_L1 + presets.at(preset);
    }
    track.setCodecProfile(codecProfile);
}
} // namespace Fooyin::Metadata
