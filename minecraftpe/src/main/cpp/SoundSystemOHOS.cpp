#include "SoundSystemOHOS.h"

#include "ohaudio/native_audiorenderer.h"
#include "ohaudio/native_audiostreambuilder.h"

#include <hilog/log.h>

#include <cstring>

#define LOGTAG "MCPE-Audio"
#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, LOGTAG, __VA_ARGS__)
#define LOGE(...) OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOGTAG, __VA_ARGS__)

SoundSystemOHOS::SoundSystemOHOS()
:   _mixer(nullptr),
    _enabled(true)
{
    for (int i = 0; i < NUM_VOICES; ++i) {
        _voices[i].state.store(0);
        _voices[i].data = nullptr;
        _voices[i].totalFrames = 0;
        _voices[i].channels = 1;
        _voices[i].pos = 0.0;
        _voices[i].step = 1.0;
        _voices[i].volume = 1.0f;
    }
}

SoundSystemOHOS::~SoundSystemOHOS()
{
    shutdownAll();
}

void SoundSystemOHOS::enable(bool status)
{
    _enabled.store(status);
}

void SoundSystemOHOS::shutdownAll()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_mixer) {
        OH_AudioRenderer_Stop(_mixer);
        OH_AudioRenderer_Release(_mixer);
        _mixer = nullptr;
    }
    for (int i = 0; i < NUM_VOICES; ++i) {
        _voices[i].state.store(0);
    }
}

bool SoundSystemOHOS::ensureMixer()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_mixer) {
        return true;
    }

    OH_AudioStreamBuilder* builder = nullptr;
    OH_AudioStream_Result res = OH_AudioStreamBuilder_Create(&builder, AUDIOSTREAM_TYPE_RENDERER);
    if (res != AUDIOCOMMON_RESULT_SUCCESS || !builder) {
        LOGE("mixer: StreamBuilder_Create failed: %{public}d", res);
        return false;
    }

    OH_AudioStreamBuilder_SetSamplingRate(builder, MIX_RATE);
    OH_AudioStreamBuilder_SetChannelCount(builder, 2);
    OH_AudioStreamBuilder_SetSampleFormat(builder, AUDIOSTREAM_SAMPLE_S16LE);
    OH_AudioStreamBuilder_SetEncodingType(builder, AUDIOSTREAM_ENCODING_TYPE_RAW);
    OH_AudioStreamBuilder_SetLatencyMode(builder, AUDIOSTREAM_LATENCY_MODE_NORMAL);
    OH_AudioStreamBuilder_SetRendererInfo(builder, AUDIOSTREAM_USAGE_GAME);
    OH_AudioStreamBuilder_SetRendererWriteDataCallbackAdvanced(builder, SoundSystemOHOS::onMixWrite, this);

    res = OH_AudioStreamBuilder_GenerateRenderer(builder, &_mixer);
    OH_AudioStreamBuilder_Destroy(builder);
    if (res != AUDIOCOMMON_RESULT_SUCCESS || !_mixer) {
        LOGE("mixer: GenerateRenderer failed: %{public}d", res);
        return false;
    }

    res = OH_AudioRenderer_Start(_mixer);
    if (res != AUDIOCOMMON_RESULT_SUCCESS) {
        LOGE("mixer: Start failed: %{public}d", res);
        OH_AudioRenderer_Release(_mixer);
        _mixer = nullptr;
        return false;
    }

    LOGI("mixer ready: %{public}dHz stereo, %{public}d voices", MIX_RATE, NUM_VOICES);
    return true;
}

// 混音线程回调：把所有活跃 Voice 就近重采样后饱和累加进输出。
// 只读 Voice 的 data/pos/step/volume/channels，交接靠 state 的 acquire/release。
int32_t SoundSystemOHOS::onMixWrite(OH_AudioRenderer* renderer, void* userData, void* audioData, int32_t audioDataSize)
{
    SoundSystemOHOS* self = (SoundSystemOHOS*)userData;
    if (!self || audioDataSize <= 0) {
        return 0;
    }

    int16_t* out = (int16_t*)audioData;
    int samples = audioDataSize / (int)sizeof(int16_t);
    memset(out, 0, audioDataSize);

    if (!self->_enabled.load(std::memory_order_relaxed)) {
        return audioDataSize;
    }

    for (int v = 0; v < NUM_VOICES; ++v) {
        Voice& vo = self->_voices[v];
        if (vo.state.load(std::memory_order_acquire) != 1) {
            continue;
        }

        const int16_t* src = (const int16_t*)vo.data;
        int frames = samples / 2;   // 输出固定双声道
        for (int f = 0; f < frames; ++f) {
            int idx = (int)vo.pos;
            if (idx >= vo.totalFrames) {
                vo.state.store(0, std::memory_order_release);
                break;
            }
            int16_t l = src[idx * vo.channels];
            int16_t r = (vo.channels >= 2) ? src[idx * vo.channels + 1] : l;

            int mixL = out[f * 2]     + (int)(l * vo.volume);
            int mixR = out[f * 2 + 1] + (int)(r * vo.volume);
            out[f * 2]     = (int16_t)(mixL > 32767 ? 32767 : (mixL < -32768 ? -32768 : mixL));
            out[f * 2 + 1] = (int16_t)(mixR > 32767 ? 32767 : (mixR < -32768 ? -32768 : mixR));

            vo.pos += vo.step;
        }
    }

    return audioDataSize;
}

void SoundSystemOHOS::playAt(const SoundDesc& desc, float x, float y, float z, float volume, float pitch)
{
    if (!desc.isValid() || !desc.frames || desc.size <= 0) {
        return;
    }
    if (desc.channels != 1 && desc.channels != 2) {
        return;
    }

    if (!ensureMixer()) {
        return;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    if (!_enabled.load()) {
        return;
    }

    int totalFrames = desc.size / (2 * desc.channels);  // S16LE
    if (totalFrames <= 0) {
        return;
    }

    for (int v = 0; v < NUM_VOICES; ++v) {
        Voice& vo = _voices[v];
        if (vo.state.load(std::memory_order_acquire) != 0) {
            continue;
        }
        vo.data = desc.frames;
        vo.totalFrames = totalFrames;
        vo.channels = desc.channels;
        vo.pos = 0.0;
        vo.step = (double)desc.frameRate / (double)MIX_RATE
                  * ((pitch > 0.01f && pitch != 1.0f) ? pitch : 1.0f);
        vo.volume = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
        vo.state.store(1, std::memory_order_release);
        return;
    }

    // 32 个 voice 全忙：丢弃（原实现超 24 个渲染器时同样是丢弃）
}
