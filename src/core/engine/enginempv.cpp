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

#include "enginempv.h"

#include "core/models/track.h"

#include <mpv/client.h>

#include <QDebug>

namespace Fy::Core::Engine {
static void wakeup(void* ctx)
{
    auto* engine = static_cast<EngineMpv*>(ctx);
    emit engine->mpvEvent();
}

EngineMpv::EngineMpv(QObject* parent)
    : Engine{parent}
    , m_posInterval{100}
    , m_ms{1000LL}
    , m_lastTick{0}
    , m_mpv{nullptr}
{
    // Not thread safe
    setlocale(LC_NUMERIC, "C"); // NOLINT

    m_mpv = mpv_create();

    if(!m_mpv) {
        return;
    }

    engineSetup();
}

EngineMpv::~EngineMpv()
{
    mpv_terminate_destroy(m_mpv);
    m_mpv = nullptr;
}

void EngineMpv::engineSetup()
{
    mpv_set_option_string(m_mpv, "config", "no");
    mpv_set_option_string(m_mpv, "audio-display", "no");
    mpv_set_option_string(m_mpv, "vo", "null");
    mpv_set_option_string(m_mpv, "idle", "yes");
    mpv_set_option_string(m_mpv, "input-default-bindings", "no");
    mpv_set_option_string(m_mpv, "input-vo-keyboard", "no");
    mpv_set_option_string(m_mpv, "input-cursor", "no");
    mpv_set_option_string(m_mpv, "input-media-keys", "no");
    mpv_set_option_string(m_mpv, "ytdl", "no");
    mpv_set_option_string(m_mpv, "fs", "no");
    mpv_set_option_string(m_mpv, "osd-level", "0");
    mpv_set_option_string(m_mpv, "quiet", "yes");
    mpv_set_option_string(m_mpv, "softvol", "yes");
    mpv_set_option_string(m_mpv, "softvol-max", "100.0");
    mpv_set_option_string(m_mpv, "audio-client-name", "fooyin");

    connect(this, &EngineMpv::mpvEvent, this, &EngineMpv::processEvents, Qt::QueuedConnection);

    mpv_set_wakeup_callback(m_mpv, wakeup, this);

    if(mpv_initialize(m_mpv) < 0) {
        return;
    }
}

void EngineMpv::processEvents()
{
    // Process all events, until the event queue is empty.
    while(m_mpv) {
        mpv_event* event = mpv_wait_event(m_mpv, 0);
        if(event->event_id == MPV_EVENT_NONE) {
            break;
        }
        handleEvent(event);
    }
}

void EngineMpv::play()
{
    mpv_observe_property(m_mpv, 1, "audio-pts", MPV_FORMAT_DOUBLE);
    int f = 0;
    mpv_set_property_async(m_mpv, 0, "pause", MPV_FORMAT_FLAG, &f);
}

void EngineMpv::stop()
{
    mpv_unobserve_property(m_mpv, 1);

    const char* cmd[] = {"stop", nullptr}; // NOLINT
    mpv_command(m_mpv, cmd);
}

void EngineMpv::pause()
{
    int f = 1;
    mpv_set_property_async(m_mpv, 0, "pause", MPV_FORMAT_FLAG, &f);
}

void EngineMpv::seek(uint64_t pos)
{
    const uint64_t seconds = pos / 1000;
    const QByteArray tmp   = QString::number(seconds).toUtf8();
    const char* cmd[]      = {"seek", tmp.constData(), "absolute", nullptr}; // NOLINT
    mpv_command(m_mpv, cmd);
}

void EngineMpv::changeTrack(const Track& track)
{
    const QByteArray path_ba = track.filepath().toUtf8();
    const char* cmd[]        = {"loadfile", path_ba.constData(), "replace", nullptr}; // NOLINT
    mpv_command(m_mpv, cmd);
}

void EngineMpv::setVolume(float value)
{
    double volume = value;
    mpv_set_property_async(m_mpv, 0, "volume", MPV_FORMAT_DOUBLE, &volume);
}

void EngineMpv::handleEvent(mpv_event* event)
{
    if(!event || event->event_id == MPV_EVENT_NONE) {
        return;
    }

    if(event->error < 0) {
        qDebug() << mpv_error_string(event->error);
        return;
    }

    switch(event->event_id) {
        case(MPV_EVENT_PROPERTY_CHANGE): {
            return handlePropertyChange(event);
        }
        case(MPV_EVENT_LOG_MESSAGE): {
            auto* msg = static_cast<mpv_event_log_message*>(event->data);
            qDebug() << "[" << msg->prefix << "] " << msg->level << ": " << msg->text;
            break;
        }
        case(MPV_EVENT_START_FILE): {
            break;
        }
        case(MPV_EVENT_END_FILE): {
            auto* eof_event = static_cast<mpv_event_end_file*>(event->data);
            if(eof_event->reason == MPV_END_FILE_REASON_EOF) {
                emit trackFinished();
            }
            break;
        }
        default:
            return;
    }
}

void EngineMpv::handlePropertyChange(mpv_event* event)
{
    auto* prop = static_cast<mpv_event_property*>(event->data);

    if(QString(prop->name) == "audio-pts") {
        if(prop->format == MPV_FORMAT_DOUBLE) {
            auto time = static_cast<uint64_t>((*static_cast<double*>(prop->data) * m_ms));

            if(time != m_lastTick && time > 0) {
                if(time + m_posInterval >= m_lastTick || time - m_posInterval <= m_lastTick) {
                    m_lastTick = time;
                    emit currentPositionChanged(time);
                }
            }
        }
    }
}
} // namespace Fy::Core::Engine
