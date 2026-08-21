#include "SoundSystemOHOS.h"

#include "ohaudio/native_audiorenderer.h"
#include "ohaudio/native_audiostreambuilder.h"

#include <hilog/log.h>

#include <cstring>

#define LOGTAG "MCPE-Audio"
#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, LOGTAG, __VA_ARGS__)
#define LOGE(...) OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOGTAG, __VA_ARGS__)

#define MAX_SIMULTANEOUS_PLAYERS 24

SoundSystemOHOS::SoundSystemOHOS()
:   _enabled(true)
{
}

SoundSystemOHOS::~SoundSystemOHOS()
{
    shutdownAll();
}

void SoundSystemOHOS::enable(bool status)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _enabled = status;
}

void SoundSystemOHOS::shutdownAll()
{
    std::lock_guard<std::mutex> lock(_mutex);
    for (size_t i = 0; i < _players.size(); ++i) {
        Player* p = _players[i];
        if (p->renderer) {
            OH_AudioRenderer_Stop(p->renderer);
            OH_AudioRenderer_Release(p->renderer);
        }
        delete p;
    }
    _players.clear();
}

void SoundSystemOHOS::releaseFinished()
{
    std::lock_guard<std::mutex> lock(_mutex);
    for (size_t i = 0; i < _players.size();) {
        Player* p = _players[i];
        if (p->finished) {
            if (p->renderer) {
                OH_AudioRenderer_Stop(p->renderer);
                OH_AudioRenderer_Release(p->renderer);
            }
            delete p;
            _players.erase(_players.begin() + i);
        } else {
            ++i;
        }
    }
}

int32_t SoundSystemOHOS::onWriteData(OH_AudioRenderer* renderer, void* userData, void* audioData, int32_t audioDataSize)
{
    Player* p = (Player*)userData;
    if (!p || p->finished || audioDataSize <= 0) {
        return 0;
    }


    int toCopy = p->remainingBytes < audioDataSize ? p->remainingBytes : audioDataSize;
    if (toCopy > 0) {
        memcpy(audioData, p->data, toCopy);
        p->data += toCopy;
        p->remainingBytes -= toCopy;
    }

    if (p->remainingBytes <= 0) {
        p->finished = true;
        return 0;
    }

    return toCopy;
}

void SoundSystemOHOS::playAt(const SoundDesc& desc, float x, float y, float z, float volume, float pitch)
{
    if (!desc.isValid() || !desc.frames || desc.size <= 0) {
        return;
    }

    LOGI("playAt called: rate=%d ch=%d size=%d vol=%.2f pitch=%.2f", desc.frameRate, desc.channels, desc.size, volume, pitch);

    releaseFinished();

    std::lock_guard<std::mutex> lock(_mutex);
    if (!_enabled || _players.size() >= MAX_SIMULTANEOUS_PLAYERS) {
        return;
    }

    Player* p = new Player();
    p->renderer = nullptr;
    p->data = desc.frames;
    p->remainingBytes = desc.size;
    p->frameRate = desc.frameRate;
    p->channels = desc.channels;
    p->finished = false;

    OH_AudioStreamBuilder* builder = nullptr;
    OH_AudioStream_Result res = OH_AudioStreamBuilder_Create(&builder, AUDIOSTREAM_TYPE_RENDERER);
    if (res != AUDIOCOMMON_RESULT_SUCCESS || !builder) {
        LOGE("OH_AudioStreamBuilder_Create failed: %{public}d", res);
        delete p;
        return;
    }

    OH_AudioStreamBuilder_SetSamplingRate(builder, desc.frameRate);
    OH_AudioStreamBuilder_SetChannelCount(builder, desc.channels);
    OH_AudioStreamBuilder_SetSampleFormat(builder, AUDIOSTREAM_SAMPLE_S16LE);
    OH_AudioStreamBuilder_SetEncodingType(builder, AUDIOSTREAM_ENCODING_TYPE_RAW);
    OH_AudioStreamBuilder_SetLatencyMode(builder, AUDIOSTREAM_LATENCY_MODE_NORMAL);
    OH_AudioStreamBuilder_SetRendererInfo(builder, AUDIOSTREAM_USAGE_GAME);
    OH_AudioStreamBuilder_SetRendererWriteDataCallbackAdvanced(builder, SoundSystemOHOS::onWriteData, p);

    res = OH_AudioStreamBuilder_GenerateRenderer(builder, &p->renderer);
    OH_AudioStreamBuilder_Destroy(builder);
    if (res != AUDIOCOMMON_RESULT_SUCCESS || !p->renderer) {
        LOGE("OH_AudioStreamBuilder_GenerateRenderer failed: %{public}d", res);
        delete p;
        return;
    }

    float vol = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
    OH_AudioRenderer_SetVolume(p->renderer, vol);

    if (pitch > 0.01f && pitch != 1.0f) {
        OH_AudioRenderer_SetSpeed(p->renderer, pitch);
    }

    res = OH_AudioRenderer_Start(p->renderer);
    if (res != AUDIOCOMMON_RESULT_SUCCESS) {
        LOGE("OH_AudioRenderer_Start failed: %{public}d", res);
        OH_AudioRenderer_Release(p->renderer);
        delete p;
        return;
    }

    _players.push_back(p);
}
