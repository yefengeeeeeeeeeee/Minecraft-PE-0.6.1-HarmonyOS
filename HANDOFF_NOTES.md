# MCPE-ohos 接手注意事项（README for Handoff）

> 本文档写给后续接手此项目的人员。动手前请先通读，避免踩坑。

## 一、环境要求（关键）

- **IDE**：DevEco Studio（本机安装于 `C:\Program Files\Huawei\DevEco Studio`）。
- **SDK**：`C:\Program Files\Huawei\DevEco Studio\sdk`，项目使用 API 26。
- **构建工具**：hvigor，位于 `C:\Program Files\Huawei\DevEco Studio\tools\hvigor\bin\hvigorw.bat`。
- **hdc 工具**：`C:\Program Files\Huawei\DevEco Studio\sdk\default\openharmony\toolchains\hdc.exe`（**不在系统 PATH 中，必须用绝对路径调用**）。
- **node / ohpm / hvigor / jbr**：均不在 PATH，构建前需手动 `$env:PATH` 拼接，并设置 `JAVA_HOME`、`DEVECO_SDK_HOME`（见 `PROJECT_SUMMARY.md` 构建命令）。

## 二、签名注意事项（易踩坑）

1. 签名材料在 `C:\Users\Administrator\.ohos\config\`，文件名为 `default_mcpe-ohos_*`（`.cer` / `.p7b` / `.p12`）。
2. `build-profile.json5` 里写死了这些证书的**绝对路径**和密码。**换机器 / 换用户 / 移动 `.ohos` 目录后，签名会失效**，需要重新生成签名或在 DevEco Studio 中自动签名。
3. 包名是 `net.minecraft.pe`，若修改包名需同步更新 `AppScope/app.json5`，并重新签名。

## 三、构建常见问题

1. **clang++ `--gcc-toolchain` unused 警告**：属正常告警，可忽略，不影响构建。
2. **`Gui.cpp` 的 `-Wformat` 警告**（`%.1f` 打印 int）：属上游源码告警，不影响运行，暂未修改。
3. 首次构建或改动 `third_party/mcpe/src` 后会触发全量 `BuildNativeWithNinja`，耗时较长属正常。
4. CMake 用 `GLOB_RECURSE` 收集源文件，**新增 `.cpp` 后需重新构建（`CONFIGURE_DEPENDS` 已启用，一般会自动重配置）**。

## 四、源码修改注意事项

1. **不要随意改动 `third_party/mcpe/src` 上游源码**，除非明确知道影响范围；平台适配逻辑应放在 `entry/src/main/cpp/` 层。
2. **CMakeLists.txt 的 EXCLUDE 列表**：OHOS 不支持的平台入口/后端已被排除。若要新增平台特定文件，需确认不会引入 `main_*.cpp` 冲突入口。
3. 渲染/输入/音频的核心适配点：
   - 渲染：`main_ohos.cpp`（EGL + OH_NativeWindow）
   - 输入：`Index.ets` → NAPI `renderTouch`
   - 音频：`SoundSystemOHOS.cpp`
   - 平台文件/资源：`AppPlatform_ohos.cpp`
4. NAPI 导出方法定义在 `main_ohos.cpp`，新增 NAPI 接口需同步更新 `types/libentry/Index.d.ts`（ArkTS 侧类型）。

## 五、部署与调试

1. **安装必须用相对路径**：先 `cd` 到产物目录，再 `hdc install -r entry-default-signed.hap`（hdc 的 install 不认绝对路径）。
2. 查看崩溃/异常：`hdc hilog -x -e`，日志 tag 主要为 `MCPE-Main`、`EglProbe`。
3. 截图：`hdc shell snapshot_display -f /data/local/tmp/x.jpeg`，再 `hdc file recv`（注意扩展名必须是 `.jpeg`）。
4. 应用横屏全屏由 `EntryAbility.ets` 的 `setFullscreen` 强制设置，若真机显示异常先查此处。

## 六、当前设备

- 测试设备序列号：`4NZ0225605001361`
- 已安装并启动验证通过（进程 `net.minecraft.pe`）。

## 七、快速上手清单

1. 读 `PROJECT_SUMMARY.md`。
2. 配置构建环境变量（见构建命令）。
3. 执行 `assembleHap` 构建。
4. `hdc install -r` 安装到设备。
5. `aa start -a EntryAbility -b net.minecraft.pe` 启动。
6. `hdc hilog -x -e` 查看日志。