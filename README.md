# Window Siphon · 窗口虹吸

让窗口在最小化时连续拉伸、收束到任务栏，还原时流畅展开。Window Siphon 是基于 Windhawk **MacOS Minimize Animation** 独立维护的动画模组。

当前版本：**3.1.6-classic.18**。

## 功能

- 连续收束的虹吸动画，默认时长 **300 ms**，窗口还原末段平滑减速，收束位置贴合任务栏外沿。
- 按对应任务栏项确定动画目标，不要求关闭按钮、最小化按钮或标题栏；保留系统捕获保护、用户排除列表和无有效截图时的原生回退。
- GPU 网格着色器完成形变，每帧更新 **96 字节**参数；圆角与柔和阴影随形变变化，可关闭动态模糊。
- 自动匹配窗口所在显示器的刷新率，按画布尺寸与刷新率自动选择 **50–100%** 渲染比例，低分辨率纹理由 GPU 平滑放大。
- 最小化先合成完整首帧再隐藏真实窗口；截图纹理可缓存到还原阶段，缓存容量有上限。
- 启动动画遇到空白或纯黑截图时进行有限重试；找不到可靠任务栏位置或无法取得有效截图时正常显示窗口。
- 截图使用 `PrintWindow`，不创建 Windows Graphics Capture 会话。
- 合并重复动画事件，反向操作和窗口状态变化会取消旧动画；空闲时没有动画渲染循环。
- 设置项汉化，保留窗口还原、排除程序、自动隐藏任务栏和实验性多显示器支持。

手动将动画分辨率设为 **100%** 可使用兼容渲染路径；GPU 不可用时也会自动回退。兼容路径不提供 GPU 网格路径的圆角与阴影增强。

## 安装

1. 安装 [Windhawk](https://windhawk.net/)。当前版本在 Windows 11、Windhawk 1.7.3 上完成基本验证。
2. 打开 [window-siphon.wh.cpp](window-siphon.wh.cpp)，复制完整源码。
3. Windhawk → 创建新模组 → 全选替换代码 → 编译并启用。

本版本使用独立模组 ID `window-siphon`。若已安装 MacOS Minimize Animation 或旧 Classic Genie，先禁用旧模组，再启用 Window Siphon；需要的自定义设置请在新模组中重新填写。

启动动画与多显示器支持默认关闭。动画使用静态截图；半透明背景与实时模糊可能无法由应用截图保留，也不会随动画更新。应用截图能力和窗口类型会影响兼容性。

## 本地编译

```powershell
.\build.ps1 -WindhawkRoot 'C:\Program Files\Windhawk'
.\build.ps1 -WindhawkRoot 'C:\Program Files\Windhawk' -Architecture i686
```

生成文件位于 `build/window-siphon-64.dll` 或 `build/window-siphon-32.dll`。脚本只编译，不安装或启用模组。

## 验证范围

classic.18 的动画实现已完成无关闭/最小化按钮的任务栏窗口、延迟绘制后首次与再次出现、Win+R，以及资源管理器重复最小化/还原的基本验证。仓库源码通过 x64 与 x86 编译。

这些检查不覆盖全部应用和窗口状态。每帧参数计算开销降低不代表整体响应时间或显示帧率已经改善，实际观感仍取决于应用截图、任务栏定位、GPU 和显示器。

## 署名与许可

原模组作者：[Abdullah Masood](https://github.com/Abdullah-Masood-05)。截图、任务栏 UI Automation 定位与自动隐藏处理包含 Potassiumuncher 的贡献。此修改版由 SinCircle 维护，保留原作者署名。

[上游源码](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/macos-minimize-animation.wh.cpp) · [MIT 许可证](LICENSE)
