---
id: 2026-08-21-gles1-to-gles2-macro-redirect-software-matrix-stack
title: GLES1 固定管线迁移到 GLES2 的宏重定向与软件矩阵栈兼容模式
category: coding-pattern
nature: technique
created_at: 2026-08-21T19:36:11
updated_at: 2026-08-21T19:36:11
tags: [opengl-es, graphics, migration, cpp, compatibility]
keywords: [GLES1, GLES2, OpenGL ES, 固定管线, 可编程管线, 矩阵栈, glTranslatef, glRotatef, 着色器, 兼容层, 宏重定向, glVertexAttribPointer]
problem_patterns: [固定管线 API 在可编程管线中不存在, GLES1 遗留枚举在 GLES2 头文件中缺失, 宏包装导致调用真实 API 前必须先 undef]
source_sessions: [unknown]
project: mcpe-ohos
severity: high
quality_score: 86
hit_count: 0
last_hit_at: null
low_quality_hit_count: 0
tier: S
related_experiences: []
---

# GLES1 固定管线迁移到 GLES2 的宏重定向与软件矩阵栈兼容模式

## Scenario

需要把一个依赖 OpenGL ES 1.x 固定管线（fixed pipeline）的老代码库迁移到 GLES2（可编程管线），但不希望重写所有上层渲染调用。老代码大量使用 `glTranslatef`、`glRotatef`、`glScalef`、`glMatrixMode`、`glLoadIdentity` 等固定管线 API，而这些 API 在 GLES2 中已不存在。

## Key Signals

- 编译期：`glTranslatef`、`glRotatef` 等符号未声明，因为 GLES2 头文件 `<GLES2/gl2.h>` 不提供这些固定管线入口。
- 编译期：`GL_RESCALE_NORMAL`（0x803A）、`GL_NORMALIZE`（0x0BA1）等 GLES1 专属枚举在 GLES2 头文件中缺失，导致 `undeclared identifier`。
- 运行期风险：若仍创建 GLES1 上下文（`EGL_CONTEXT_CLIENT_VERSION=1`），GLES2 的可编程管线调用会失败。

## Essence

GLES1 与 GLES2 的本质差异是「固定管线」→「可编程管线」。最小改动的迁移方法是**不逐处改写上层调用**，而是建立一个兼容层：

1. 用宏把 `glTranslatef`/`glRotatef` 等固定管线符号重定向到自实现的 `mc_gl*` 函数。
2. 用软件实现的矩阵栈（`MatrixStack`，含 `modelViewStack`/`projectionStack`/`currentStack`）在 CPU 侧维护模型视图矩阵与投影矩阵，替代硬件固定管线矩阵栈。
3. 内嵌一个默认顶点/片元着色器（`Shader`），用 `glVertexAttribPointer` 替代 `glVertexPointer`，由着色器完成矩阵变换。
4. 在真正调用 GLES2 API 的实现文件里，必须**先 `#undef` 这些重定向宏**，否则真实 GL API 也会被宏替换造成无限递归或错误替换。

## Approach

1. 在公共头（如 `gles.h`）把 include 从 `GLES/gl.h` 改为 `#include <GLES2/gl2.h>`，并补丁 GLES1 遗留枚举（如 `#define GL_RESCALE_NORMAL 0x803A`、`#define GL_NORMALIZE 0x0BA1`）。
2. 声明宏重定向：`#define glTranslatef mc_glTranslatef` 等，把所有被老代码使用的固定管线符号映射到 `mc_*` 兼容函数。
3. 新增软件矩阵栈：`Vector3`/`Matrix4`（`util/Math.h`）与 `MatrixStack`（`util/MatrixStack.h/.cpp`），维护 modelView / projection 两个栈，并在 CPU 侧完成平移、旋转、缩放、矩阵模式切换等语义。
4. 新增 `Shader.h/.cpp`，内嵌默认 vertex/fragment shader，供没有自定义 shader 的路径复用。
5. 在实现 `mc_*` 函数的 `.cpp`（如 `gles.cpp`）顶部先 `#undef` 相关宏，再 include GLES2 头，才能在函数体内调用真实 `gl*` API。
6. 顶点提交从 `glVertexPointer`/固定管线绘制改为 `glVertexAttribPointer` + `glDrawArrays`/`glDrawElements`；若 `glDrawArrays` 也被宏包装，在调用前同样先 `#undef glDrawArrays`。
7. 验证：以「能正常启动不崩溃」为最小成功标准，构建通过后安装到真机并启动确认。

## Faster Next Time

- 迁移 GLES1→GLES2 时，先 grep 出所有固定管线符号（`glTranslatef|glRotatef|glScalef|glMatrixMode|glLoadIdentity|glVertexPointer|glColorPointer|glTexCoordPointer`），一次性列全重定向宏清单，避免反复补编译错误。
- 遇到「明明 include 了 GLES2 头却还报 GLES1 枚举未声明」时，先确认枚举是否属于 GLES1 专属，直接按 OpenGL ES 1.1 规范补枚举值即可。
- 任何「在兼容函数内部调用真实 GL API」的 `.cpp`，第一反应是检查宏是否污染了真实 API 名称，必要时局部 `#undef`，而不是改调用逻辑。
- 先把 EGL 上下文升到 ES2（`EGL_CONTEXT_CLIENT_VERSION=2`），否则渲染层切换了但上下文仍是 ES1，会出现难以定位的渲染异常。

## Applicability Boundaries

- 适用：需复用大量 GLES1 固定管线调用、不愿大规模重写上层的迁移场景；游戏/引擎的老渲染层移植到 GLES2。
- 适用：目标平台只提供 GLES2/GLES3，而遗留代码按 GLES1 编写。
- 不适用：需要充分利用 GLES2/3 高级特性（VBO、自定义 shader 全量重写、纹理压缩等）并追求极致性能的场景；此时应直接重写渲染层而非兼容层。
- 注意：软件矩阵栈的浮点精度/性能与硬件固定管线不完全一致，对数值精度敏感的场景需额外验证。

## Evidence or Examples

- 项目：`mcpe-ohos`（MCPE 客户端 GLES1→GLES2 迁移）。
- 头文件：`third_party/mcpe/src/client/renderer/gles.h` 改为 `#include <GLES2/gl2.h>`，并补充 `GL_RESCALE_NORMAL`(0x803A)、`GL_NORMALIZE`(0x0BA1)。
- 矩阵栈：`third_party/mcpe/src/util/Math.h`、`util/MatrixStack.h/.cpp`。
- 着色器：`third_party/mcpe/src/client/renderer/Shader.h/.cpp`。
- 实现：`gles.cpp` 顶部先 `#undef` 再调用真实 API；`Tesselator.cpp` 的 `draw()` 改用 `glVertexAttribPointer`，且先 `#undef glDrawArrays`。
- 首次构建报错即 `GL_RESCALE_NORMAL` 未声明，补枚举后 `BUILD SUCCESSFUL`；安装到真机后 `start ability successfully`，已进入游戏世界（画面存在花屏，但按需求仅需启动不崩溃即视为迁移成功）。