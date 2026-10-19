#ifndef SOUNDSYSTEM_OHOS_H__
#define SOUNDSYSTEM_OHOS_H__

#include "platform/audio/SoundSystem.h"
#include "client/sound/Sound.h"
#include "ohaudio/native_audiorenderer.h"
#include "ohaudio/native_audiostream_base.h"


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
    struct Player {
        OH_AudioRenderer* renderer;
        const char* data;
        int remainingBytes;
        int frameRate;
        int channels;
        bool finished;
    };

    static int32_t onWriteData(OH_AudioRenderer* renderer, void* userData, void* audioData, int32_t audioDataSize);
    void releaseFinished();
    void shutdownAll();

    std::vector<Player*> _players;
    std::mutex _mutex;
    bool _enabled;
};

#endif /*SOUNDSYSTEM_OHOS_H__*/