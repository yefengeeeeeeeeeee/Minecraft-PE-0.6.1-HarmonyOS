#include "AppPlatform_ohos.h"

#include "rawfile/raw_file_manager.h"
#include "rawfile/raw_file.h"

#include "multimedia/image_framework/image_source_mdk.h"
#include "multimedia/image_framework/image_pixel_map_mdk.h"

#include <hilog/log.h>
#include <cstring>

#define PIXEL_FORMAT_RGBA_8888 3

#define LOGTAG "MCPE-Platform"
#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, LOGTAG, __VA_ARGS__)
#define LOGE(...) OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOGTAG, __VA_ARGS__)

AppPlatform_ohos::AppPlatform_ohos()
:   _isInited(false),
    _env(nullptr),
    _resMgr(nullptr),
    _screenWidth(1280),
    _screenHeight(800),
    _pixelsPerMillimeter(10.0f)
{
}

AppPlatform_ohos::~AppPlatform_ohos()
{
    if (_resMgr) {
        OH_ResourceManager_ReleaseNativeResourceManager(_resMgr);
        _resMgr = nullptr;
    }
}

void AppPlatform_ohos::init(napi_env env, napi_value jsResMgr, const std::string& filesDir,
    int screenWidth, int screenHeight)
{
    if (_isInited) return;

    _env = env;
    _filesDir = filesDir;
    if (screenWidth > 0) _screenWidth = screenWidth;
    if (screenHeight > 0) _screenHeight = screenHeight;
    _pixelsPerMillimeter = 10.0f;

    if (jsResMgr) {
        _resMgr = OH_ResourceManager_InitNativeResourceManager(env, jsResMgr);
        if (!_resMgr) {
            LOGE("OH_ResourceManager_InitNativeResourceManager FAILED");
        }
    }

    _isInited = true;
    LOGI("AppPlatform_ohos::init ok filesDir=%{public}s size=%{public}dx%{public}d",
        filesDir.c_str(), _screenWidth, _screenHeight);
}

void AppPlatform_ohos::setScreenDimensions(int w, int h)
{
    if (w > 0) _screenWidth = w;
    if (h > 0) _screenHeight = h;
}

BinaryBlob AppPlatform_ohos::readRawFile(const std::string& filename)
{
    if (!_isInited || !_resMgr) {
        LOGE("readRawFile not inited: %{public}s", filename.c_str());
        return BinaryBlob();
    }

    RawFile* f = OH_ResourceManager_OpenRawFile(_resMgr, filename.c_str());
    if (!f) {
        LOGI("OpenRawFile FAILED: %{public}s", filename.c_str());
        return BinaryBlob();
    }

    long size = OH_ResourceManager_GetRawFileSize(f);
    if (size <= 0) {
        OH_ResourceManager_CloseRawFile(f);
        return BinaryBlob();
    }

    unsigned char* buf = new unsigned char[size];
    int read = OH_ResourceManager_ReadRawFile(f, buf, (size_t)size);
    OH_ResourceManager_CloseRawFile(f);

    if (read <= 0) {
        delete[] buf;
        return BinaryBlob();
    }

    return BinaryBlob(buf, read);
}

BinaryBlob AppPlatform_ohos::readAssetFile(const std::string& filename)
{
    return readRawFile(filename);
}

std::string AppPlatform_ohos::getPlatformStringVar(int stringId)
{
    if (stringId == PlatformStringVars::DEVICE_BUILD_MODEL)
        return "OHOS";
    return "";
}

TextureData AppPlatform_ohos::loadTexture(const std::string& filename_, bool textureFolder)
{
    std::string path = textureFolder ? "images/" + filename_ : filename_;
    BinaryBlob blob = readRawFile(path);
    if (!blob.data && textureFolder) {
        blob = readRawFile(filename_);
    }
    if (!blob.data) {
        LOGE("loadTexture file not found: %{public}s", path.c_str());
        return TextureData();
    }
    TextureData out = decodePng(blob.data, blob.size);
    delete[] blob.data;
    return out;
}

TextureData AppPlatform_ohos::loadTextureFromMemory(const unsigned char* data, size_t size)
{
    return decodePng(data, size);
}

TextureData AppPlatform_ohos::decodePng(const unsigned char* data, size_t size)
{
    TextureData out;
    if (!_isInited || !_env || !data || size == 0) {
        LOGE("decodePng bad params");
        return out;
    }

    OhosImageSourceOps ops;
    memset(&ops, 0, sizeof(ops));

    napi_value jsImageSource = nullptr;
    int32_t ret = OH_ImageSource_CreateFromData(_env, (uint8_t*)data, size, &ops, &jsImageSource);
    if (ret != IMAGE_RESULT_SUCCESS || !jsImageSource) {
        LOGE("OH_ImageSource_CreateFromData FAILED ret=%{public}d", ret);
        return out;
    }

    ImageSourceNative* native = OH_ImageSource_InitNative(_env, jsImageSource);
    if (!native) {
        LOGE("OH_ImageSource_InitNative FAILED");
        return out;
    }

    OhosImageDecodingOps decodeOps;
    memset(&decodeOps, 0, sizeof(decodeOps));
    decodeOps.pixelFormat = PIXEL_FORMAT_RGBA_8888;

    napi_value jsPixelMap = nullptr;
    ret = OH_ImageSource_CreatePixelMap(native, &decodeOps, &jsPixelMap);
    if (ret != IMAGE_RESULT_SUCCESS || !jsPixelMap) {
        LOGE("OH_ImageSource_CreatePixelMap FAILED ret=%{public}d", ret);
        OH_ImageSource_Release(native);
        return out;
    }

    NativePixelMap* pixelMap = OH_PixelMap_InitNativePixelMap(_env, jsPixelMap);
    if (!pixelMap) {
        LOGE("OH_PixelMap_InitNativePixelMap FAILED");
        OH_ImageSource_Release(native);
        return out;
    }

    OhosPixelMapInfos info;
    memset(&info, 0, sizeof(info));
    if (OH_PixelMap_GetImageInfo(pixelMap, &info) != IMAGE_RESULT_SUCCESS) {
        LOGE("OH_PixelMap_GetImageInfo FAILED");
        OH_ImageSource_Release(native);
        return out;
    }

    void* addr = nullptr;
    if (OH_PixelMap_AccessPixels(pixelMap, &addr) != IMAGE_RESULT_SUCCESS || !addr) {
        LOGE("OH_PixelMap_AccessPixels FAILED");
        OH_ImageSource_Release(native);
        return out;
    }

    size_t dataBytes = (size_t)info.rowSize * info.height;
    unsigned char* pixels = new unsigned char[dataBytes];
    memcpy(pixels, addr, dataBytes);

    OH_PixelMap_UnAccessPixels(pixelMap);
    OH_ImageSource_Release(native);

    out.w = (int)info.width;
    out.h = (int)info.height;
    out.numBytes = (int)dataBytes;
    out.data = pixels;
    out.transparent = true;
    out.memoryHandledExternally = false;
    out.format = TEXF_UNCOMPRESSED_8888;

    return out;
}