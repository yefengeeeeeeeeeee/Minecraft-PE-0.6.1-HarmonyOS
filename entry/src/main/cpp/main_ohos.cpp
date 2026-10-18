#include "App.h"
#include "AppPlatform_ohos.h"
#include "NinecraftApp.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <native_window/external_window.h>
#include <hilog/log.h>

#include "platform/input/Mouse.h"
#include "platform/input/Multitouch.h"
#include "platform/input/Keyboard.h"

#include "napi/native_api.h"

#include <mutex>
#include "sys/stat.h"
#include <string>
#include <cstring>
#include <cstdint>
#include <cstdlib>

#define LOGTAG "MCPE-Main"
#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, LOGTAG, __VA_ARGS__)
#define LOGE(...) OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOGTAG, __VA_ARGS__)

namespace {

struct OhosEngine {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    OHNativeWindow* nativeWindow = nullptr;
    int width = 0;
    int height = 0;
    NinecraftApp* app = nullptr;
    AppContext appContext;
    bool inited = false;
};

OhosEngine g_engine;
AppPlatform_ohos g_platform;
std::mutex g_mutex;

bool EngineInitDisplay(uint64_t surfaceId, int32_t wantWidth, int32_t wantHeight)
{
    int32_t ret = OH_NativeWindow_CreateNativeWindowFromSurfaceId(surfaceId, &g_engine.nativeWindow);
    if (ret != 0 || g_engine.nativeWindow == nullptr) {
        LOGE("CreateNativeWindowFromSurfaceId FAILED ret=%{public}d", ret);
        return false;
    }

    // 必须把 XComponent 的实际尺寸写进 NativeWindow 的 buffer 几何，
    // 否则 eglSwapBuffers 时 buffer 尺寸与视口/裁剪不一致，表现为画面撕裂/拉伸/花屏。
    if (wantWidth > 0 && wantHeight > 0) {
        int32_t optRet = OH_NativeWindow_NativeWindowHandleOpt(
            g_engine.nativeWindow, SET_BUFFER_GEOMETRY, wantWidth, wantHeight);
        LOGI("SET_BUFFER_GEOMETRY %{public}dx%{public}d ret=%{public}d",
             wantWidth, wantHeight, optRet);
    }

    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay FAILED");
        return false;
    }
    EGLint major = 0, minor = 0;
    if (!eglInitialize(dpy, &major, &minor)) {
        LOGE("eglInitialize FAILED err=0x%{public}x", eglGetError());
        return false;
    }

    EGLint cfgAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_NONE
    };
    EGLConfig config = nullptr;
    EGLint numConfigs = 0;
    if (!eglChooseConfig(dpy, cfgAttribs, &config, 1, &numConfigs) || numConfigs == 0) {
        LOGE("eglChooseConfig FAILED num=%{public}d err=0x%{public}x", numConfigs, eglGetError());
        return false;
    }
    
    EGLint ctxAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    // 注意：第 4 个参数是「surface 属性表」，不能塞 context 属性（EGL_CONTEXT_CLIENT_VERSION
    // 不是合法 surface 属性，严格实现会直接返回 EGL_BAD_ATTRIBUTE → EGL_NO_SURFACE）。
    EGLSurface surface = eglCreateWindowSurface(dpy, config,
        (EGLNativeWindowType)g_engine.nativeWindow, nullptr);
    if (surface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface FAILED err=0x%{public}x", eglGetError());
        return false;
    }
    
    EGLContext context = eglCreateContext(dpy, config, EGL_NO_CONTEXT, ctxAttribs);
    if (context == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext FAILED err=0x%{public}x", eglGetError());
        return false;
    }

    if (!eglMakeCurrent(dpy, surface, surface, context)) {
        LOGE("eglMakeCurrent FAILED err=0x%{public}x", eglGetError());
        return false;
    }

    EGLint w = 0, h = 0;
    eglQuerySurface(dpy, surface, EGL_WIDTH, &w);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &h);
    LOGI("surface size=%{public}dx%{public}d", w, h);

    // 一次性打印 GL 环境：定位渲染问题先看这几行
    LOGI("GL_VENDOR=%{public}s", (const char*)glGetString(GL_VENDOR));
    LOGI("GL_RENDERER=%{public}s", (const char*)glGetString(GL_RENDERER));
    LOGI("GL_VERSION=%{public}s", (const char*)glGetString(GL_VERSION));
    LOGI("GL_SL_VERSION=%{public}s", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
    EGLint depthBits = 0, stencilBits = 0, sampleBits = 0;
    eglGetConfigAttrib(dpy, config, EGL_DEPTH_SIZE, &depthBits);
    eglGetConfigAttrib(dpy, config, EGL_STENCIL_SIZE, &stencilBits);
    eglGetConfigAttrib(dpy, config, EGL_SAMPLES, &sampleBits);
    LOGI("EGLConfig depth=%{public}d stencil=%{public}d samples=%{public}d",
         depthBits, stencilBits, sampleBits);

    g_engine.display = dpy;
    g_engine.surface = surface;
    g_engine.context = context;
    g_engine.width = w;
    g_engine.height = h;
    g_engine.inited = true;

    eglSwapInterval(dpy, 1);
    return true;
}

void EngineTermDisplay()
{
    if (g_engine.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_engine.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_engine.context != EGL_NO_CONTEXT) {
            eglDestroyContext(g_engine.display, g_engine.context);
        }
        if (g_engine.surface != EGL_NO_SURFACE) {
            eglDestroySurface(g_engine.display, g_engine.surface);
        }
        eglTerminate(g_engine.display);
    }
    if (g_engine.nativeWindow) {
        OH_NativeWindow_DestroyNativeWindow(g_engine.nativeWindow);
        g_engine.nativeWindow = nullptr;
    }
    g_engine.display = EGL_NO_DISPLAY;
    g_engine.surface = EGL_NO_SURFACE;
    g_engine.context = EGL_NO_CONTEXT;
    g_engine.inited = false;
}

uint64_t GetSurfaceId(napi_env env, napi_value arg)
{
    uint64_t surfaceId = 0;
    napi_valuetype type = napi_undefined;
    napi_typeof(env, arg, &type);
    if (type == napi_string) {
        size_t len = 0;
        napi_get_value_string_utf8(env, arg, nullptr, 0, &len);
        std::string buf(len, '\0');
        if (len > 0) {
            napi_get_value_string_utf8(env, arg, &buf[0], len + 1, &len);
        }
        surfaceId = strtoull(buf.c_str(), nullptr, 10);
    } else if (type == napi_number) {
        double v = 0.0;
        napi_get_value_double(env, arg, &v);
        surfaceId = (uint64_t)v;
    }
    return surfaceId;
}

std::string GetStringArg(napi_env env, napi_value arg)
{
    size_t len = 0;
    napi_get_value_string_utf8(env, arg, nullptr, 0, &len);
    std::string s(len, '\0');
    if (len > 0) {
        napi_get_value_string_utf8(env, arg, &s[0], len + 1, &len);
    }
    return s;
}

int32_t GetIntArg(napi_env env, napi_value arg, int32_t def)
{
    int32_t v = def;
    napi_get_value_int32(env, arg, &v);
    return v;
}

void PreseedOptionsFile(const std::string& filesDir)
{
    std::string path = filesDir + "/options.txt";
    
    struct stat buffer;
    if (stat(path.c_str(), &buffer) == 0) {
        return;
    }
    
    FILE* f = fopen(path.c_str(), "w");
    if (!f) {
        LOGE("PreseedOptionsFile failed to open %{public}s", path.c_str());
        return;
    }
    fprintf(f, "options.firstLaunch:false\n");
    fprintf(f, "options.username:SteveOHOS\n");
    fprintf(f, "options.useTouchscreen:true\n");
    fclose(f);
    LOGI("PreseedOptionsFile written: %{public}s", path.c_str());
}

} // namespace

static napi_value InitRender(napi_env env, napi_callback_info info)
{
    size_t argc = 6;
    napi_value args[6] = { nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_engine.app) {
        delete g_engine.app;
        g_engine.app = nullptr;
    }

    napi_value jsResMgr = (argc >= 1) ? args[0] : nullptr;
    uint64_t surfaceId = (argc >= 2) ? GetSurfaceId(env, args[1]) : 0;
    int32_t screenW = (argc >= 3) ? GetIntArg(env, args[2], 0) : 0;
    int32_t screenH = (argc >= 4) ? GetIntArg(env, args[3], 0) : 0;
    std::string filesDir = (argc >= 5) ? GetStringArg(env, args[4]) : std::string();

    if (surfaceId == 0) {
        LOGE("InitRender: invalid surfaceId");
        return nullptr;
    }

    g_platform.init(env, jsResMgr, filesDir, screenW, screenH);

    if (!EngineInitDisplay(surfaceId, screenW, screenH)) {
        LOGE("InitRender: EngineInitDisplay FAILED");
        return nullptr;
    }

    g_platform.setScreenDimensions(g_engine.width, g_engine.height);

    NinecraftApp* app = new NinecraftApp();
    g_engine.app = app;

    g_engine.appContext.display = g_engine.display;
    g_engine.appContext.context = g_engine.context;
    g_engine.appContext.surface = g_engine.surface;
    g_engine.appContext.platform = &g_platform;
    g_engine.appContext.doRender = true;

    app->externalStoragePath = g_platform.getFilesDir();
    app->externalCacheStoragePath = g_platform.getFilesDir();

    PreseedOptionsFile(g_platform.getFilesDir());

    app->App::init(g_engine.appContext);
    app->setSize(g_engine.width, g_engine.height);

    LOGI("InitRender OK size=%{public}dx%{public}d", g_engine.width, g_engine.height);

    napi_value ret = nullptr;
    napi_create_int32(env, 0, &ret);
    return ret;
}

static napi_value RenderFrame(napi_env env, napi_callback_info info)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_engine.app || !g_engine.inited) {
        return nullptr;
    }
    // 渲染循环可能被切到别的线程 / 上下文被系统回收（切前后台、surface 重建），
    // 这里每帧兜底一次，避免所有 GL 调用静默失败导致画面错乱。
    if (eglGetCurrentContext() != g_engine.context) {
        if (!eglMakeCurrent(g_engine.display, g_engine.surface, g_engine.surface, g_engine.context)) {
            static int s_makeCurrentFail = 0;
            if ((s_makeCurrentFail++ % 60) == 0) {
                LOGE("RenderFrame: eglMakeCurrent FAILED err=0x%{public}x", eglGetError());
            }
            return nullptr;
        }
    }
    g_engine.app->update();
    return nullptr;
}

static napi_value RenderTouch(napi_env env, napi_callback_info info)
{
    size_t argc = 4;
    napi_value args[4] = { nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    double x = 0, y = 0;
    if (argc >= 1) napi_get_value_double(env, args[0], &x);
    if (argc >= 2) napi_get_value_double(env, args[1], &y);
    int32_t action = (argc >= 3) ? GetIntArg(env, args[2], 0) : 0;
    int32_t pointerId = (argc >= 4) ? GetIntArg(env, args[3], 0) : 0;

    std::lock_guard<std::mutex> lock(g_mutex);

    short sx = (short)x;
    short sy = (short)y;

    LOGI("RenderTouch action=%{public}d x=%{public}d y=%{public}d pid=%{public}d", action, sx, sy, pointerId);
    switch (action) {
        case 1: // DOWN
            Mouse::feed(1, 1, sx, sy);
            Multitouch::feed(1, 1, sx, sy, (char)pointerId);
            break;
        case 0: // UP
            Mouse::feed(1, 0, sx, sy);
            Multitouch::feed(1, 0, sx, sy, (char)pointerId);
            break;
        case 2: // MOVE
        default:
            Mouse::feed(0, 0, sx, sy);
            Multitouch::feed(0, 0, sx, sy, (char)pointerId);
            break;
    }
    return nullptr;
}

static napi_value handleKeyBoard(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = { nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    int32_t action = 0;
    int32_t keyCode = 0;
    if (argc >= 1) napi_get_value_int32(env, args[0], &action);
    if (argc >= 2) napi_get_value_int32(env, args[1], &keyCode);

    std::lock_guard<std::mutex> lock(g_mutex);
    
    LOGI("RenderTouch action=%{public}d keyCode=%{public}d", action, keyCode);
    bool isDown = action == 0;
    Keyboard::feed(keyCode, isDown);
    return nullptr;
}

static napi_value DestroyRender(napi_env env, napi_callback_info info)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_engine.app) {
        delete g_engine.app;
        g_engine.app = nullptr;
    }
    EngineTermDisplay();
    return nullptr;
}

static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        {"initRender", nullptr, InitRender, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"renderFrame", nullptr, RenderFrame, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"renderTouch", nullptr, RenderTouch, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"handleKeyBoard", nullptr, handleKeyBoard, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"destroyRender", nullptr, DestroyRender, nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}

static napi_module mcpeModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "entry",
    .nm_priv = ((void*)0),
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void)
{
    napi_module_register(&mcpeModule);
}