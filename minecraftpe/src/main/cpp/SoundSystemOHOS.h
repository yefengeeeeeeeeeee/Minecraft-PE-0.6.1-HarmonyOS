#ifndef SOUNDSYSTEM_OHOS_H__
#define SOUNDSYSTEM_OHOS_H__

#include "platform/audio/SoundSystem.h"
#include "client/sound/Sound.h"
#include "ohaudio/native_audiorenderer.h"
#include "ohaudio/native_audiostream_base.h"

#include <atomic>
#include <mutex>


class SoundSystemOHOS : public SoundSystem
{
public:
    SoundSystemOHOS();
    virtual ~SoundSystemOHOS();

    virtual bool isAvailable() override { return true; }
    virtual void enable(bool status) override;

    virtual void setListenerPos(float x, float y, float z) override {}
    virtual void setListenerAngle(float deg) override {}

    virtual void playAt(const SoundDesc& desc, float x, float y, float z, float volume, float pitch) override;

private:
    // 真机实测：OHOS 上音频渲染器的 Create/Generate/Start/Flush 单次要 10-37ms，
    // "每个声音碰一次渲染器"（哪怕池化复用）都会把主线程顿出可感的卡顿，
    // 夜晚怪物齐叫时更是直接掉到个位数帧率。
    // 所以这里只创建 **一个** 常驻混音渲染器（48k 双声道），所有声音在写回调里
    // 软件混音；playAt 变成纯内存操作（填一个 Voice 槽位，微秒级）。
    static const int NUM_VOICES = 32;
    static const int MIX_RATE = 48000;

    struct Voice {
        std::atomic<int> state;     // 0=空闲 1=占用；主线程置 1，混音线程放完置 0
        const char* data;           // S16LE PCM（来自常驻内存的 SoundRepository）
        int totalFrames;            // 源总帧数
        int channels;               // 1 或 2
        double pos;                 // 当前读取位置（帧，含重采样步进）
        double step;                // 每输出一帧推进多少源帧 = rate/48000*pitch
        float volume;
    };

    bool ensureMixer();
    static int32_t onMixWrite(OH_AudioRenderer* renderer, void* userData, void* audioData, int32_t audioDataSize);
    void shutdownAll();

    Voice _voices[NUM_VOICES];
    OH_AudioRenderer* _mixer;
    std::atomic<bool> _enabled;
    std::mutex _mutex;              // 只保护 voice 分配与 _mixer 的懒创建
};

#endif /*SOUNDSYSTEM_OHOS_H__*/
