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

#include "pipewireoutput.h"

#include <core/constants.h>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/utils/result.h>

extern "C"
{
#include <libavcodec/avcodec.h>
}

#include <QDebug>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
#pragma clang diagnostic ignored "-Wc99-extensions"
#endif

constexpr auto DefaultDevice = "default";

namespace {
spa_audio_format findSpaFormat(AVSampleFormat format)
{
    switch(format) {
        case(AV_SAMPLE_FMT_U8):
        case(AV_SAMPLE_FMT_U8P):
            return SPA_AUDIO_FORMAT_U8;
        case(AV_SAMPLE_FMT_S16):
        case(AV_SAMPLE_FMT_S16P):
            return SPA_AUDIO_FORMAT_S16;
        case(AV_SAMPLE_FMT_S32):
        case(AV_SAMPLE_FMT_S32P):
            return SPA_AUDIO_FORMAT_S32;
        case(AV_SAMPLE_FMT_FLT):
        case(AV_SAMPLE_FMT_FLTP):
            return SPA_AUDIO_FORMAT_F32;
        case(AV_SAMPLE_FMT_DBL):
        case(AV_SAMPLE_FMT_DBLP):
            return SPA_AUDIO_FORMAT_F64;
        case(AV_SAMPLE_FMT_NONE):
        case(AV_SAMPLE_FMT_S64):
        case(AV_SAMPLE_FMT_S64P):
        case(AV_SAMPLE_FMT_NB):
        default:
            return SPA_AUDIO_FORMAT_UNKNOWN;
    }
}

struct PwCoreDeleter
{
    void operator()(pw_core* core) const
    {
        if(core) {
            pw_core_disconnect(core);
        }
    }
};
using PwCoreUPtr = std::unique_ptr<pw_core, PwCoreDeleter>;

struct PwPropertiesDeleter
{
    void operator()(pw_properties* properties) const
    {
        if(properties) {
            pw_properties_free(properties);
        }
    }
};
using PwPropertiesUPtr = std::unique_ptr<pw_properties, PwPropertiesDeleter>;

struct PwStreamDeleter
{
    void operator()(pw_stream* stream) const
    {
        if(stream) {
            pw_stream_disconnect(stream);
            pw_stream_destroy(stream);
        }
    }
};
using PwStreamUPtr = std::unique_ptr<pw_stream, PwStreamDeleter>;

struct PwThreadLoopDeleter
{
    void operator()(pw_thread_loop* loop) const
    {
        if(loop) {
            pw_thread_loop_destroy(loop);
        }
    }
};
using PwThreadLoopUPtr = std::unique_ptr<pw_thread_loop, PwThreadLoopDeleter>;

struct PwRegistryDeleter
{
    void operator()(pw_registry* registry) const
    {
        if(registry) {
            pw_proxy_destroy(reinterpret_cast<pw_proxy*>(registry));
        }
    }
};
using PwRegistryUPtr = std::unique_ptr<pw_registry, PwRegistryDeleter>;

struct PwContextDeleter
{
    void operator()(pw_context* context) const
    {
        if(context) {
            pw_context_destroy(context);
        }
    }
};
using PwContextUPtr = std::unique_ptr<pw_context, PwContextDeleter>;

struct PipewireContext
{
    Fy::Core::Engine::OutputContext outputContext;

    PwCoreUPtr core;
    PwThreadLoopUPtr loop;
    PwStreamUPtr stream;
    PwRegistryUPtr registry;

    spa_hook streamListener;
    spa_hook coreListener;
    spa_hook registryListener;

    bool initialised{false};
    int bufferSize{2048};

    int coreInitSeq{0};

    Fy::Core::Engine::OutputDevices sinks;
};

void onError(void* /*data*/, uint32_t /*id*/, int /*seq*/, int res, const char* message)
{
    qWarning() << "PW: Error during playback: " << spa_strerror(res) << ", " << message; // NOLINT
}

void onCoreDone(void* data, uint32_t id, int seq)
{
    auto* pc = static_cast<PipewireContext*>(data);

    if(id != PW_ID_CORE || seq != pc->coreInitSeq) {
        return;
    }

    spa_hook_remove(&pc->registryListener);
    spa_hook_remove(&pc->coreListener);

    pc->initialised = true;
    pw_thread_loop_signal(pc->loop.get(), false);
}

void onRegistryEvent(void* data, uint32_t /*id*/, uint32_t /*permissions*/, const char* type, uint32_t /*version*/,
                     const struct spa_dict* props)
{
    auto* pc = static_cast<PipewireContext*>(data);
    if(!pc) {
        return;
    }

    if(strcmp(type, PW_TYPE_INTERFACE_Node) != 0) {
        return;
    }

    const char* media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    if(!media_class) {
        return;
    }

    if(strcmp(media_class, "Audio/Sink") != 0) {
        return;
    }

    const char* name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    const char* desc = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);

    pc->sinks.emplace_back(name, desc);

    pc->coreInitSeq = pw_core_sync(pc->core.get(), PW_ID_CORE, pc->coreInitSeq); // NOLINT
}

void onParamChanged(void* userdata, uint32_t id, const struct spa_pod* param)
{
    auto* pc = static_cast<PipewireContext*>(userdata);
    if(!pc) {
        return;
    }

    const spa_pod* params[1];
    uint8_t buffer[1024];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    if(!param || id != SPA_PARAM_Format) {
        return;
    }

    const int bufferSize
        = pc->bufferSize * findSpaFormat(pc->outputContext.format) * pc->outputContext.channelLayout.nb_channels;

    params[0] = static_cast<const spa_pod*>(spa_pod_builder_add_object(
        &b, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers, SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1),
        SPA_PARAM_BUFFERS_size, SPA_POD_CHOICE_RANGE_Int(bufferSize, 0, INT32_MAX), SPA_PARAM_BUFFERS_stride,
        SPA_POD_Int(pc->outputContext.sstride)));

    if(!params[0]) {
        qWarning() << "PW: Could not build parameter pod";
        return;
    }

    if(pw_stream_update_params(pc->stream.get(), params, 1) < 0) {
        qWarning() << "PW: Could not update stream parameters";
        return;
    }
}

void onStateChanged(void* userdata, enum pw_stream_state old, enum pw_stream_state state, const char* /*error*/)
{
    const auto* pc = static_cast<PipewireContext*>(userdata);
    if(!pc) {
        return;
    }

    // TODO: Handle errors/disconnections
    if(state == PW_STREAM_STATE_ERROR) { }

    if(state == PW_STREAM_STATE_UNCONNECTED && old != PW_STREAM_STATE_UNCONNECTED) { }
}

void onProcess(void* userData)
{
    auto* pc = static_cast<PipewireContext*>(userData);
    if(!pc) {
        return;
    }

    pw_buffer* b{pw_stream_dequeue_buffer(pc->stream.get())};
    if(!b) {
        return;
    }

    spa_buffer* buf = b->buffer;

    const int sstride   = pc->outputContext.sstride;
    uint64_t frameCount = buf->datas[0].maxsize / pc->outputContext.channelLayout.nb_channels / sstride;
    if(b->requested != 0) {
        frameCount = std::min(b->requested, frameCount);
    }

    const int samples = pc->outputContext.writeAudioToBuffer(std::bit_cast<uint8_t*>(buf->datas[0].data),
                                                             static_cast<int>(frameCount));

    b->size                     = samples;
    buf->datas[0].chunk->size   = samples * sstride;
    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = pc->outputContext.sstride;

    pw_stream_queue_buffer(pc->stream.get(), b);
}

void updateChannelMap(spa_audio_info_raw* info, int channels)
{
    switch(channels) {
        case(9):
            info->position[8] = SPA_AUDIO_CHANNEL_RC;
            // Fall through
        case(8):
            info->position[6] = SPA_AUDIO_CHANNEL_FLC;
            info->position[7] = SPA_AUDIO_CHANNEL_FRC;
            // Fall through
        case(6):
            info->position[4] = SPA_AUDIO_CHANNEL_RL;
            info->position[5] = SPA_AUDIO_CHANNEL_RR;
            // Fall through
        case(4):
            info->position[3] = SPA_AUDIO_CHANNEL_LFE;
            // Fall through
        case(3):
            info->position[2] = SPA_AUDIO_CHANNEL_FC;
            // Fall through
        case(2):
            info->position[0] = SPA_AUDIO_CHANNEL_FL;
            info->position[1] = SPA_AUDIO_CHANNEL_FR;
            break;
        case(1):
            info->position[0] = SPA_AUDIO_CHANNEL_MONO;
            break;
    }
}

} // namespace

namespace Fy::Core::Engine {
struct PipeWireOutput::Private
{
    PipewireContext pc;
    PwContextUPtr context;
    char* remote;
    QString device;
    bool muted{false};

    bool initCore()
    {
        pc.loop = PwThreadLoopUPtr{pw_thread_loop_new("fooyin/audioouput/pipewire", nullptr)};
        if(!pc.loop) {
            qWarning() << "PW: Could not create main loop";
            return false;
        }

        context = PwContextUPtr{pw_context_new(pw_thread_loop_get_loop(pc.loop.get()),
                                               pw_properties_new(PW_KEY_CONFIG_NAME, "client-rt.conf", nullptr), 0)};
        if(!context) {
            qWarning() << "PW: Could not create context";
            return false;
        }

        pc.core
            = PwCoreUPtr{pw_context_connect(context.get(), pw_properties_new(PW_KEY_REMOTE_NAME, remote, nullptr), 0)};
        if(!pc.core) {
            qWarning() << "PW: Could not connect to context";
            return false;
        }

        static const pw_core_events coreEvents = {
            .version = PW_VERSION_CORE_EVENTS,
            .done    = onCoreDone,
            .error   = onError,
        };

        int err = pw_core_add_listener(pc.core.get(), &pc.coreListener, &coreEvents, &pc); // NOLINT
        if(err < 0) {
            qWarning() << "PW: Could not add core listener";
            return false;
        }

        pc.registry = PwRegistryUPtr{pw_core_get_registry(pc.core.get(), PW_VERSION_REGISTRY, 0)};
        if(!pc.registry) {
            qWarning() << "PW: Could not retrieve registry";
            return false;
        }

        static const pw_registry_events registryEvents = {
            .version = PW_VERSION_REGISTRY_EVENTS,
            .global  = onRegistryEvent,
        };

        err = pw_registry_add_listener(pc.registry.get(), &pc.registryListener, &registryEvents, &pc); // NOLINT
        if(err < 0) {
            qWarning() << "PW: Could not add registry listener";
            return false;
        }

        if(pw_thread_loop_start(pc.loop.get()) != 0) {
            qWarning() << "PW: Could not start thread loop";
            return false;
        }

        pw_thread_loop_lock(pc.loop.get());
        while(!pc.initialised) {
            if(pw_thread_loop_timed_wait(pc.loop.get(), 2) != 0) {
                break;
            }
        }
        pw_thread_loop_unlock(pc.loop.get());

        if(!pc.initialised) {
            qWarning() << "PW: Could not initialise loop";
            return false;
        }

        return true;
    }

    bool initStream()
    {
        static const pw_stream_events streamEvents = {
            .version       = PW_VERSION_STREAM_EVENTS,
            .state_changed = onStateChanged,
            .param_changed = onParamChanged,
            .process       = onProcess,
        };

        pw_thread_loop_lock(pc.loop.get());

        auto properties = PwPropertiesUPtr{pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", nullptr)};
        if(!properties) {
            qWarning() << "PW: Could not create properties";
            pw_thread_loop_unlock(pc.loop.get());
            return false;
        }
        auto* props = properties.get();

        pw_properties_setf(props, PW_KEY_MEDIA_CATEGORY, "Playback");
        pw_properties_setf(props, PW_KEY_MEDIA_ROLE, "Music");
        pw_properties_setf(props, PW_KEY_APP_ID, Constants::AppName);
        pw_properties_setf(props, PW_KEY_APP_ICON_NAME, Constants::AppName);
        pw_properties_setf(props, PW_KEY_APP_NAME, Constants::AppName);
        pw_properties_setf(props, PW_KEY_NODE_ALWAYS_PROCESS, "true");
        pw_properties_setf(props, PW_KEY_NODE_RATE, "1/%u", pc.outputContext.sampleRate);

        if(device != DefaultDevice) {
            pw_properties_setf(props, PW_KEY_TARGET_OBJECT, "%s", device.toUtf8().constData());
        }

        // Stream takes ownership of properties
        pc.stream = PwStreamUPtr{pw_stream_new(pc.core.get(), "audio-src", properties.release())};
        if(!pc.stream) {
            qWarning() << "PW: Could not create stream";
            pw_thread_loop_unlock(pc.loop.get());
            return false;
        }

        pw_stream_add_listener(pc.stream.get(), &pc.streamListener, &streamEvents, &pc);

        const spa_audio_format spaFormat = findSpaFormat(pc.outputContext.format);
        if(spaFormat == SPA_AUDIO_FORMAT_UNKNOWN) {
            qWarning() << "PW: Unknown audio format";
            pw_thread_loop_unlock(pc.loop.get());
            return false;
        }

        const spa_pod* params[1];
        uint8_t buffer[1024];
        spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

        spa_audio_info_raw audioInfo = {
            .format   = spaFormat,
            .flags    = SPA_AUDIO_FLAG_NONE,
            .rate     = static_cast<uint32_t>(pc.outputContext.sampleRate),
            .channels = static_cast<uint32_t>(pc.outputContext.channelLayout.nb_channels),
        };

        updateChannelMap(&audioInfo, pc.outputContext.channelLayout.nb_channels);

        params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audioInfo);

        auto flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_INACTIVE
                                                  | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS);

        if(pw_stream_connect(pc.stream.get(), PW_DIRECTION_OUTPUT, PW_ID_ANY, flags, params, 1) < 0) {
            pw_thread_loop_unlock(pc.loop.get());
            return false;
        }

        pw_thread_loop_unlock(pc.loop.get());
        return true;
    }

    void uninit()
    {
        if(pc.loop) {
            pw_thread_loop_stop(pc.loop.get());
        }

        spa_hook_remove(&pc.streamListener);
        spa_zero(pc.streamListener);

        pc.stream.reset(nullptr);
        pc.registry.reset(nullptr);
        pc.core.reset(nullptr);
        context.reset(nullptr);

        pc.loop.reset(nullptr);

        pw_deinit();

        pc = {};
    }
};

PipeWireOutput::PipeWireOutput()
    : p{std::make_unique<Private>()}
{ }

PipeWireOutput::~PipeWireOutput() = default;

bool PipeWireOutput::init(const OutputContext& oc)
{
    if(p->pc.initialised) {
        return false;
    }

    p->pc.outputContext = oc;

    pw_init(nullptr, nullptr);

    if(!p->initCore() || !p->initStream()) {
        uninit();
        return false;
    }

    setVolume(oc.volume);

    return true;
}

void PipeWireOutput::uninit()
{
    p->uninit();
}

void PipeWireOutput::reset()
{
    pw_thread_loop_lock(p->pc.loop.get());
    pw_stream_set_active(p->pc.stream.get(), false);
    pw_stream_flush(p->pc.stream.get(), false);
    pw_thread_loop_unlock(p->pc.loop.get());
}

void PipeWireOutput::start()
{
    pw_thread_loop_lock(p->pc.loop.get());
    pw_stream_set_active(p->pc.stream.get(), true);
    pw_thread_loop_unlock(p->pc.loop.get());
}

bool PipeWireOutput::initialised() const
{
    return p->pc.initialised;
}

QString PipeWireOutput::device() const
{
    return p->device;
}

bool PipeWireOutput::canHandleVolume() const
{
    return true;
}

OutputDevices PipeWireOutput::getAllDevices() const
{
    OutputDevices devices;

    devices.emplace_back(DefaultDevice, "Default");

    if(!p->pc.initialised) {
        pw_init(nullptr, nullptr);

        p->initCore();
        std::ranges::copy(p->pc.sinks, std::back_inserter(devices));
        p->uninit();
    }
    else {
        std::ranges::copy(p->pc.sinks, std::back_inserter(devices));
    }

    return devices;
}

void PipeWireOutput::setVolume(double volume)
{
    if(!initialised()) {
        return;
    }

    pw_thread_loop_lock(p->pc.loop.get());

    auto vol = static_cast<float>(volume);

    if(p->muted || vol == 0.0) {
        float mute = p->muted ? 0.0F : 1.0F;
        p->muted   = !p->muted;
        if(pw_stream_set_control(p->pc.stream.get(), SPA_PROP_mute, 1, &mute, 0) < 0) {
            qDebug() << "PW: Could not mute";
        }
    }

    if(vol > 0.0) {
        if(pw_stream_set_control(p->pc.stream.get(), SPA_PROP_volume, 1, &vol, 0) < 0) {
            qDebug() << "PW: Could not set volume";
        }
    }

    pw_thread_loop_unlock(p->pc.loop.get());
}

void PipeWireOutput::setDevice(const QString& device)
{
    if(!device.isEmpty()) {
        p->device = device;
    }
}
} // namespace Fy::Core::Engine
