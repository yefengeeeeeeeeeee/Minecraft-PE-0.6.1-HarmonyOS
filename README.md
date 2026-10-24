# Minecraft: Pocket Edition 0.6.1 alpha · HarmonyOS 移植

将 **Minecraft: Pocket Edition v0.6.1 alpha** 移植到 **HarmonyOS（OpenHarmony）** 

基于 MCPE 0.6.1 泄露源代码

> 本项目为学习与移植研究用途，游戏版权归 Mojang / Microsoft 所有。

## 构建

环境要求：

- DevEco Studio（HarmonyOS SDK，本机路径 `/Applications/DevEco-Studio.app`）
- Node.js（用于 hvigor 构建）

```bash
# 关键：显式指定 SDK 路径，否则会报 DEVECO_SDK_HOME 无效
export DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk

node /Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw.js \
  assembleHap --mode module -p product=default -p buildMode=debug --no-daemon
```

构建产物：`minecraftpe/build/default/outputs/default/minecraftpe-default-unsigned.hap`

## 真机运行 / 调试

使用 hdc（HarmonyOS 命令行工具）连接真机：

```bash
HDC=~/Library/OpenHarmony/Sdk/12/toolchains/hdc

# 安装
$HDC -t 127.0.0.1:5555 install -r minecraftpe/build/default/outputs/default/minecraftpe-default-unsigned.hap

# 启动（bundle 名 / ability 见 AppScope 与 module.json5）
$HDC -t 127.0.0.1:5555 shell "aa start -a EntryAbility -b net.minecraft.pe.cdlinuxit43"

# 截图（必须 .jpeg 后缀）
$HDC -t 127.0.0.1:5555 shell "snapshot_display -f /data/local/tmp/screen.jpeg"
$HDC -t 127.0.0.1:5555 file recv /data/local/tmp/screen.jpeg ./screen.jpeg

# 抓日志（hilog 对格式化参数默认脱敏为 <private>，需用 %{public}s/%{public}d）
$HDC -t 127.0.0.1:5555 shell "hilog -x | grep <TAG>"
```
