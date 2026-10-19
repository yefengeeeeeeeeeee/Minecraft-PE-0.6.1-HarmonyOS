#ifndef APPPLATFORM_OHOS_H__
#define APPPLATFORM_OHOS_H__

#include "AppPlatform.h"
#include "napi/native_api.h"

#include <string>

struct NativeResourceManager;

// 屏幕键盘（软键盘）桥接。
// 引擎侧（TextBox::setFocus / TextEditScreen / TouchscreenInput 的聊天按钮）
// 通过 AppPlatform::showKeyboard() / hideKeyboard() 表达「现在需要输入文本」，
// 这里只记一个标志；ArkTS 侧每帧轮询它，再决定弹出/收起系统输入法。
// 之所以不新加一个 .cpp，是因为 CMakeLists 里 entry 的源文件是显式列出的。
namespace ohos_soft_keyboard {
    void setRequested(bool wanted);
    bool isRequested();
}

class AppPlatform_ohos : public AppPlatform
{
    typedef AppPlatform super;
public:
    AppPlatform_ohos();
    virtual ~AppPlatform_ohos();

    void init(napi_env env, napi_value jsResMgr, const std::string& filesDir, int screenWidth, int screenHeight);
    void setScreenDimensions(int w, int h);

    virtual TextureData loadTexture(const std::string& filename_, bool textureFolder) override;
    virtual TextureData loadTextureFromMemory(const unsigned char* data, size_t size) override;
    virtual BinaryBlob readAssetFile(const std::string& filename) override;

    virtual int getScreenWidth() override { return _screenWidth; }
    virtual int getScreenHeight() override { return _screenHeight; }
    virtual float getPixelsPerMillimeter() override { return _pixelsPerMillimeter; }
    virtual bool supportsTouchscreen() override { return true; }
    virtual std::string getPlatformStringVar(int stringId) override;

    virtual void playSound(const std::string& fn, float volume, float pitch) override {}
    virtual void vibrate(int milliSeconds) override {}
    virtual void showDialog(int dialogId) override {}
    virtual void createUserInput() override {}
    virtual void openURL(const std::string& url) override {}
    virtual void finish() override {}

    // 输入法（软键盘）
    virtual void showKeyboard() override;
    virtual void hideKeyboard() override;
    virtual bool isKeyboardVisible() override { return ohos_soft_keyboard::isRequested(); }

    const std::string& getFilesDir() const { return _filesDir; }

private:
    TextureData decodePng(const unsigned char* data, size_t size);
    BinaryBlob readRawFile(const std::string& filename);

    bool _isInited;
    napi_env _env;
    NativeResourceManager* _resMgr;
    int _screenWidth;
    int _screenHeight;
    float _pixelsPerMillimeter;
    std::string _filesDir;
};

#endif /*APPPLATFORM_OHOS_H__*/