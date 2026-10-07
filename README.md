# Window Siphon · 窗口虹吸

让窗口在最小化时连续拉伸、收束到任务栏，还原时流畅展开。Window Siphon 是基于 Windhawk **MacOS Minimize Animation** 独立维护的动画模组。

本项目原名 Classic Genie；更名时保留版本号 **3.1.6-classic.11**，便于追溯现有版本。

## 功能

- 连续收束的虹吸动画，默认时长 **300 ms**，窗口还原末段平滑减速。
- 收束位置贴合任务栏外沿，设置项汉化。
- 自动匹配窗口所在显示器的刷新率；小窗口保持原生分辨率，大窗口按画布尺寸与刷新率自动选择 50–100% 渲染比例。
- 低分辨率动画由 GPU 放大，也可手动设置分辨率和关闭动态模糊。
- 跳过透明分层窗口、点击穿透叠层等浮窗，减少录屏工具被错误施加动画的问题。
- 保留窗口还原、排除程序、自动隐藏任务栏和实验性多显示器支持。

## 安装

1. 安装 [Windhawk](https://windhawk.net/)。当前版本在 Windows 11、Windhawk 1.7.3 上验证。
2. 打开 [window-siphon.wh.cpp](window-siphon.wh.cpp)，复制完整源码。
3. Windhawk → 创建新模组 → 全选替换代码 → 编译并启用。

本版本使用独立模组 ID `window-siphon`。若已安装 MacOS Minimize Animation 或旧 Classic Genie，先禁用旧模组，再启用 Window Siphon；需要的自定义设置请在新模组中重新填写。

启动动画与多显示器支持默认关闭；应用截图能力和窗口类型可能影响兼容性。半透明背景使用静态截图，背景模糊不会实时更新。

## 本地编译

```powershell
.\build.ps1 -WindhawkRoot 'C:\Program Files\Windhawk'
.\build.ps1 -WindhawkRoot 'C:\Program Files\Windhawk' -Architecture i686
```

生成文件位于 `build/window-siphon-64.dll` 或 `build/window-siphon-32.dll`。脚本只编译，不安装或启用模组。

## 署名与许可

原模组作者：[Abdullah Masood](https://github.com/Abdullah-Masood-05)。截图、任务栏 UI Automation 定位与自动隐藏处理包含 Potassiumuncher 的贡献。此修改版由 SinCircle 维护，保留原作者署名。

[上游源码](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/macos-minimize-animation.wh.cpp) · [MIT 许可证](LICENSE)
