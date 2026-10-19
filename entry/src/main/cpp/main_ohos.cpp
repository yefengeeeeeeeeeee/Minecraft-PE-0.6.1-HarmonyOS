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

// 把鸿蒙 KeyCode（@ohos.multimodalInput.keyCode）映射成引擎期望的 ASCII 键码。
// 引擎侧 Keyboard / Options 默认键码是 ASCII：W=87 A=65 S=83 D=68 SPACE=32 SHIFT=10 E=69 Q=81 T=84 ...
// 鸿蒙枚举则是 KEYCODE_A=2017 ... KEYCODE_W=2039，二者完全不是一套编码，必须显式转换。
static int OhosKeyCodeToMcpe(int keyCode)
{
    switch (keyCode) {
        // 字母 A-Z：2017..2042 -> 65..90
        case 2017: return 'A';
        case 2018: return 'B';
        case 2019: return 'C';
        case 2020: return 'D';
        case 2021: return 'E';
        case 2022: return 'F';
        case 2023: return 'G';
        case 2024: return 'H';
        case 2025: return 'I';
        case 2026: return 'J';
        case 2027: return 'K';
        case 2028: return 'L';
        case 2029: return 'M';
        case 2030: return 'N';
        case 2031: return 'O';
        case 2032: return 'P';
        case 2033: return 'Q';
        case 2034: return 'R';
        case 2035: return 'S';
        case 2036: return 'T';
        case 2037: return 'U';
        case 2038: return 'V';
        case 2039: return 'W';
        case 2040: return 'X';
        case 2041: return 'Y';
        case 2042: return 'Z';
        // 数字 0-9：2000..2009 -> 48..57
        case 2000: return '0';
        case 2001: return '1';
        case 2002: return '2';
        case 2003: return '3';
        case 2004: return '4';
        case 2005: return '5';
        case 2006: return '6';
        case 2007: return '7';
        case 2008: return '8';
        case 2009: return '9';
        // 空格 / 回车 / Tab
        case 2050: return ' ';
        case 2054: return 13;   // KEYCODE_ENTER -> RETURN
        case 2049: return 9;    // KEYCODE_TAB
        // 功能键
        case 2090: return 112;  // F1
        case 2091: return 113;  // F2
        case 2092: return 114;  // F3
        case 2093: return 115;  // F4
        case 2094: return 116;  // F5
        case 2095: return 117;  // F6
        case 2096: return 118;  // F7
        case 2097: return 119;  // F8
        case 2098: return 120;  // F9
        case 2099: return 121;  // F10
        case 2100: return 122;  // F11
        case 2101: return 123;  // F12
        // Escape / 退格 / 方向键（方向键映射到引擎自定义：左37 上38 右39 下40，见 keyMenu* 默认值）
        case 2070: return 27;   // ESCAPE
        case 2055: return 8;    // DEL -> BACKSPACE
        case 2071: return 127;  // FORWARD_DEL
        case 2012: return 38;   // DPAD_UP
        case 2013: return 40;   // DPAD_DOWN
        case 2014: return 37;   // DPAD_LEFT
        case 2015: return 39;   // DPAD_RIGHT
        // 左右 Shift -> 10（引擎 KEY_LSHIFT）
        case 2047:
        case 2048: return 10;
        // 左右 Ctrl -> 232（引擎 KEY_LEFT_CTRL）
        case 2072:
        case 2073: return 232;
        default:
            return -1;
    }
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

    // 鸿蒙 KeyType: Down=0, Up=1, CANCEL=3（API 26+ 系统补发）。
    // 只接受明确的 Down / Up；CANCEL 等其他值一律忽略，避免把
    // 「按住移动」的持续状态意外清成 KEYUP，导致无法长按持续行动。
    bool isDown;
    if (action == 0) {
        isDown = true;
    } else if (action == 1) {
        isDown = false;
    } else {
        return nullptr; // CANCEL(3) 等：忽略
    }

    int mcpeKey = OhosKeyCodeToMcpe(keyCode);
    if (mcpeKey < 0) {
        return nullptr;
    }

    // 关键优化：长按一个键时，鸿蒙会高频触发 repeat Down（每秒数十次）。
    // 若每次都 feed，会 1) 让 Keyboard::_inputs 队列暴涨、2) 跨 NAPI + 抢 g_mutex
    // 频繁打断渲染循环，导致「按住前进却一卡一卡」。
    // 因此：DOWN 且该键已经处于 DOWN 状态 → 直接忽略（重复按下）；UP 才透传。
    // 反过来，UP 也必须是幂等的：ArkTS 侧在去抖/兜底时可能补发多次 Up，
    // 重复的 Up 再喂一遍没有意义，只会让上层多处理一次按键事件。
    bool alreadyDown = Keyboard::isKeyDown(mcpeKey);
    if (isDown && alreadyDown) {
        return nullptr; // repeat Down，去重
    }
    if (!isDown && !alreadyDown) {
        return nullptr; // 重复 Up，去重
    }

    LOGI("handleKeyBoard keyCode=%{public}d -> mcpeKey=%{public}d %{public}s",
         keyCode, mcpeKey, isDown ? "DOWN" : "UP");

    // Keyboard::feed 的第二个参数期望 KeyboardAction::KEYDOWN(1)/KEYUP(0)
    Keyboard::feed((unsigned char)mcpeKey, isDown ? KeyboardAction::KEYDOWN : KeyboardAction::KEYUP);
    return nullptr;
}

static napi_value handleKeyText(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = { nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        return nullptr;
    }

    std::string text = GetStringArg(env, args[0]);
    int32_t keyCode = (argc >= 2) ? GetIntArg(env, args[1], -1) : -1;

    std::lock_guard<std::mutex> lock(g_mutex);

    // 关键：鸿蒙的 event.keyText 对「功能键 / 移动键」返回的是键名（如 "Shift"、
    // "Space"、"KEYCODE_I"），对字母返回单字符 "w"。如果无脑逐字符投递，会把
    // "Shift" 拆成 'S','h','i','f','t' 五个字符打进告示牌 / 聊天框，出现乱符号。
    // 策略：
    //   1) keyText 是单个可打印 ASCII 字符（真实键盘 / IME 的正常情况）→ 直接投递；
    //   2) 否则用 keyCode 映射表回退成 ASCII 字符投递（模拟器 uinput 的
    //      keyText 形如 "KEYCODE_I"，必须靠 keyCode 才能得到真实字符）。
    char ch = 0;
    if (text.size() == 1) {
        unsigned char c = (unsigned char)text[0];
        if (c >= 32 && c < 127) {
            ch = (char)c;
        }
    } else if (keyCode >= 0) {
        int mapped = OhosKeyCodeToMcpe(keyCode);
        if (mapped >= 32 && mapped < 127) {
            ch = (char)mapped;
        }
    }
    
    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
        bool shiftDown = Keyboard::isKeyDown(Keyboard::KEY_LSHIFT);
        if (shiftDown && ch >= 'a' && ch <= 'z') {
            ch = (char)(ch - 'a' + 'A');
        } else if (!shiftDown && ch >= 'A' && ch <= 'Z') {
            ch = (char)(ch - 'A' + 'a');
        }
    }

    if (ch) {
        Keyboard::feedText(ch);
    }
    return nullptr;
}

// 软键盘 / 输入法一次性提交的文本（可能一次给出多个字符，例如拼音候选词上屏）。
// ArkTS 侧的隐藏输入框按「增量」把新输入的内容送到这里，原样保留大小写。
static napi_value FeedText(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        return nullptr;
    }

    std::string text = GetStringArg(env, args[0]);

    std::lock_guard<std::mutex> lock(g_mutex);

    // MCPE 0.6.1 的字体是 ASCII 位图，非 ASCII 字符画不出来，直接丢弃；
    // 但必须按 UTF-8 码点整体跳过，不能按字节拆（否则会拆出半个字符的乱码字节）。
    size_t i = 0;
    while (i < text.size()) {
        unsigned char c = (unsigned char)text[i];
        size_t len = 1;
        if ((c & 0xE0) == 0xC0) {
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            len = 4;
        }
        if (i + len > text.size()) {
            break;
        }
        if (len == 1 && c >= 32 && c < 127) {
            Keyboard::feedText((char)c);
        }
        i += len;
    }
    return nullptr;
}

// ArkTS 每帧核对一次「引擎里这个键现在还是按下状态吗」。
// 用途：长按期间如果系统/模拟器补发了 Up（或事件成对抖动），引擎里的按键
// 状态会被清掉，玩家就会表现为「走一下、停一下」。ArkTS 侧拿这个查询做自愈：
// 发现状态丢了就立刻补一次按下。参数是鸿蒙键码，映射不到的一律返回 true
// （调用方据此不再补发，避免给功能键发送无意义的重复按下）。
static napi_value IsKeyDown(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    int32_t keyCode = (argc >= 1) ? GetIntArg(env, args[0], -1) : -1;

    bool down = true;
    int mcpeKey = (keyCode >= 0) ? OhosKeyCodeToMcpe(keyCode) : -1;
    if (mcpeKey >= 0) {
        down = Keyboard::isKeyDown(mcpeKey);
    }

    napi_value ret = nullptr;
    napi_get_boolean(env, down, &ret);
    return ret;
}

// ArkTS 每帧问一次：引擎现在需不需要文本输入？需要就去聚焦隐藏输入框，
// 系统输入法（屏幕键盘）会随焦点自动弹出。
static napi_value IsKeyboardRequested(napi_env env, napi_callback_info info)
{
    napi_value ret = nullptr;
    napi_get_boolean(env, ohos_soft_keyboard::isRequested(), &ret);
    return ret;
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
        {"handleKeyText", nullptr, handleKeyText, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"feedText", nullptr, FeedText, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"isKeyDown", nullptr, IsKeyDown, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"isKeyboardRequested", nullptr, IsKeyboardRequested, nullptr, nullptr, nullptr, napi_default, nullptr},
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