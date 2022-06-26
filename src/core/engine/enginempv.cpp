/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "models/track.h"
#include "utils/utils.h"

#include <mpv/client.h>

struct EngineMpv::Private
{
    const int posInterval{100};
    const int ms{1000LL};
    quint64 lastTick{0};
    mpv_handle* mpv;
};

static void wakeup(void* ctx)
{
    auto* engine = static_cast<EngineMpv*>(ctx);
    emit engine->mpvEvent();
}

EngineMpv::EngineMpv(QObject* parent)
    : Engine(parent)
    , p(std::make_unique<Private>())
{
    // Not thread safe
    setlocale(LC_NUMERIC, "C"); // NOLINT

    p->mpv = mpv_create();

    if(!p->mpv) {
        return;
    }

    engineSetup();
}

EngineMpv::~EngineMpv()
{
    mpv_terminate_destroy(p->mpv);
    p->mpv = nullptr;
}

void EngineMpv::engineSetup()
{
    mpv_set_option_string(p->mpv, "config", "no");
    mpv_set_option_string(p->mpv, "audio-display", "no");
    mpv_set_option_string(p->mpv, "vo", "null");
    mpv_set_option_string(p->mpv, "idle", "yes");
    mpv_set_option_string(p->mpv, "input-default-bindings", "no");
    mpv_set_option_string(p->mpv, "input-vo-keyboard", "no");
    mpv_set_option_string(p->mpv, "input-cursor", "no");
    mpv_set_option_string(p->mpv, "input-media-keys", "no");
    mpv_set_option_string(p->mpv, "ytdl", "no");
    mpv_set_option_string(p->mpv, "fs", "no");
    mpv_set_option_string(p->mpv, "osd-level", "0");
    mpv_set_option_string(p->mpv, "quiet", "yes");
    mpv_set_option_string(p->mpv, "softvol", "yes");
    mpv_set_option_string(p->mpv, "softvol-max", "100.0");
    mpv_set_option_string(p->mpv, "audio-client-name", "fooyin");

    connect(this, &EngineMpv::mpvEvent, this, &EngineMpv::processEvents, Qt::QueuedConnection);

    mpv_set_wakeup_callback(p->mpv, wakeup, this);

    if(mpv_initialize(p->mpv) < 0) {
        return;
    }
}

void EngineMpv::processEvents()
{
    // Process all events, until the event queue is empty.
    while(p->mpv) {
        mpv_event* event = mpv_wait_event(p->mpv, 0);
        if(event->event_id == MPV_EVENT_NONE) {
            break;
        }
        handleEvent(event);
    }
}

void EngineMpv::play()
{
    mpv_observe_property(p->mpv, 1, "audio-pts", MPV_FORMAT_DOUBLE);
    int f = 0;
    mpv_set_property_async(p->mpv, 0, "pause", MPV_FORMAT_FLAG, &f);
}

void EngineMpv::stop()
{
    mpv_unobserve_property(p->mpv, 1);

    const char* cmd[] = {"stop", nullptr}; // NOLINT
    mpv_command(p->mpv, cmd);
}

void EngineMpv::pause()
{
    int f = 1;
    mpv_set_property_async(p->mpv, 0, "pause", MPV_FORMAT_FLAG, &f);
}

void EngineMpv::seek(quint64 pos)
{
    const quint64 seconds = pos / 1000;
    const QByteArray tmp = QString::number(seconds).toUtf8();
    const char* cmd[] = {"seek", tmp.constData(), "absolute", nullptr}; // NOLINT
    mpv_command(p->mpv, cmd);
}

void EngineMpv::changeTrack(Track* track)
{
    const QByteArray path_ba = track->filepath().toUtf8();
    const char* cmd[] = {"loadfile", path_ba.constData(), "replace", nullptr}; // NOLINT
    mpv_command(p->mpv, cmd);
}

void EngineMpv::setVolume(float value)
{
    double volume = value;
    mpv_set_property_async(p->mpv, 0, "volume", MPV_FORMAT_DOUBLE, &volume);
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
            auto time = static_cast<quint64>((*static_cast<double*>(prop->data) * p->ms));

            if(time != p->lastTick && time > 0) {
                if(time + p->posInterval >= p->lastTick || time - p->posInterval <= p->lastTick) {
                    p->lastTick = time;
                    emit currentPositionChanged(time);
                }
            }
        }
    }
}
