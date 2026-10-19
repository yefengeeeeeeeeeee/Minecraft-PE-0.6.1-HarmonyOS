# MCPE-ohos 项目汇总

## 一、项目概述

本项目是将开源 Minecraft Pocket Edition（MCPE，代号 `NinecraftApp`）客户端移植到 HarmonyOS（鸿蒙）的工程。游戏本体 C++ 源码位于 `third_party/mcpe/src`，通过一层 OHOS 平台适配层接入鸿蒙 ArkUI/NAPI 渲染框架。

| 项目 | 内容 |
|------|------|
| 工程名 | mcpe-ohos |
| 包名（bundleName） | `net.minecraft.pe` |
| 应用名 | MCPE game port |
| 版本 | 1.0.0（versionCode 1000000） |
| SDK | API 26（targetSdkVersion / compatibleSdkVersion / compileSdkVersion 均为 26.0.0） |
| 运行系统 | HarmonyOS |
| 屏幕方向 | 横屏（landscape），全屏沉浸式 |
| ABI | arm64-v8a、x86_64 |

## 二、目录结构

```
mcpe-ohos/
├── AppScope/                    # 应用级配置
│   ├── app.json5                # bundleName、版本、图标
│   └── resources/               # 图标、字符串
├── entry/                       # 主模块（唯一模块）
│   ├── build-profile.json5      # native 构建配置（CMake 路径、abiFilters）
│   ├── obfuscation-rules.txt
│   ├── oh-package.json5
│   └── src/main/
│       ├── module.json5         # EntryAbility 声明、横屏
│       ├── ets/
│       │   ├── entryability/EntryAbility.ets   # 全屏/横屏设置
│       │   └── pages/Index.ets                 # XComponent + 渲染循环 + 触摸转发
│       ├── cpp/                 # OHOS 平台适配层（NAPI + EGL）
│       │   ├── main_ohos.cpp    # 引擎初始化、EGL 上下文、渲染/触摸 NAPI 入口
│       │   ├── AppPlatform_ohos.cpp/.h   # 平台接口实现
│       │   ├── SoundSystemOHOS.cpp/.h    # OHOS 音频后端
│       │   ├── gles1_dyn.cpp/.h          # GLES1 动态加载
│       │   ├── gles1_stub.cpp / gles1_consts.h
│       │   ├── GLES/gl.h, glext.h        # 本地 GLES 头文件
│       │   ├── CMakeLists.txt             # 编译脚本（GLOB 收集 mcpe 源码）
│       │   └── types/libentry/Index.d.ts  # NAPI 类型声明
│       └── resources/
│           ├── base/            # 颜色/字符串/图标/启动页
│           └── rawfile/         # 游戏资源（armor/art/environment/font/gui/images/item/lang/mob 等）
├── third_party/mcpe/src/        # MCPE 游戏本体 C++ 源码
│   ├── client/                  # 客户端渲染/游戏逻辑
│   ├── server/                  # 服务端逻辑
│   ├── world/                   # 世界/方块/实体
│   ├── network/  raknet/        # 网络与 RakNet 协议
│   ├── nbt/                     # NBT 存档格式
│   ├── locale/                  # 本地化
│   ├── platform/                # 跨平台抽象（输入等）
│   └── util/                    # 工具类
├── tools/
│   └── gen_color_maps.ps1       # 颜色映射生成脚本
├── hvigor/  hvigorfile.ts       # 构建工具链
├── build-profile.json5          # 签名配置、产品定义
└── oh-package.json5             # 工程级依赖（当前无外部依赖）
```

## 三、技术栈与关键实现

1. **渲染**：`main_ohos.cpp` 通过 EGL + `OH_NativeWindow` 创建 OpenGL ES 上下文，绑定 XComponent 的 surface；`Index.ets` 用 `setInterval(16ms)` 驱动 `renderFrame()` 渲染循环。
2. **触摸输入**：ArkUI 侧 `Index.ets` 捕获 `TouchEvent`，经 NAPI `renderTouch(x, y, action, id)` 转发到 `platform/input/Multitouch`。
3. **音频**：`SoundSystemOHOS.cpp` 使用 OHOS `libohaudio.so` 实现音频后端。
4. **GLES 兼容**：MCPE 使用 GLES1，通过 `gles1_dyn.cpp` 动态加载 GLES1 符号，缺失符号走 `gles1_stub.cpp`。
5. **平台适配**：`AppPlatform_ohos.cpp` 实现 `AppPlatform` 接口（文件、资源加载等），`main_ohos.cpp` 提供 NAPI 导出：`initRender` / `renderFrame` / `renderTouch` / `destroyRender`。
6. **资源**：游戏贴图/字体等静态资源打包进 `rawfile`，由 `AppPlatform_ohos` 从 rawfile 读取。

## 四、编译配置要点（CMakeLists.txt）

- 源文件通过 `file(GLOB_RECURSE ... third_party/mcpe/src/*.cpp)` 收集。
- 通过 `list(FILTER ... EXCLUDE REGEX)` 排除不用于 OHOS 的入口/后端：
  `main.cpp`、`main_android.cpp`、`main_android_java.cpp`、`main_rpi.cpp`、`main_dedicated.cpp`、`main_glfw.cpp`、`AppPlatform_win32.cpp`、`AppPlatform_glfw.cpp`、`SoundSystemSL.cpp`、`SoundSystemAL.cpp`、`PngLoader.cpp`。
- 编译宏：`-DOHOS -Wno-narrowing`。
- 链接库：`libace_napi.z.so`、`libhilog_ndk.z.so`、`libEGL.so`、`libGLESv3.so`、`libnative_window.so`、`librawfile.z.so`、`libimage_source_ndk.z.so`、`libpixelmap_ndk.z.so`、`libz.so`、`libohaudio.so`。

## 五、签名配置

签名材料位于 `C:\Users\Administrator\.ohos\config\` 下，`build-profile.json5` 中引用的 `default_mcpe-ohos_*` 系列证书：
- certpath：`default_mcpe-ohos_FQ7S3hdwMBBO1tRYJfalwPtYUg46592LLRVem0_SbGQ=.cer`
- profile：同名 `.p7b`
- storeFile：同名 `.p12`
- signAlg：SHA256withECDSA，keyAlias：debugKey

## 六、构建与部署

**构建 HAP（debug）**：
```powershell
$env:PATH = "C:\Program Files\Huawei\DevEco Studio\tools\node;C:\Program Files\Huawei\DevEco Studio\tools\ohpm\bin;C:\Program Files\Huawei\DevEco Studio\ohos\bin;C:\Program Files\Huawei\DevEco Studio\jbr\bin;" + $env:PATH
$env:JAVA_HOME = "C:\Program Files\Huawei\DevEco Studio\jbr"
$env:DEVECO_SDK_HOME = "C:\Program Files\Huawei\DevEco Studio\sdk"
& "C:\Program Files\Huawei\DevEco Studio\tools\hvigor\bin\hvigorw.bat" assembleHap --mode module -p product=default -p module=entry@default -p buildMode=debug --no-daemon
```
> 需在工程根目录（`mcpe-ohos`）下执行。

**产物路径**：`entry/build/default/outputs/default/entry-default-signed.hap`（约 30.6 MB）

**安装到真机**（hdc 不支持绝对路径，需在产物目录下用相对路径）：
```powershell
& "C:\Program Files\Huawei\DevEco Studio\sdk\default\openharmony\toolchains\hdc.exe" install -r "entry-default-signed.hap"
```

**启动**：
```powershell
& "C:\Program Files\Huawei\DevEco Studio\sdk\default\openharmony\toolchains\hdc.exe" shell "aa start -a EntryAbility -b net.minecraft.pe"
```

**当前测试设备**：`4NZ0225605001361`

## 七、当前状态

- 已成功构建并通过真机安装、启动测试。
- 应用进程 `net.minecraft.pe` 在设备上运行正常，无崩溃日志。
- 已生成完整安装包（含构建产物）。