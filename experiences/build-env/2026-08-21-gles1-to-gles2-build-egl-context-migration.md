---
id: 2026-08-21-gles1-to-gles2-build-egl-context-migration
title: 鸿蒙 CMake 项目 GLES1→GLES2 迁移的构建与 EGL 上下文配置
category: build-env
nature: pitfall
created_at: 2026-08-21T19:36:11
updated_at: 2026-08-21T19:36:11
tags: [harmonyos, opengl-es, egl, cmake, build]
keywords: [鸿蒙, HarmonyOS, EGL, EGL_CONTEXT_CLIENT_VERSION, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, CMakeLists, GLES1, GLES2, hvigor, 真机部署]
problem_patterns: [EGL 上下文版本未升级导致 ES2 特性不可用, 遗留 GLES1 stub 源文件仍在构建中引入旧符号, GLES1 动态加载实现与新链接库冲突]
source_sessions: [unknown]
project: mcpe-ohos
severity: medium
quality_score: 82
hit_count: 0
last_hit_at: null
low_quality_hit_count: 0
tier: A
related_experiences: []
---

# 鸿蒙 CMake 项目 GLES1→GLES2 迁移的构建与 EGL 上下文配置

## Scenario

在 HarmonyOS（OpenHarmony）项目中，把一个按 GLES1 初始化的渲染层升级到 GLES2。代码层已经切换为 GLES2 头文件与可编程管线，但构建配置与 EGL 上下文仍停留在 GLES1，导致编译或运行期问题。

## Key Signals

- 编译期：CMake 仍把旧的 `gles1_dyn.cpp`/`gles1_stub.cpp` 编入目标，重新引入 GLES1 动态加载符号，与新的 GLES2 头/实现冲突。
- 运行期：`eglChooseConfig` 的 `EGL_RENDERABLE_TYPE` 仍为 GLES1 位，`eglCreateContext` 的 `EGL_CONTEXT_CLIENT_VERSION` 仍为 1，导致 ES2 特性不可用或渲染异常。

## Essence

GLES1→GLES2 迁移不止是改代码，还要同步改两处「环境」配置，否则代码切换无效：

1. **构建配置**：移除所有 GLES1 专用的 stub/动态加载源文件，确保目标只链接 GLES2。
2. **EGL 上下文**：把 renderable type 与 context client version 从 ES1 升级到 ES2，让运行时真正创建 ES2 上下文。

## Approach

1. 在 `CMakeLists.txt` 中删除 `gles1_dyn.cpp`、`gles1_stub.cpp`（或任何 GLES1 动态加载/stub 源文件）的引用，避免再次引入 GLES1 符号。
2. 在 EGL 初始化处（如 `entry/src/main/cpp/main_ohos.cpp`）修改 `eglChooseConfig` 的属性：`EGL_RENDERABLE_TYPE` 设为 `EGL_OPENGL_ES2_BIT`。
3. 修改 `eglCreateContext` 的属性：`EGL_CONTEXT_CLIENT_VERSION` 设为 `2`。
4. 重新构建（hvigor/HAP 构建），确认 `BUILD SUCCESSFUL`。
5. 部署到真机：安装 HAP（`entry/build/default/outputs/default/entry-default-signed.hap`），执行 `install` 与 `start`，确认 `install bundle successfully` / `start ability successfully`。

## Faster Next Time

- 迁移时先 grep 构建脚本与源文件列表中所有 `gles1` 字样，一次性清理，而不是逐个编译错误修复。
- 改完渲染层后，立即检查 EGL 初始化三件套：`EGL_RENDERABLE_TYPE`、`EGL_CONTEXT_CLIENT_VERSION`、config attribs 里的 depth/stencil，确保版本一致。
- 若真机 `aa start` 报 10106102（屏幕锁定无法启动），先确认设备是否锁屏——这是设备状态问题而非渲染/构建问题，唤醒解锁后再启动即可。

## Applicability Boundaries

- 适用：OpenHarmony/HarmonyOS 原生（ArkTS + C++）项目，使用 EGL + OpenGL ES 的渲染层升级场景。
- 适用：任何从 GLES1 上下文升级到 GLES2 的嵌入式/移动渲染项目（EGL 配置方式通用）。
- 不适用：纯代码层（非 EGL/构建）的 GLES 兼容问题，应归入 `coding-pattern` 类经验。
- 平台差异：命令基于 Windows + hvigor/真机部署；`aa`/`hdc` 等真机命令仅适用于 OpenHarmony 设备。

## Evidence or Examples

- 项目：`mcpe-ohos`（MCPE 客户端）。
- `CMakeLists.txt`：删除 `gles1_dyn.cpp`/`gles1_stub.cpp`。
- `main_ohos.cpp`：`EGL_CONTEXT_CLIENT_VERSION` 从 1 改为 2；`EGL_RENDERABLE_TYPE` 改为 `EGL_OPENGL_ES2_BIT`。
- 构建结果：`BUILD SUCCESSFUL`；HAP 位于 `entry/build/default/outputs/default/entry-default-signed.hap`。
- 部署结果：`install bundle successfully`、`start ability successfully`，应用进入游戏世界（有花屏但按需求只需不崩溃）。
- 曾遇设备锁屏导致 `aa start` 报 10106102，解锁后启动成功。