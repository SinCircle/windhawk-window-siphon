// ==WindhawkMod==
// @id              macos-minimize-animation
// @name            经典神灯最小化动画
// @description     Classic Genie 经典神灯动画：连续拉伸收束，展开末段缓动，贴合任务栏外沿。
// @version         3.1.6-classic.11
// @author          Abdullah Masood
// @github          https://github.com/Abdullah-Masood-05
// @include         *
// @compilerOptions -ldwmapi -lgdi32 -lole32 -loleaut32 -luuid -lshell32
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# 经典神灯最小化动画

仅保留 Classic Genie。窗口最小化时收束到任务栏外沿，还原时反向展开。

- 默认动画时长 **300 毫秒**，收起和展开起步直接推进，仅展开接近原窗口时减速收尾。
- 移动、拉伸和收窄由同一个进度连续驱动，没有原地收窄或固定上端的等待阶段。
- 窗口越贴近任务栏，底边起步收窄越柔和，避免最大化窗口突然夹紧。
- 下端在移动中逐渐收窄，短暂外凸向上传递后转为内凹；动态模糊随速度变化，可在设置中关闭。
- 收束点贴合任务栏外沿，按边界裁剪，不额外留出露出壁纸的细缝。
- 保留前中段轮廓，仅末段轻微收窄上方；额外收窄不超过原轮廓的 30%。
- 下端的入口收窄分散到更多帧，整体移动节奏保持不变。
- 设置项均已汉化；保留窗口还原、启动动画、多显示器和自动隐藏任务栏支持。
- 直接读取截图像素、按实际漏斗范围分配画布、逐行清理旧像素。
- 自动匹配窗口所在屏幕的刷新率，包括小数刷新率；每次动画重新读取。
- 自动模式按像素量与刷新率选择 50–100% 动画分辨率，也可手动指定。
- 低分辨率纹理由 GPU 平滑放大到原来的窗口范围，轮廓和任务栏边界使用物理坐标。
- 完整截图仍保留，降低的是动画纹理与画布；兼容模式可设为 100%。
- 跳过透明分层窗口、颜色键透明、点击穿透和无激活叠层，以及无标题工具浮层，避免录屏工具出现黑底动画。
- 空闲时没有动画渲染循环。
- 图标定位在后台 MTA 线程查询，缓存立即可用；查询合并且设置提供程序超时。
- 逐帧只清理旧轮廓多出的区域，避免清空随后立即重绘的像素。
- 内存中保留分段耗时计数，不记录窗口标题、路径或截图。
- 截图缓存有容量上限，还原时直接转移截图所有权，减少内存复制。

启动动画仍属实验功能，默认关闭。半透明窗口使用静态截图，实时背景模糊
不会随动画更新；应用拒绝截图时回退系统动画。多显示器与自动隐藏任务栏
保留原有选项，具体应用兼容性可能不同。

原模组作者：Abdullah Masood，MIT 许可。
截图、任务栏 UI Automation 定位及自动隐藏处理源自 Potassiumuncher 的贡献。
本地修改：精简 Classic、中文设置、任务栏边界修复与性能优化。
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- duration_ms: 300
  $name: 动画时长（毫秒）
  $description: 最小化和还原动画的持续时间，范围 50–2000，默认 300。收起和展开起步直接推进，仅展开末段减速收尾。
- render_scale_percent: 0
  $name: 动画分辨率（%，0 为自动）
  $description: 0 根据画布大小和屏幕刷新率自动选择 50–100%；也可输入 50–100。75% 表示宽高各为 75%，像素量约 56%。只降低运动画面清晰度，不改变窗口大小和动画时长。
- motion_blur: true
  $name: 启用动态模糊
  $description: 随运动速度变化的轻量模糊，起止保持清晰。关闭可降低大窗口的渲染开销。
- open_animation: true
  $name: 启用窗口还原动画
  $description: 从任务栏还原窗口时，播放反向神灯动画。
- launch_animation: false
  $name: 启用应用启动动画（实验性）
  $description: 应用窗口首次出现时播放动画；部分应用可能闪烁，或误识别启动页、对话框。
- multi_monitor: false
  $name: 多显示器支持（实验性）
  $description: 使用窗口所在显示器的任务栏作为动画目标；关闭时使用主显示器。
- unhide_taskbar: true
  $name: 动画期间显示自动隐藏的任务栏
  $description: 最小化时临时展开自动隐藏的任务栏，动画结束后恢复隐藏。
- unhide_duration_ms: 300
  $name: 任务栏保持显示时间（毫秒）
  $description: 自动隐藏任务栏临时展开的时长，范围 0–5000，默认 300。
- excluded_programs: [""]
  $name: 排除的程序
  $description: 每项填写一个程序名，例如 WindowsTerminal.exe。按窗口所属程序匹配，无论动画从哪个进程触发。
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <dwmapi.h>
#include <math.h>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>
#include <cwctype>
#include <uiautomation.h>
#include <shellapi.h>

#ifndef DWMWA_EXTENDED_FRAME_BOUNDS
#define DWMWA_EXTENDED_FRAME_BOUNDS 9
#endif

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 2
#endif

// High-resolution waitable timer flag (Win10 1803+). The kernel rejects the
// flag on 1709 and earlier (and Win7/8.1), so CreateWaitableTimerExW is retried
// without it; the pacer then just ticks coarser. A plain Sleep(1) floor keeps
// the loop from ever degrading to a busy spin.
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

typedef LRESULT (WINAPI *DefWindowProcW_t)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
DefWindowProcW_t DefWindowProcW_Original;

typedef BOOL (WINAPI *ShowWindow_t)(HWND hWnd, int nCmdShow);
ShowWindow_t ShowWindow_Original;

typedef BOOL (WINAPI *ShowWindowAsync_t)(HWND hWnd, int nCmdShow);
ShowWindowAsync_t ShowWindowAsync_Original;

typedef BOOL (WINAPI *SetWindowPlacement_t)(HWND hWnd, const WINDOWPLACEMENT* lpwndpl);
SetWindowPlacement_t SetWindowPlacement_Original;

typedef BOOL (WINAPI *CloseWindow_t)(HWND hWnd);
CloseWindow_t CloseWindow_Original;

typedef BOOL (WINAPI *SetWindowPos_t)(HWND hWnd, HWND hWndInsertAfter, int X, int Y,
                                      int cx, int cy, UINT uFlags);
SetWindowPos_t SetWindowPos_Original;

struct MacGenieAnimData {
    HWND hRealWnd;
    HBITMAP hBitmap;      // 32-bit top-down premultiplied DIB snapshot (w x h)
    void* pBits;          // its bits (fed straight to D2D CreateBitmap)
    RECT targetRect;      // the window's DWM extended-frame rect when anim started
    HMONITOR hMon;        // monitor the genie targets (window's when multi-monitor
                          // support is on, else the primary)
    int width;
    int height;
    int targetDockX;      // taskbar button X located via UI Automation
    BOOL isRising;        // FALSE = minimize (flow in), TRUE = restore (flow out)
    LONG_PTR originalExStyle;
    BOOL hiddenByCloak;   // rising only: TRUE = hidden via DWM cloak (restore path),
                          // FALSE = via WS_EX_LAYERED + alpha 0 (launch path)
    HANDLE hFirstFrameShown; // falling only: event signaled once the ghost's first
                             // frame is composed; NULL when unused
    int durationMs;
    // Auto-hide "unhide" (deferred minimize) - Potassiumuncher's engine. Only used
    // on the DefWindowProcW SC_MINIMIZE path when the taskbar is set to auto-hide.
    BOOL requestedUnhide;
    HWND hNextApp;
    int  unhideDurationMs;
    BOOL deferredMinimize;
};

// Handed to MacGenieLaunchThread: the launch worker needs the window's TRUE
// pre-hide extended style (captured at the hook site, before WS_EX_LAYERED was
// added) so the layered bit can be removed again after the reveal.
struct MacGenieLaunchData {
    HWND     hWnd;
    LONG_PTR originalExStyle;
};

// Cached snapshot for the restore animation. The DIB section owns pBits, so
// DeleteObject(hBmp) frees both. Snapshot model from Potassiumuncher's engine.
struct SnapCache { HBITMAP hBmp; void* pBits; int w; int h; ULONGLONG stamp; };

// --- THE VAULTS ---
std::unordered_map<HWND, SnapCache> g_SnapshotCache;
struct IconPosition { int x; DWORD pid; HMONITOR monitor; ULONGLONG stamp; };
std::unordered_map<HWND, IconPosition> g_IconPositions;            // per-window learned icon X
std::unordered_map<std::wstring, int> g_ProcessIconPositions; // per-process fallback
std::unordered_set<HWND> g_LaunchSeen;   // windows we've already shown/animated once
std::unordered_set<HWND> g_AnimActive;   // windows with a genie currently in flight
std::mutex g_CacheMutex;

// --- SETTINGS ---
std::atomic<int> g_durationMs{300};
std::atomic<bool> g_openAnimation{true};
std::atomic<bool> g_motionBlur{true};
std::atomic<int> g_renderScalePercent{0};
std::atomic<bool> g_launchAnimation{false};
std::atomic<bool> g_multiMonitor{false};
std::atomic<int>  g_unhideDurationMs{300};
std::atomic<bool> g_unhideEnabled{true};
// Lowercased entries of the "Excluded programs" setting. Guarded by g_CacheMutex
// (written only from LoadSettings, read once per animation start).
std::vector<std::wstring> g_ExcludedPrograms;

// --- UNLOAD COORDINATION ---
// Windhawk unmaps the mod DLL right after uninit, so any worker thread still
// running mod code at that point would crash its host process. Workers register
// here and abort promptly once g_unloading is set; Wh_ModBeforeUninit waits for
// the count to drain before the DLL goes away. (MINE's drain model is kept in
// preference to HIS's join-and-pump StopAndJoinAllThreads: pumping messages while
// unloading can re-enter our six hooks under @include *.)
std::atomic<bool> g_unloading{false};
std::atomic<int>  g_workerCount{0};

void MacGenieLoadSettings() {
    int ms = Wh_GetIntSetting(L"duration_ms");
    if (ms < 50) ms = 50;
    if (ms > 2000) ms = 2000;
    g_durationMs.store(ms, std::memory_order_relaxed);
    int scale=Wh_GetIntSetting(L"render_scale_percent");
    g_renderScalePercent.store(scale==0?0:std::clamp(scale,50,100),std::memory_order_relaxed);
    g_motionBlur.store(Wh_GetIntSetting(L"motion_blur") != 0, std::memory_order_relaxed);
    g_openAnimation.store(Wh_GetIntSetting(L"open_animation") != 0, std::memory_order_relaxed);
    g_launchAnimation.store(Wh_GetIntSetting(L"launch_animation") != 0, std::memory_order_relaxed);
    g_multiMonitor.store(Wh_GetIntSetting(L"multi_monitor") != 0, std::memory_order_relaxed);

    int unhide_ms = Wh_GetIntSetting(L"unhide_duration_ms");
    if (unhide_ms < 0) unhide_ms = 0;
    if (unhide_ms > 5000) unhide_ms = 5000;
    g_unhideDurationMs.store(unhide_ms, std::memory_order_relaxed);
    g_unhideEnabled.store(Wh_GetIntSetting(L"unhide_taskbar") != 0, std::memory_order_relaxed);

    // "Excluded programs" list: lowercase each entry once here so the per-anim
    // check is a straight comparison.
    std::vector<std::wstring> excluded;
    for (int i = 0;; i++) {
        PCWSTR prog = Wh_GetStringSetting(L"excluded_programs[%d]", i);
        bool has = *prog;   // L"" on unset, so *prog alone signals presence
        if (has) {
            std::wstring s = prog;
            std::transform(s.begin(), s.end(), s.begin(), ::towlower);
            excluded.push_back(std::move(s));
        }
        Wh_FreeStringSetting(prog);
        if (!has) break;
    }
    {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        g_ExcludedPrograms.swap(excluded);
    }
}

// TRUE if the process that OWNS hWnd is on the "Excluded programs" list. This is
// checked at the single animation choke point (StartMacGenieAnim), NOT via
// Windhawk's own exclusion list: that list only prevents the mod from being
// LOADED into a process, but this mod's hooks animate a window from whichever
// process manipulates it (Explorer's taskbar click, the shell, the app itself),
// so an app excluded there would still animate. Matching the window's owning
// process here respects the exclusion no matter where the trigger came from.
bool MacGenieIsExcluded(HWND hWnd) {
    {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        if (g_ExcludedPrograms.empty()) return false;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(hWnd, &pid);
    if (!pid) return false;
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return false;
    WCHAR path[MAX_PATH];
    DWORD len = MAX_PATH;
    bool ok = QueryFullProcessImageNameW(hProc, 0, path, &len) != 0;
    CloseHandle(hProc);
    if (!ok) return false;

    std::wstring full = path;
    std::transform(full.begin(), full.end(), full.begin(), ::towlower);
    size_t slash = full.find_last_of(L"\\/");
    std::wstring base = (slash == std::wstring::npos) ? full : full.substr(slash + 1);
    std::wstring baseNoExt = base;
    if (baseNoExt.size() > 4 && baseNoExt.compare(baseNoExt.size() - 4, 4, L".exe") == 0) {
        baseNoExt.resize(baseNoExt.size() - 4);
    }

    std::lock_guard<std::mutex> lock(g_CacheMutex);
    for (const std::wstring& e : g_ExcludedPrograms) {
        if (e == full || e == base || e == baseNoExt) return true;
    }
    return false;
}

void MacGenieSetDwmTransitions(HWND hWnd, BOOL enable) {
    BOOL disable = !enable;
    DwmSetWindowAttribute(hWnd, DWMWA_TRANSITIONS_FORCEDISABLED, &disable, sizeof(disable));
}

// Hide / show a window at the DWM level. A cloaked window is simply not rendered
// (frame included) while staying "visible" to Win32 and painting normally
// underneath, so a restore can happen invisibly under the genie with no
// layered-surface rebuild flash. Kept from MINE in preference to HIS's
// WS_EX_LAYERED + alpha 0 restore hide, which flashes the bare frame.
void MacGenieSetCloak(HWND hWnd, BOOL cloak) {
    DwmSetWindowAttribute(hWnd, DWMWA_CLOAK, &cloak, sizeof(cloak));
}

// Undo a rising caller's pre-animation hide (used whenever the animation can't run
// or is refused): uncloak or un-hide depending on how the window was hidden, and
// re-enable its DWM transitions.
static void MacGenieUndoRisingHide(HWND hWnd, LONG_PTR originalExStyle, BOOL cloakHidden) {
    if (cloakHidden) {
        MacGenieSetCloak(hWnd, FALSE);
    } else {
        SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA);
        if (!(originalExStyle & WS_EX_LAYERED)) {
            SetWindowLongPtrW(hWnd, GWL_EXSTYLE, originalExStyle);
        }
    }
    MacGenieSetDwmTransitions(hWnd, TRUE);
}

// Transparent overlays cannot be represented by an opaque PrintWindow snapshot.
// Inspect the original styles before any animation-owned hide is applied.
static bool MacGenieIsTransparentOverlay(HWND window,LONG_PTR exStyle,bool ownAlphaHide=false) {
    if(exStyle&(WS_EX_TRANSPARENT|WS_EX_NOACTIVATE))return true;
    const LONG_PTR style=GetWindowLongPtrW(window,GWL_STYLE);
    if((exStyle&WS_EX_TOOLWINDOW)&&!(style&WS_CAPTION))return true;
    if(exStyle&WS_EX_LAYERED){
        COLORREF color=0;BYTE alpha=255;DWORD flags=0;
        // Per-pixel layered surfaces do not expose constant-alpha attributes.
        if(!GetLayeredWindowAttributes(window,&color,&alpha,&flags))return true;
        if(flags&LWA_COLORKEY)return true;
        if(!ownAlphaHide&&(flags&LWA_ALPHA)&&alpha<255)return true;
    }
    return false;
}

// Should we animate this window at all? Skip child / tiny windows so the effect
// only fires on real top-level windows. The size check stays on the placement /
// window rect; the animation geometry (below) uses the DWM extended frame bounds.
static bool MacGenieShouldAnimate(HWND hWnd) {
    if (!hWnd) return false;
    LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_STYLE);
    if (style & WS_CHILD) return false;
    if(MacGenieIsTransparentOverlay(hWnd,GetWindowLongPtrW(hWnd,GWL_EXSTYLE)))return false;

    RECT r;
    if (IsIconic(hWnd)) {
        WINDOWPLACEMENT wp;
        wp.length = sizeof(wp);
        if (!GetWindowPlacement(hWnd, &wp)) return false;
        r = wp.rcNormalPosition;
    } else if (!GetWindowRect(hWnd, &r)) {
        return false;
    }
    if ((r.right - r.left) < 40 || (r.bottom - r.top) < 40) return false;
    return true;
}

// A real, top-level window with a title bar (skips child windows, tool windows,
// and borderless popups / menus / tooltips / splash screens).
static bool MacGenieIsLaunchWindow(HWND hWnd) {
    if (!hWnd) return false;
    if (GetAncestor(hWnd, GA_ROOT) != hWnd) return false;
    LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_STYLE);
    if (!(style & WS_CAPTION)) return false;
    LONG_PTR exStyle = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return false;
    return MacGenieShouldAnimate(hWnd);
}

HWND FindTaskbarForMonitor(HMONITOR hMon) {
    HWND hMainTray = FindWindowW(L"Shell_TrayWnd", NULL);
    HMONITOR mainMon = MonitorFromWindow(hMainTray, MONITOR_DEFAULTTOPRIMARY);
    if (hMon == mainMon || !hMon) return hMainTray;

    HWND hSecTray = NULL;
    while ((hSecTray = FindWindowExW(NULL, hSecTray, L"Shell_SecondaryTrayWnd", NULL)) != NULL) {
        if (MonitorFromWindow(hSecTray, MONITOR_DEFAULTTONULL) == hMon) {
            return hSecTray;
        }
    }
    return nullptr;
}

// Ported from Potassiumuncher's genie engine (github.com/Potassiumuncher).
// Locates the app's taskbar button via UI Automation and returns its center X
// (falls back to fallbackX, then to the per-window / per-process learned cache).
// Runs on the hook (app UI) thread: CoInitializeEx(APARTMENTTHREADED) matches the
// typical GUI-thread apartment (S_FALSE) or reports RPC_E_CHANGED_MODE on an MTA
// thread; either way CoUninitialize is balanced only when we actually initialized.
// Numeric, in-memory diagnostics only: no window titles, paths or screen pixels.
struct alignas(8) GeniePerfStage { volatile LONG64 count, totalUs, maxUs; };
struct alignas(8) GeniePerfData {
    GeniePerfStage stages[6];
    volatile LONG64 cacheHits,cacheMisses,uiaMatches,uiaMisses;
};
// Keep MinGW's auto-export behavior for Windhawk's lifecycle entry points.
extern "C" { GeniePerfData g_GeniePerf{}; }
static LONGLONG GenieClock() { LARGE_INTEGER v; QueryPerformanceCounter(&v); return v.QuadPart; }
static void GenieRecord(unsigned stage, LONGLONG start) {
    static const LONGLONG frequency = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f.QuadPart; }();
    LONG64 us = (GenieClock()-start)*1000000/frequency;
    auto& s = g_GeniePerf.stages[stage];
    InterlockedIncrement64(&s.count); InterlockedAdd64(&s.totalUs, us);
    LONG64 old = InterlockedCompareExchange64(&s.maxUs,0,0);
    while (us > old) { LONG64 seen=InterlockedCompareExchange64(&s.maxUs,us,old); if(seen==old)break;old=seen; }
}
struct GenieTiming { unsigned stage; LONGLONG start=GenieClock(); ~GenieTiming(){GenieRecord(stage,start);} };

struct TargetRequest { HWND window; DWORD pid; HMONITOR monitor; int fallback; std::wstring title; };
std::vector<TargetRequest> g_TargetQueue;
struct TargetStamp { DWORD pid; HMONITOR monitor; ULONGLONG time; };
std::unordered_map<HWND,TargetStamp> g_TargetRequested;
std::unordered_set<HWND> g_TargetPending;
bool g_TargetWorkerActive=false; // protected by g_CacheMutex
#ifdef WH_EDITING
int (*g_TestTargetProvider)(HWND,int,HMONITOR)=nullptr;
#endif
static int QueryTaskbarButtonX(HWND, int, HMONITOR,const wchar_t* suppliedTitle=nullptr);
static DWORD WINAPI TargetWorker(void*) {
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    for (;;) {
        TargetRequest request{};
        {
            std::lock_guard<std::mutex> lock(g_CacheMutex);
            if (g_unloading.load() || g_TargetQueue.empty()) {
                g_TargetQueue.clear();g_TargetPending.clear();g_TargetWorkerActive=false;
                break;
            }
            request=g_TargetQueue.front();g_TargetQueue.erase(g_TargetQueue.begin());
        }
        DWORD pid=0;GetWindowThreadProcessId(request.window,&pid);
        if(pid==request.pid) {
            GenieTiming timing{1};
#ifdef WH_EDITING
            if(g_TestTargetProvider)g_TestTargetProvider(request.window,request.fallback,request.monitor);
            else
#endif
            QueryTaskbarButtonX(request.window,request.fallback,request.monitor,request.title.c_str());
        }
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        g_TargetPending.erase(request.window);
    }
    g_workerCount.fetch_sub(1,std::memory_order_release);
    return 0;
}

// Never wait for an accessibility provider on a window or animation thread.
static int CachedTaskbarButtonX(HWND window,int fallback,HMONITOR monitor) {
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    std::lock_guard<std::mutex> lock(g_CacheMutex);
    auto found=g_IconPositions.find(window);
    return found!=g_IconPositions.end() && found->second.pid==pid && found->second.monitor==monitor
        ? found->second.x : fallback;
}
int GetTaskbarButtonX(HWND window,int fallback,HMONITOR monitor) {
    GenieTiming timing{0};
    DWORD pid=0;DWORD ownerThread=GetWindowThreadProcessId(window,&pid);
    if(!pid)return fallback;
    // A same-thread title read or a foreign-process cached caption needs no
    // cross-thread UI wait. Carry it into MTA so capture cannot block WM_GETTEXT.
    wchar_t title[512]{};
    if(ownerThread==GetCurrentThreadId() || pid!=GetCurrentProcessId())GetWindowTextW(window,title,512);
    const ULONGLONG now=GetTickCount64();
    std::lock_guard<std::mutex> lock(g_CacheMutex);
    auto found=g_IconPositions.find(window);
    if(found!=g_IconPositions.end() && found->second.pid==pid && found->second.monitor==monitor) {
        fallback=found->second.x;InterlockedIncrement64(&g_GeniePerf.cacheHits);
    } else InterlockedIncrement64(&g_GeniePerf.cacheMisses);
    auto requested=g_TargetRequested.find(window);
    if(g_unloading.load() || g_TargetPending.count(window) ||
       (requested!=g_TargetRequested.end() && requested->second.pid==pid &&
        requested->second.monitor==monitor && now-requested->second.time<1000) || g_TargetQueue.size()>=16)return fallback;
    if(g_TargetRequested.size()>=128)g_TargetRequested.clear();
    g_TargetRequested[window]={pid,monitor,now};g_TargetPending.insert(window);
    g_TargetQueue.push_back({window,pid,monitor,fallback,title});
    if(!g_TargetWorkerActive) {
        g_TargetWorkerActive=true;g_workerCount.fetch_add(1,std::memory_order_relaxed);
        HANDLE thread=CreateThread(nullptr,0,TargetWorker,nullptr,0,nullptr);
        if(thread)CloseHandle(thread);
        else {g_TargetWorkerActive=false;g_TargetQueue.clear();g_TargetPending.clear();g_workerCount.fetch_sub(1);}
    }
    return fallback;
}
static void WarmTaskbarTarget(HWND window) {
    if(g_unloading.load() || !MacGenieShouldAnimate(window))return;
    HMONITOR monitor=g_multiMonitor.load() ? MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST)
        : MonitorFromPoint(POINT{},MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{sizeof(mi)};
    if(GetMonitorInfoW(monitor,&mi))GetTaskbarButtonX(window,(mi.rcMonitor.left+mi.rcMonitor.right)/2,monitor);
}

static int QueryTaskbarButtonX(HWND hWndApp, int fallbackX, HMONITOR hMon,const wchar_t* suppliedTitle) {
    int targetX = fallbackX;
    bool uiaFound = false;

    std::wstring procNameLower = L"";
    DWORD ownerPid = 0;
    GetWindowThreadProcessId(hWndApp, &ownerPid);
    if (ownerPid) {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, ownerPid);
        if (hProc) {
            WCHAR exePath[MAX_PATH] = {0};
            DWORD exePathLen = MAX_PATH;
            if (QueryFullProcessImageNameW(hProc, 0, exePath, &exePathLen)) {
                WCHAR* name = wcsrchr(exePath, L'\\');
                if (name) {
                    procNameLower = (name + 1);
                    size_t dotPos = procNameLower.find(L'.');
                    if (dotPos != std::wstring::npos) procNameLower = procNameLower.substr(0, dotPos);
                    std::transform(procNameLower.begin(), procNameLower.end(), procNameLower.begin(), ::towlower);
                }
            }
            CloseHandle(hProc);
        }
    }

    std::wstring processKey = procNameLower;
    if (!processKey.empty() && hMon) {
        processKey += L"_" + std::to_wstring(reinterpret_cast<size_t>(hMon));
    }

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool coInit = (hr == S_OK || hr == S_FALSE);

    if (hr == S_OK || hr == S_FALSE || hr == RPC_E_CHANGED_MODE) {
        IUIAutomation* pAutomation = nullptr;
        HRESULT hrUia = CoCreateInstance(__uuidof(CUIAutomation8), NULL, CLSCTX_INPROC_SERVER, __uuidof(IUIAutomation), (void**)&pAutomation);
        if (FAILED(hrUia)) {
            hrUia = CoCreateInstance(__uuidof(CUIAutomation), NULL, CLSCTX_INPROC_SERVER, __uuidof(IUIAutomation), (void**)&pAutomation);
        }

        // Bound provider waits; only the windowless MTA worker calls UIA.
        if (SUCCEEDED(hrUia) && pAutomation) {
            IUIAutomation2* bounded=nullptr;
            if (FAILED(pAutomation->QueryInterface(__uuidof(IUIAutomation2),(void**)&bounded))) {
                pAutomation->Release();pAutomation=nullptr;
            } else {
                HRESULT a=bounded->put_ConnectionTimeout(250),b=bounded->put_TransactionTimeout(500);
                bounded->Release();
                if(FAILED(a)||FAILED(b)){pAutomation->Release();pAutomation=nullptr;}
            }
        }
        if (pAutomation) {
            HWND hTray = FindTaskbarForMonitor(hMon);
            if (hTray) {
                IUIAutomationElement* pTrayElement = nullptr;
                if (SUCCEEDED(pAutomation->ElementFromHandle(hTray, &pTrayElement)) && pTrayElement) {

                    WCHAR titleW[512] = {0};
                    DWORD_PTR textResult=0;
                    if(suppliedTitle && *suppliedTitle)lstrcpynW(titleW,suppliedTitle,512);
                    else SendMessageTimeoutW(hWndApp,WM_GETTEXT,512,(LPARAM)titleW,
                        SMTO_ABORTIFHUNG|SMTO_BLOCK,40,&textResult);
                    std::wstring titleLower = titleW;
                    std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(), ::towlower);

                    std::wstring procHintLower = procNameLower;
                    if (procNameLower == L"chrome") procHintLower = L"google chrome";
                    else if (procNameLower == L"msedge") procHintLower = L"microsoft edge";
                    else if (procNameLower == L"firefox") procHintLower = L"firefox";
                    else if (procNameLower == L"brave") procHintLower = L"brave";
                    else if (procNameLower == L"opera") procHintLower = L"opera";
                    else if (procNameLower == L"vivaldi") procHintLower = L"vivaldi";

                    IUIAutomationCondition* pButtonCond = nullptr;
                    IUIAutomationCondition* pListItemCond = nullptr;
                    IUIAutomationCondition* pOrCond = nullptr;

                    VARIANT varBtn; varBtn.vt = VT_I4; varBtn.lVal = UIA_ButtonControlTypeId;
                    pAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, varBtn, &pButtonCond);

                    VARIANT varList; varList.vt = VT_I4; varList.lVal = UIA_ListItemControlTypeId;
                    pAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, varList, &pListItemCond);

                    if (pButtonCond && pListItemCond) {
                        pAutomation->CreateOrCondition(pButtonCond, pListItemCond, &pOrCond);
                    }

                    // Fetch names and bounds in one provider request, rather than
                    // performing cross-process calls for every button property.
                    IUIAutomationCacheRequest* request = nullptr;
                    bool cached = SUCCEEDED(pAutomation->CreateCacheRequest(&request)) && request;
                    if (cached) {
                        cached = SUCCEEDED(request->AddProperty(UIA_NamePropertyId)) &&
                                 SUCCEEDED(request->AddProperty(UIA_BoundingRectanglePropertyId)) &&
                                 SUCCEEDED(request->put_TreeScope(TreeScope_Element)) &&
                                 SUCCEEDED(request->put_AutomationElementMode(AutomationElementMode_None));
                    }
                    IUIAutomationElementArray* pArray = nullptr;
                    HRESULT found = E_FAIL;
                    if (pOrCond && cached)
                        found = pTrayElement->FindAllBuildCache(TreeScope_Descendants, pOrCond, request, &pArray);
                    // Failed/slow providers keep the last known target. Do not issue
                    // another traversal or per-element cross-thread property calls.
                    if (request) request->Release();
                    if (SUCCEEDED(found) && pArray) {
                        int length = 0;
                        pArray->get_Length(&length);

                        MONITORINFO mi = {0};
                        mi.cbSize = sizeof(MONITORINFO);
                        GetMonitorInfoW(hMon, &mi);
                        int monRight = mi.rcMonitor.right;

                        int bestScore = 0;

                        for (int i = 0; i < length; i++) {
                            IUIAutomationElement* pItem = nullptr;
                            if (SUCCEEDED(pArray->GetElement(i, &pItem)) && pItem) {
                                BSTR name = nullptr;
                                if (SUCCEEDED(cached ? pItem->get_CachedName(&name) : pItem->get_CurrentName(&name)) && name) {
                                    std::wstring uiaNameLower = name;
                                    std::transform(uiaNameLower.begin(), uiaNameLower.end(), uiaNameLower.begin(), ::towlower);

                                    if (!uiaNameLower.empty()) {
                                        int score = 0;

                                        if (titleLower == uiaNameLower) score += 1000;
                                        if (!titleLower.empty() && titleLower.find(uiaNameLower) != std::wstring::npos) score += 500;
                                        if (!titleLower.empty() && uiaNameLower.find(titleLower) != std::wstring::npos) score += 500;

                                        if (!procNameLower.empty() && uiaNameLower.find(procNameLower) != std::wstring::npos) score += 400;
                                        if (!procHintLower.empty() && procHintLower != procNameLower &&
                                            uiaNameLower.find(procHintLower) != std::wstring::npos) score += 900;

                                        std::wstring currentWord;
                                        for (wchar_t c : titleLower) {
                                            if (iswalnum(c)) {
                                                currentWord += c;
                                            } else {
                                                if (currentWord.length() >= 4 && uiaNameLower.find(currentWord) != std::wstring::npos) score += 50;
                                                currentWord.clear();
                                            }
                                        }
                                        if (currentWord.length() >= 4 && uiaNameLower.find(currentWord) != std::wstring::npos) score += 50;

                                        if (uiaNameLower.find(L"start") != std::wstring::npos) score -= 500;
                                        if (uiaNameLower.find(L"search") != std::wstring::npos) score -= 500;
                                        if (uiaNameLower.find(L"task view") != std::wstring::npos) score -= 500;
                                        if (uiaNameLower.find(L"widgets") != std::wstring::npos) score -= 500;

                                        if (score > bestScore) {
                                            RECT bRect;
                                            if (SUCCEEDED((cached ? pItem->get_CachedBoundingRectangle(&bRect) : pItem->get_CurrentBoundingRectangle(&bRect)))) {
                                                if (bRect.right > bRect.left && bRect.left < monRight - 50) {
                                                    bestScore = score;
                                                    targetX = bRect.left + (bRect.right - bRect.left) / 2;
                                                    uiaFound = true;
                                                }
                                            }
                                        }
                                    }
                                    SysFreeString(name);
                                }
                                pItem->Release();
                            }
                        }
                        pArray->Release();
                    }
                    if (pButtonCond) pButtonCond->Release();
                    if (pListItemCond) pListItemCond->Release();
                    if (pOrCond) pOrCond->Release();
                    pTrayElement->Release();
                }
            }
            pAutomation->Release();
        }
        if (coInit) CoUninitialize();
    }

    std::lock_guard<std::mutex> lock(g_CacheMutex);
    DWORD stillPid=0;GetWindowThreadProcessId(hWndApp,&stillPid);
    InterlockedIncrement64(uiaFound ? &g_GeniePerf.uiaMatches : &g_GeniePerf.uiaMisses);
    if (uiaFound && stillPid==ownerPid && !g_unloading.load()) {
        if(g_IconPositions.size()>=128 && !g_IconPositions.count(hWndApp)) {
            auto oldest=std::min_element(g_IconPositions.begin(),g_IconPositions.end(),
                [](const auto& a,const auto& b){return a.second.stamp<b.second.stamp;});
            g_IconPositions.erase(oldest);
        }
        g_IconPositions[hWndApp] = {targetX,ownerPid,hMon,GetTickCount64()};
        if (!processKey.empty()) {
            g_ProcessIconPositions[processKey] = targetX;
        }
    } else if (g_IconPositions.count(hWndApp)) {
        targetX = g_IconPositions[hWndApp].x;
    } else if (!processKey.empty() && g_ProcessIconPositions.count(processKey)) {
        targetX = g_ProcessIconPositions[processKey];
    }

    return targetX;
}

// Ported from Potassiumuncher's genie engine (github.com/Potassiumuncher).
// The macOS "lamp" curve: maps a mesh vertex (tx,ty in 0..1) at progress p to its
// warped screen position, given the window rect w and the taskbar icon rect i.
// Hardware compositor scales the small premultiplied bitmap to exact physical
// window bounds. No full-size CPU upsample or full-size layered-window upload.
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <memory>
template<class T> static void GenieRelease(T*& p){if(p){p->Release();p=nullptr;}}
static std::mutex g_GpuMutex;
struct GenieGpuDevice {
    ID3D11Device* device=nullptr;
    ID3D11DeviceContext* context=nullptr;
    DXGI_ADAPTER_DESC1 description{};
    std::mutex mutex;
    ~GenieGpuDevice(){
        if(context){context->ClearState();context->Flush();}
        GenieRelease(context);GenieRelease(device);
    }
};
static std::vector<std::unique_ptr<GenieGpuDevice>> g_GpuDevices;
static HMODULE g_D3DModule=nullptr,g_DCompModule=nullptr,g_DxgiModule=nullptr;
static void ResetGenieGpu() {
    std::lock_guard<std::mutex> lock(g_GpuMutex);
    g_GpuDevices.clear(); // all workers and compositor objects have already drained
    if(g_DCompModule){FreeLibrary(g_DCompModule);g_DCompModule=nullptr;}
    if(g_D3DModule){FreeLibrary(g_D3DModule);g_D3DModule=nullptr;}
    if(g_DxgiModule){FreeLibrary(g_DxgiModule);g_DxgiModule=nullptr;}
}
static GenieGpuDevice* PrepareGenieGpu(HWND window) { // caller holds g_GpuMutex
    if(!g_D3DModule)g_D3DModule=LoadLibraryExW(L"d3d11.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!g_DCompModule)g_DCompModule=LoadLibraryExW(L"dcomp.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!g_DxgiModule)g_DxgiModule=LoadLibraryExW(L"dxgi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!g_D3DModule||!g_DCompModule||!g_DxgiModule)return nullptr;
    auto create=reinterpret_cast<decltype(&D3D11CreateDevice)>(GetProcAddress(g_D3DModule,"D3D11CreateDevice"));
    auto makeFactory=reinterpret_cast<decltype(&CreateDXGIFactory1)>(GetProcAddress(g_DxgiModule,"CreateDXGIFactory1"));
    if(!create||!makeFactory)return nullptr;
    IDXGIFactory1* factory=nullptr;
    if(FAILED(makeFactory(__uuidof(IDXGIFactory1),(void**)&factory)))return nullptr;
    IDXGIAdapter1* selected=nullptr;
    HMONITOR monitor=MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST);
    for(UINT i=0;!selected;++i){
        IDXGIAdapter1* adapter=nullptr;
        if(factory->EnumAdapters1(i,&adapter)!=S_OK)break;
        for(UINT j=0;;++j){
            IDXGIOutput* output=nullptr;
            if(adapter->EnumOutputs(j,&output)!=S_OK)break;
            DXGI_OUTPUT_DESC desc{};HRESULT hr=output->GetDesc(&desc);output->Release();
            if(SUCCEEDED(hr)&&desc.Monitor==monitor){selected=adapter;break;}
        }
        if(!selected)adapter->Release();
    }
    if(!selected)factory->EnumAdapters1(0,&selected);
    factory->Release();
    if(!selected)return nullptr;
    DXGI_ADAPTER_DESC1 desc{};
    if(FAILED(selected->GetDesc1(&desc))){selected->Release();return nullptr;}
    for(auto& cached:g_GpuDevices){
        if(cached->description.AdapterLuid.HighPart==desc.AdapterLuid.HighPart&&
           cached->description.AdapterLuid.LowPart==desc.AdapterLuid.LowPart){
            selected->Release();
            return SUCCEEDED(cached->device->GetDeviceRemovedReason())?cached.get():nullptr;
        }
    }
    auto state=std::make_unique<GenieGpuDevice>();state->description=desc;
    D3D_FEATURE_LEVEL actual;
    D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0};
    HRESULT hr=create(selected,D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        levels,3,D3D11_SDK_VERSION,&state->device,&actual,&state->context);
    selected->Release();
    if(FAILED(hr))return nullptr;
    GenieGpuDevice* result=state.get();g_GpuDevices.push_back(std::move(state));return result;
}
class GenieGpuCanvas {
    GenieGpuDevice* state=nullptr;
    IDXGISwapChain1* swap=nullptr;
    ID3D11Texture2D* back=nullptr;
    IDCompositionDevice* composition=nullptr;
    IDCompositionTarget* target=nullptr;
    IDCompositionVisual* visual=nullptr;
public:
    const wchar_t* AdapterName()const{return state?state->description.Description:L"none";}
    LUID AdapterLuid()const{return state?state->description.AdapterLuid:LUID{};}
    ~GenieGpuCanvas(){Close();}
    void Close(){
        if(target)target->SetRoot(nullptr);
        if(composition)composition->Commit();
        GenieRelease(visual);GenieRelease(target);GenieRelease(composition);GenieRelease(back);GenieRelease(swap);
        state=nullptr;
    }
    bool Open(HWND window,int renderW,int renderH,int displayW,int displayH){
        std::lock_guard<std::mutex> lock(g_GpuMutex);
        state=PrepareGenieGpu(window);
        if(!state)return false;
        IDXGIDevice* dxgi=nullptr;IDXGIAdapter* adapter=nullptr;IDXGIFactory2* factory=nullptr;
        HRESULT hr=state->device->QueryInterface(__uuidof(IDXGIDevice),(void**)&dxgi);
        if(SUCCEEDED(hr))hr=dxgi->GetAdapter(&adapter);
        if(SUCCEEDED(hr))hr=adapter->GetParent(__uuidof(IDXGIFactory2),(void**)&factory);
        DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=renderW;desc.Height=renderH;
        desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;
        desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;
        desc.Scaling=DXGI_SCALING_STRETCH;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;
        if(SUCCEEDED(hr))hr=factory->CreateSwapChainForComposition(state->device,&desc,nullptr,&swap);
        auto create=reinterpret_cast<decltype(&DCompositionCreateDevice)>(GetProcAddress(g_DCompModule,"DCompositionCreateDevice"));
        if(SUCCEEDED(hr))hr=create?create(dxgi,__uuidof(IDCompositionDevice),(void**)&composition):E_NOINTERFACE;
        GenieRelease(factory);GenieRelease(adapter);GenieRelease(dxgi);
        if(SUCCEEDED(hr))hr=swap->GetBuffer(0,__uuidof(ID3D11Texture2D),(void**)&back);
        if(SUCCEEDED(hr))hr=composition->CreateTargetForHwnd(window,TRUE,&target);
        if(SUCCEEDED(hr))hr=composition->CreateVisual(&visual);
        D2D_MATRIX_3X2_F matrix{};matrix._11=(float)displayW/renderW;matrix._22=(float)displayH/renderH;
        if(SUCCEEDED(hr))hr=visual->SetTransform(matrix);
        if(SUCCEEDED(hr))hr=visual->SetBitmapInterpolationMode(DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);
        if(SUCCEEDED(hr))hr=visual->SetContent(swap);
        if(SUCCEEDED(hr))hr=target->SetRoot(visual);
        if(SUCCEEDED(hr))hr=composition->Commit();
        if(FAILED(hr)){Close();return false;}
        return true;
    }
    bool Present(const uint32_t* pixels,int width){
        if(!state||!swap)return false;
        std::lock_guard<std::mutex> lock(state->mutex);
        if(FAILED(state->device->GetDeviceRemovedReason()))return false;
        state->context->UpdateSubresource(back,0,nullptr,pixels,width*4,0);
        return SUCCEEDED(swap->Present(0,0));
    }
};

static double GenieRefreshHz(const RECT& rect){
    MONITORINFOEXW info{};info.cbSize=sizeof(info);
    if(!GetMonitorInfoW(MonitorFromRect(&rect,MONITOR_DEFAULTTONEAREST),&info))return 60;
    // Preserve fractional rates such as 59.94, using the active display path.
    for(int retry=0;retry<2;++retry){
        UINT32 pc=0,mc=0;
        if(GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&pc,&mc)!=ERROR_SUCCESS)break;
        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pc);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(mc);
        LONG status=QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&pc,paths.data(),&mc,modes.data(),nullptr);
        if(status==ERROR_INSUFFICIENT_BUFFER)continue;
        if(status!=ERROR_SUCCESS)break;
        for(UINT32 i=0;i<pc;++i){
            DISPLAYCONFIG_SOURCE_DEVICE_NAME name{};name.header.type=DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            name.header.size=sizeof(name);name.header.adapterId=paths[i].sourceInfo.adapterId;name.header.id=paths[i].sourceInfo.id;
            if(DisplayConfigGetDeviceInfo(&name.header)!=ERROR_SUCCESS||wcscmp(name.viewGdiDeviceName,info.szDevice))continue;
            auto rate=paths[i].targetInfo.refreshRate;
            double hz=rate.Denominator?(double)rate.Numerator/rate.Denominator:0;
            if(hz>=20&&hz<=1000)return hz;
        }
        break;
    }
    DEVMODEW mode{};mode.dmSize=sizeof(mode);
    if(EnumDisplaySettingsExW(info.szDevice,ENUM_CURRENT_SETTINGS,&mode,0)&&mode.dmDisplayFrequency>=20&&mode.dmDisplayFrequency<=1000)
        return mode.dmDisplayFrequency;
    return 60;
}
static float GenieRenderScale(int width,int height,double hz,int requested){
    if(requested>0)return std::clamp(requested,50,100)*0.01f;
    // Approximately a 1080p bitmap at 240 Hz. Select once per animation: no
    // within-animation resolution jumps. Keep at least half linear resolution.
    double pixelsPerSecond=(double)width*height*hz;
    double scale=sqrt(std::min(1.0,500000000.0/std::max(1.0,pixelsPerSecond)));
    return (float)std::clamp(floor(scale*20.0+1e-6)/20.0,0.5,1.0);
}
static uint32_t GeniePixelLerp(uint32_t a,uint32_t b,unsigned t){
    unsigned inv=256-t;
    uint32_t rb=(((a&0xff00ff)*inv+(b&0xff00ff)*t)>>8)&0xff00ff;
    uint32_t ga=((((a>>8)&0xff00ff)*inv+((b>>8)&0xff00ff)*t)>>8)&0xff00ff;
    return rb|(ga<<8);
}
static void GenieResizeSnapshot(const uint32_t* source,int w,int h,uint32_t* target,int tw,int th){
    if(w==tw&&h==th){memcpy(target,source,(size_t)w*h*4);return;}
    struct Sample {int x0,x1;unsigned weight;};
    std::vector<Sample> samples(tw);
    for(int x=0;x<tw;++x){
        double fx=std::max(0.0,(x+.5)*w/tw-.5);int x0=(int)fx;
        samples[x]={x0,std::min(x0+1,w-1),(unsigned)((fx-x0)*256)};
    }
    for(int y=0;y<th;++y){
        double fy=std::max(0.0,(y+.5)*h/th-.5);int y0=(int)fy,y1=std::min(y0+1,h-1);
        unsigned wy=(unsigned)((fy-y0)*256);
        const uint32_t* row0=source+(size_t)y0*w;const uint32_t* row1=source+(size_t)y1*w;
        uint32_t* dst=target+(size_t)y*tw;
        for(int x=0;x<tw;++x){
            const auto sample=samples[x];
            uint32_t a=GeniePixelLerp(row0[sample.x0],row0[sample.x1],sample.weight);
            uint32_t b=GeniePixelLerp(row1[sample.x0],row1[sample.x1],sample.weight);
            dst[x]=GeniePixelLerp(a,b,wy);
        }
    }
}

struct alignas(8) GenieResolutionStats {
    volatile LONG64 refreshMilliHz,scalePercent,renderPixels,displayPixels,gpuAnimations,fallbackAnimations,adapterLuidLow,adapterLuidHigh;
};
extern "C" { GenieResolutionStats g_GenieResolution{}; }

// CLASSIC_CORE_BEGIN -- shared verbatim with the rendering/geometry tests.
#if defined(__SSE2__)
#include <emmintrin.h>
#endif
static float ClassicEase(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static uint32_t ClassicBlurMix(uint32_t center, uint32_t before, uint32_t after, unsigned weight) {
    // Premultiplied [1,2,1] shutter kernel at full strength; packed 16-bit lanes
    // keep the three taps inexpensive and preserve transparent source pixels.
    const uint32_t sides = (before & after) + (((before ^ after) & 0xfefefefeu) >> 1);
    const unsigned inverse = 256 - weight;
    const uint32_t rb = (((center & 0x00ff00ffu)*inverse + (sides & 0x00ff00ffu)*weight) >> 8) & 0x00ff00ffu;
    const uint32_t ga = ((((center >> 8) & 0x00ff00ffu)*inverse + ((sides >> 8) & 0x00ff00ffu)*weight) >> 8) & 0x00ff00ffu;
    return rb | (ga << 8);
}

static void ClassicBlurRow(uint32_t* destination, const uint32_t* center,
                          const uint32_t* before, const uint32_t* after,
                          int width, unsigned weight) {
    int x = 0;
#if defined(__SSE2__)
    const __m128i one = _mm_set1_epi8(1);
    const __m128i zero = _mm_setzero_si128();
    const __m128i aWeight = _mm_set1_epi16((short)weight);
    const __m128i cWeight = _mm_set1_epi16((short)(256-weight));
    for (; x+4<=width; x+=4) {
        const __m128i a = _mm_loadu_si128((const __m128i*)(before+x));
        const __m128i b = _mm_loadu_si128((const __m128i*)(after+x));
        const __m128i c = _mm_loadu_si128((const __m128i*)(center+x));
        const __m128i sides = _mm_sub_epi8(_mm_avg_epu8(a,b), _mm_and_si128(_mm_xor_si128(a,b),one));
        __m128i result;
        if (weight == 128) {
            result = _mm_sub_epi8(_mm_avg_epu8(c,sides), _mm_and_si128(_mm_xor_si128(c,sides),one));
        } else {
            __m128i low = _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(c,zero),cWeight),
                                       _mm_mullo_epi16(_mm_unpacklo_epi8(sides,zero),aWeight));
            __m128i high = _mm_add_epi16(_mm_mullo_epi16(_mm_unpackhi_epi8(c,zero),cWeight),
                                        _mm_mullo_epi16(_mm_unpackhi_epi8(sides,zero),aWeight));
            result = _mm_packus_epi16(_mm_srli_epi16(low,8),_mm_srli_epi16(high,8));
        }
        _mm_storeu_si128((__m128i*)(destination+x),result);
    }
#endif
    for (; x<width; ++x) destination[x] = ClassicBlurMix(center[x],before[x],after[x],weight);
}

static float ClassicFramePhase(float timeline, bool rising) {
    const float t = std::clamp(timeline, 0.0f, 1.0f);
    if (!rising) return t;
    // Direct departure from the dock; brake only near the original rectangle.
    // The terminal quartic matches position, speed AND acceleration at t=.6.
    // There is no second clock for forming the funnel or moving its head.
    if (t <= 0.6f) return 1.0f - 1.25f*t;
    const float r = (1.0f - t) / 0.4f;
    return 0.25f*r*r*r*(2.0f-r);
}

static float ClassicFrameSpeed(float timeline, bool rising) {
    if (!rising) return 1.0f;
    const float t = std::clamp(timeline, 0.0f, 1.0f);
    if (t <= 0.6f) return 1.25f;
    const float r = (1.0f - t) / 0.4f;
    return 1.25f*r*r*(3.0f-2.0f*r);
}

static float ClassicNearDockStrength(int height, float distance) {
    // Adapt continuously to geometry, not an IsZoomed/fullscreen mode flag.
    // A touching window has no travel space for the broad leading edge.
    const float gap = std::max(0.0f, distance - height);
    return 1.0f - ClassicEase(gap / (0.12f * height));
}

static float ClassicHorizontal(float channelPosition, float sourcePosition, float progress,
                               float nearDock = 0.0f) {
    // One transport parameter p drives BOTH row positions and their entry into
    // the spatial funnel. No hold, phase cutoff, or independently eased width.
    // The source-row dependence sends a short convex shoulder upward while the
    // leading rows MOVE and narrow. Only entrance contraction is moderated;
    // row transport and the bounded terminal upper-edge correction stay intact.
    const float z = std::clamp(channelPosition, 0.0f, 1.0f);
    // Spread entrance contraction over more frames with a mild continuous bias.
    // It starts immediately and reaches the same endpoint; no hold or extra
    // animation time is introduced. This is not applied to the transport clock.
    const float entrance = progress / (1.0f + 0.35f*(1.0f-progress));
    const float q = 1.0f - entrance, q2 = q*q;
    const float r = 1.0f - entrance*sourcePosition, r2 = r*r;
    const float ordinaryResidual = q2*q2*q2*r2*r2*r;
    // The old touching-edge initial contraction slope was 11 widths/phase.
    // This spatial response lowers it to 2, then converges during the SAME
    // transport. No startup delay, geometry jump, or extra easing of time.
    const float s = 1.0f - entrance*entrance, s2 = s*s, s4 = s2*s2;
    const float closeResidual = q*r*s4*s4;
    const float entry = 1.0f - (ordinaryResidual + nearDock*(closeResidual-ordinaryResidual));
    const float base = z*(2.0f-z)*entry;
    if (progress <= 0.65f) return base;
    // Late upper-edge adjustment only. Motion itself stays continuous/linear.
    // Limit extra narrowing to 30% of the width above the neck; retain at least
    // 70% of the previous silhouette, instead of turning the body into a thread.
    const float amount = 0.30f * ClassicEase((progress-0.65f)/0.35f);
    const float headPosition = progress*progress*progress;
    const float upper = std::clamp((1.0f-z)/std::max(0.0001f,1.0f-headPosition),0.0f,1.0f);
    // Spatial weight vanishes at the mouth, keeping the near-dock fix exact.
    return base + (1.0f-base)*amount*upper;
}

static float ClassicNeckWidth(int width) {
    return std::clamp(width * 0.012f, 6.0f, 18.0f);
}

static void ClassicRowMap(float* rows, int height, float distance, float progress) {
    // Canonical distance runs from the far edge toward the dock. A source row v
    // follows F(v,p) = H*v + D*p^(3-2*v). Every row moves for every p>0;
    // the far edge naturally lags without ever being held. Leading rows stretch
    // the texture even when H == D (a maximized window already touches the dock).
    // dF/dv = H - 2*D*log(p)*p^(3-2*v) > H for 0<p<1: no folds or compression.
    // Use one exp/log per frame and a double-precision recurrence, not h powers.
    if (progress <= 0.0f) {
        for (int k = 0; k <= height; ++k) rows[k] = (float)k;
        return;
    }
    const double p = std::min((double)progress, 1.0);
    double advance = distance * p * p * p;
    const double step = exp(-2.0 * log(p) / height);
    for (int k = 0; k <= height; ++k) {
        rows[k] = (float)(k + advance);
        advance *= step;
    }
}

static uint32_t ClassicCoverage(uint32_t pixel, float coverage) {
    unsigned alpha = (unsigned)(std::clamp(coverage, 0.0f, 1.0f) * 256.0f + 0.5f);
    uint32_t rb = (((pixel & 0x00ff00ffu) * alpha) >> 8) & 0x00ff00ffu;
    uint32_t ga = ((((pixel >> 8) & 0x00ff00ffu) * alpha) >> 8) & 0x00ff00ffu;
    return rb | (ga << 8);
}

struct DockEdge { RECT work; bool above; };

static DockEdge ResolveDockEdge(const RECT& monitor, const RECT& work,
                                const RECT* tray) {
    DockEdge edge{work, false};
    if (tray && tray->right > tray->left && tray->bottom > tray->top) {
        bool horizontal = tray->right - tray->left >= tray->bottom - tray->top;
        if (horizontal) {
            edge.above = (tray->top + tray->bottom) < (monitor.top + monitor.bottom);
            if (edge.above) edge.work.top = std::max(work.top, std::min(monitor.bottom - 1, tray->bottom));
            else edge.work.bottom = std::min(work.bottom, std::max(monitor.top + 1, tray->top));
        } else {
            bool left = tray->left + tray->right < monitor.left + monitor.right;
            if (left) edge.work.left = std::max(work.left, std::min(monitor.right - 1, tray->right));
            else edge.work.right = std::min(work.right, std::max(monitor.left + 1, tray->left));
        }
    } else {
        edge.above = work.top > monitor.top;
    }
    return edge;
}

static DockEdge GetDockEdge(HMONITOR monitor) {
    MONITORINFO mi{sizeof(mi)};
    if (!GetMonitorInfoW(monitor, &mi)) {
        mi.rcMonitor = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
        mi.rcWork = mi.rcMonitor;
    }
    HWND tray = FindTaskbarForMonitor(monitor);
    RECT tr{};
    bool hasTray = tray && GetWindowRect(tray, &tr);
    // RECT bottom/right are exclusive: ending exactly at the taskbar edge
    // remains outside it, without exposing a DPI-scaled stripe of wallpaper.
    return ResolveDockEdge(mi.rcMonitor, mi.rcWork, hasTray ? &tr : nullptr);
}

class ClassicRaster {
    int w, h, left, top, canvasW, canvasH;
    float originX, originY, dockX, dockY, neck, nearDock;
    float pixelW,pixelH;
    int textureW,textureH;
    bool above;
    struct Span { int first = 0, end = 0; };
    std::vector<Span> previous;
    std::vector<float> rowMap;
    std::vector<uint32_t> blurRow;
public:
    ClassicRaster(int width, int height, int x, int y, float dx, float dy,
                  int canvasLeft, int canvasTop, int cw, int ch,
                  float worldPixelW=1.0f,float worldPixelH=1.0f,int tw=0,int th=0)
        : w(width), h(height), left(canvasLeft), top(canvasTop), canvasW(cw), canvasH(ch),
          originX(x + width * 0.5f), originY((float)y), dockX(dx), dockY(dy),
          neck(ClassicNeckWidth(width)),
          nearDock(ClassicNearDockStrength(height, std::max(dy-y, y+height-dy))),
                     pixelW(worldPixelW),pixelH(worldPixelH),textureW(tw?tw:width),textureH(th?th:height),
           above(dy < y + height * 0.5f), previous(ch), rowMap(height + 1), blurRow(textureW) {}

    void Render(const uint32_t* source, uint32_t* canvas, float t, float shutterProgress = 0.0f) {
        t = std::clamp(t, 0.0f, 1.0f);
        const float farY = above ? originY + h : originY;
        const float direction = above ? -1.0f : 1.0f;
        const float distance = std::max(1.0f, (dockY - farY) * direction);
        auto clearRow=[&](int y) {
            auto& span=previous[y];
            if(span.end>span.first)memset(canvas+(size_t)y*canvasW+span.first,0,(size_t)(span.end-span.first)*4);
            span={};
        };
        if(t>=1.0f){for(int y=0;y<canvasH;++y)clearRow(y);return;}
        ClassicRowMap(rowMap.data(), h, distance, t);
        // Normally D >= H and the clipping plane stays at the dock. For a
        // cross-monitor target inside the source's vertical span, introduce the
        // clipping plane continuously to preserve the original identity frame.
        const float clipDistance = std::max(distance, h + (distance - h) * ClassicEase(t));
        const float front = farY + direction * rowMap.front();
        const float mouth = farY + direction * std::min(rowMap.back(), clipDistance);
        const float yTop = std::min(front, mouth), yBottom = std::max(front, mouth);
        int firstY = std::max(0, (int)floorf((yTop - top)/pixelH));
        int endY = std::min(canvasH, (int)ceilf((yBottom - top)/pixelH));
        for(int y=0;y<canvasH;++y)if(y<firstY||y>=endY)clearRow(y);
        const float invLength = 1.0f / distance;
        int segment = 0;
        int cachedSource = -1, cachedShift = -1;
        unsigned cachedWeight = 0;
        // Traverse toward the dock in both orientations. Each source boundary
        // is visited at most once, so inversion costs O(canvasHeight + H).
        for (int row = firstY; row < endY; ++row) {
            const int y = above ? endY - 1 - (row - firstY) : row;
            float sy = top+(y+0.5f)*pixelH;
            float d = (sy - farY) * direction;
            while (segment < h - 1 && rowMap[segment + 1] <= d) ++segment;
            float channelPosition = std::clamp(d * invLength, 0.0f, 1.0f);
            float fraction = std::clamp((d-rowMap[segment]) /
                (rowMap[segment+1]-rowMap[segment]), 0.0f, 1.0f);
            float sourcePosition = (segment+fraction)/h;
            float e = ClassicHorizontal(channelPosition, sourcePosition, t, nearDock);
            float width = std::max(1.0f, w + (neck - w) * e)/pixelW;
            float cx = originX + (dockX - originX) * e;
            float xLeft = (cx-left)/pixelW-width*0.5f;
            int begin = std::max(0, (int)floorf(xLeft));
            int end = std::min(canvasW, (int)ceilf(xLeft + width));
            if (end <= begin) {clearRow(y);continue;}
            int sourceY = segment;
            if (above) sourceY = h - 1 - sourceY;
            sourceY=(int)((int64_t)sourceY*textureH/h);
            const auto* src = source + (size_t)sourceY * textureW;
            auto* dst = canvas + (size_t)y * canvasW;
            const auto old=previous[y];
            int eraseEnd=std::min(old.end,begin),eraseBegin=std::max(old.first,end);
            if(eraseEnd>old.first)memset(dst+old.first,0,(size_t)(eraseEnd-old.first)*4);
            if(old.end>eraseBegin)memset(dst+eraseBegin,0,(size_t)(old.end-eraseBegin)*4);
            previous[y] = {begin, end};
            if (e == 0.0f && pixelW==1.0f && textureW==w) {
                // Untouched rows are a contiguous identity copy (most early pixels).
                int sx = std::clamp((int)(begin + 0.5f - xLeft), 0, w - 1);
                std::memcpy(dst + begin, src + sx, (size_t)(end - begin) * 4);
            } else {
                double scale = (double)textureW / width;
                // 32.32 fixed-point stepping removes float conversions and
                // multiplication from every pixel. Subpixel error remains below
                // 1/65536 source pixel for any supported canvas width.
                constexpr double unit = 4294967296.0;
                int64_t sample = (int64_t)((begin + 0.5 - xLeft) * scale * unit);
                uint64_t step = (uint64_t)(scale * unit);
                int64_t limit = (int64_t)textureW - 1;
                // The fast shutter follows longitudinal texture flow. Filter
                // contiguous source rows with SIMD before the horizontal warp,
                // so every output pixel still needs only one texture lookup.
                // A single cached scanline suffices; no full-frame blur buffer.
                if (shutterProgress > 0.0f && t > 0.0f) {
                    const float stretch = std::max(1.0f, rowMap[segment+1]-rowMap[segment]);
                    const float v = (segment+0.5f)/h;
                    const float advance = (rowMap[segment]+rowMap[segment+1])*0.5f-(segment+0.5f);
                    const float velocity = (3.0f-2.0f*v)*advance/t;
                    const float yMotion = std::clamp(velocity*shutterProgress, 0.0f, 4.0f);
                    const int shift = (int)roundf(yMotion/stretch*textureH/h);
                    const unsigned weight = (unsigned)(128.0f*ClassicEase(yMotion/1.5f));
                    if (weight && shift) {
                        if (cachedSource != sourceY || cachedShift != shift || cachedWeight != weight) {
                            const auto* before = source+(size_t)std::clamp(sourceY-shift,0,textureH-1)*textureW;
                            const auto* after = source+(size_t)std::clamp(sourceY+shift,0,textureH-1)*textureW;
                            ClassicBlurRow(blurRow.data(),src,before,after,textureW,weight);
                            cachedSource=sourceY;cachedShift=shift;cachedWeight=weight;
                        }
                        src=blurRow.data();
                    }
                }
                // Only the two boundary samples can lie outside the source.
                dst[begin]=src[std::clamp(sample>>32,int64_t{0},limit)];sample+=step;
                for(int x=begin+1;x<end-1;++x){dst[x]=src[sample>>32];sample+=step;}
                if(end-begin>1)dst[end-1]=src[std::clamp(sample>>32,int64_t{0},limit)];
            }

            // Pixel coverage smooths the funnel outline. Only the two boundary
            // pixels and two partial Y rows need alpha math, not the whole image.
            float leftCoverage = std::min(xLeft + width, (float)begin + 1) - std::max(xLeft, (float)begin);
            dst[begin] = ClassicCoverage(dst[begin], leftCoverage);
            if (end - begin > 1) {
                float rightCoverage = std::min(xLeft + width, (float)end) - std::max(xLeft, (float)end - 1);
                dst[end - 1] = ClassicCoverage(dst[end - 1], rightCoverage);
            }
            float yCoverage = (std::min(yBottom,top+(y+1)*pixelH)-std::max(yTop,top+y*pixelH))/pixelH;
            if (yCoverage < 1.0f)
                for (int x = begin; x < end; ++x) dst[x] = ClassicCoverage(dst[x], yCoverage);
        }
    }
};
// CLASSIC_CORE_END

// Transfer the worker's immutable DIB into the bounded restore cache.
// No full-window allocation/copy is needed at minimize completion.
static void CacheFinishedSnapshot(MacGenieAnimData* data) {
    if (data->isRising || !g_openAnimation.load(std::memory_order_relaxed) ||
        g_unloading.load(std::memory_order_relaxed) || !IsWindow(data->hRealWnd)) return;
    const size_t incomingBytes = (size_t)data->width * data->height * 4;
    if (incomingBytes > 64 * 1024 * 1024) return;
    std::lock_guard<std::mutex> lock(g_CacheMutex);
    static ULONGLONG sequence = 0; // lock-protected; no equal-millisecond LRU ties
    auto existing = g_SnapshotCache.find(data->hRealWnd);
    if (existing != g_SnapshotCache.end()) {
        DeleteObject(existing->second.hBmp);
        g_SnapshotCache.erase(existing);
    }
    size_t cacheBytes = 0;
    for (const auto& entry : g_SnapshotCache)
        cacheBytes += (size_t)entry.second.w * entry.second.h * 4;
    while (!g_SnapshotCache.empty() &&
           (g_SnapshotCache.size() >= 8 || cacheBytes + incomingBytes > 64 * 1024 * 1024)) {
        auto oldest = std::min_element(g_SnapshotCache.begin(), g_SnapshotCache.end(),
            [](const auto& a, const auto& b) { return a.second.stamp < b.second.stamp; });
        cacheBytes -= (size_t)oldest->second.w * oldest->second.h * 4;
        DeleteObject(oldest->second.hBmp);
        g_SnapshotCache.erase(oldest);
    }
    g_SnapshotCache[data->hRealWnd] = {data->hBitmap, data->pBits,
        data->width, data->height, ++sequence};
    data->hBitmap = nullptr;
    data->pBits = nullptr;
}

DWORD WINAPI MacGenieAnimThreadClassic(LPVOID lpParam) {
    // Destroy compositor objects before allowing the DLL/device to unload.
    struct WorkerFinish {~WorkerFinish(){g_workerCount.fetch_sub(1,std::memory_order_release);}} finish;
    MacGenieAnimData* data = (MacGenieAnimData*)lpParam;

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const int W = data->width;
    const int H = data->height;
    // The monitor the genie plays on, in virtual-screen coords (from the shared
    // setup: the primary when multi-monitor support is off, else the window's).
    RECT mon;
    MONITORINFO mmi; mmi.cbSize = sizeof(mmi);
    if (data->hMon && GetMonitorInfoW(data->hMon, &mmi)) {
        mon = mmi.rcMonitor;
    } else {
        mon.left = 0; mon.top = 0;
        mon.right = GetSystemMetrics(SM_CXSCREEN);
        mon.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    const int origLeft = data->targetRect.left;
    const int origTop  = data->targetRect.top;
    const DockEdge edge = GetDockEdge(data->hMon);
    // Capture/setup can give the background query time to finish. Adopt its
    // result once, before frame zero; never move the target during the warp.
    data->targetDockX=CachedTaskbarButtonX(data->hRealWnd,data->targetDockX,data->hMon);
    const int dockX = std::clamp(data->targetDockX, (int)edge.work.left, (int)edge.work.right - 1);
    const float dockY = edge.above ? (float)edge.work.top : (float)edge.work.bottom;
    const float neckW = ClassicNeckWidth(W);
    // Every row edge is a convex combination of its source and neck edges.
    // Their union is an exact bound: no half-window padding is necessary.
    const int boundLeft = std::min(origLeft, (int)floorf(dockX - neckW * 0.5f));
    const int boundRight = std::max(origLeft + W, (int)ceilf(dockX + neckW * 0.5f));
    const int boundTop = std::min(origTop, (int)dockY);
    const int boundBottom = std::max(origTop + H, (int)dockY);
    const int boundW = std::max(1, boundRight - boundLeft);
    const int boundH = std::max(1, boundBottom - boundTop);

    const double refreshHz=GenieRefreshHz(data->targetRect);
    const double frameIntervalMs=1000.0/refreshHz;
    const float requestedScale=GenieRenderScale(boundW,boundH,refreshHz,g_renderScalePercent.load());
    int renderW=std::max(1,(int)ceilf(boundW*requestedScale));
    int renderH=std::max(1,(int)ceilf(boundH*requestedScale));
    bool reduced=renderW<boundW||renderH<boundH;
    auto createGhost=[&](bool lowResolution){return CreateWindowExW(
        (lowResolution?WS_EX_NOREDIRECTIONBITMAP:WS_EX_LAYERED)|WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_TRANSPARENT,
        L"STATIC",nullptr,WS_POPUP,boundLeft,boundTop,boundW,boundH,nullptr,nullptr,nullptr,nullptr);};
    HWND hGhost=createGhost(reduced);
    GenieGpuCanvas gpu;
    bool gpuReady=reduced&&hGhost&&gpu.Open(hGhost,renderW,renderH,boundW,boundH);
    if(reduced&&!gpuReady){
        InterlockedIncrement64(&g_GenieResolution.fallbackAnimations);
        if(hGhost)DestroyWindow(hGhost);
        hGhost=createGhost(false);renderW=boundW;renderH=boundH;
    }
    if(gpuReady)InterlockedIncrement64(&g_GenieResolution.gpuAnimations);
    const LUID adapterLuid=gpu.AdapterLuid();
    InterlockedExchange64(&g_GenieResolution.adapterLuidLow,adapterLuid.LowPart);
    InterlockedExchange64(&g_GenieResolution.adapterLuidHigh,adapterLuid.HighPart);
    InterlockedExchange64(&g_GenieResolution.refreshMilliHz,(LONG64)(refreshHz*1000+.5));
    InterlockedExchange64(&g_GenieResolution.scalePercent,gpuReady?(LONG64)(requestedScale*100+.5):100);
    InterlockedExchange64(&g_GenieResolution.renderPixels,(LONG64)renderW*renderH);
    InterlockedExchange64(&g_GenieResolution.displayPixels,(LONG64)boundW*boundH);

    HDC hScreenDC = GetDC(NULL);

    // The capture already is a premultiplied, top-down DIB. No second DIB,
    // two extra DCs, BitBlt or GdiFlush are needed in the rendering thread.
    const auto* srcBits = static_cast<const uint32_t*>(data->pBits);

    // Canvas: 32-bit top-down DIB for genuine per-pixel alpha.
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = renderW;
    bmi.bmiHeader.biHeight = -renderH;   // negative height = top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    std::vector<uint32_t> reducedCanvas,reducedSource;
    if(gpuReady)reducedCanvas.resize((size_t)renderW*renderH);
    BYTE* pBits=gpuReady?reinterpret_cast<BYTE*>(reducedCanvas.data()):nullptr;
    HBITMAP hCanvas=gpuReady?nullptr:CreateDIBSection(hScreenDC,&bmi,DIB_RGB_COLORS,(void**)&pBits,nullptr,0);
    HDC hCanvasDC=gpuReady?nullptr:CreateCompatibleDC(hScreenDC);
    HBITMAP hOldCanvas=hCanvas&&hCanvasDC?(HBITMAP)SelectObject(hCanvasDC,hCanvas):nullptr;
    const float pixelW=(float)boundW/renderW,pixelH=(float)boundH/renderH;
    const int textureW=gpuReady?std::max(1,(int)ceilf(W/pixelW)):W;
    const int textureH=gpuReady?std::max(1,(int)ceilf(H/pixelH)):H;
    const uint32_t* renderSource=srcBits;
    if(gpuReady){
        reducedSource.resize((size_t)textureW*textureH);
        GenieResizeSnapshot(srcBits,W,H,reducedSource.data(),textureW,textureH);
        renderSource=reducedSource.data();
    }
    ClassicRaster raster(W,H,origLeft,origTop,dockX,dockY,boundLeft,boundTop,renderW,renderH,
        pixelW,pixelH,textureW,textureH);
    const double totalMs=(double)data->durationMs;
    const bool motionBlur=g_motionBlur.load(std::memory_order_relaxed);
    const bool ready=hGhost&&hScreenDC&&pBits&&(gpuReady||(hCanvas&&hCanvasDC&&hOldCanvas));
    if(ready)memset(pBits,0,(size_t)renderW*renderH*4);

    // Wall-clock progress, paced at the display refresh rate.
    LARGE_INTEGER qpcFreq, qpcStart, qpcNow;
    QueryPerformanceFrequency(&qpcFreq);
    QueryPerformanceCounter(&qpcStart);

    HANDLE hFrameTimer = CreateWaitableTimerExW(
        nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_MODIFY_STATE | SYNCHRONIZE);
    if (!hFrameTimer) {
        // Pre-1803: the high-resolution flag isn't recognized. A plain waitable
        // timer paces coarser, but it still isn't a spin.
        hFrameTimer = CreateWaitableTimerExW(nullptr, nullptr, 0,
                                             TIMER_MODIFY_STATE | SYNCHRONIZE);
    }

    BOOL firstFrame = TRUE;
    for (; ready;) {
        QueryPerformanceCounter(&qpcNow);
        const LARGE_INTEGER qpcFrameStart = qpcNow;
        double elapsedMs = (qpcNow.QuadPart - qpcStart.QuadPart) * 1000.0 / qpcFreq.QuadPart;
        BOOL lastFrame = (elapsedMs >= totalMs);
        const float timeline = firstFrame ? 0.0f : (lastFrame ? 1.0f : (float)(elapsedMs / totalMs));
        const float progress = timeline;
        const float shutterProgress = motionBlur
            ? (float)(ClassicFrameSpeed(timeline, data->isRising) * std::min(8.0, frameIntervalMs * 0.5) / (2.0 * totalMs)) : 0.0f;

        // 0 = full window at its place, 1 = collapsed at the dock; restore reverses.
        float tt = ClassicFramePhase(progress, data->isRising);

        { GenieTiming timing{4};
          raster.Render(renderSource, reinterpret_cast<uint32_t*>(pBits), tt, shutterProgress); }

        POINT ptDst = { boundLeft, boundTop };
        SIZE  sz    = { boundW, boundH };
        POINT ptSrc = { 0, 0 };
        BLENDFUNCTION bf;
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = 255; // Disappear by geometric clipping at the mouth.
        bf.AlphaFormat = AC_SRC_ALPHA;
        BOOL presented;
        { GenieTiming timing{5};
          presented=gpuReady?gpu.Present(reinterpret_cast<uint32_t*>(pBits),renderW):
              UpdateLayeredWindow(hGhost,hScreenDC,&ptDst,&sz,hCanvasDC,&ptSrc,0,&bf,ULW_ALPHA); }
        if (!presented) break;

        if (firstFrame) {
            ShowWindow(hGhost, SW_SHOWNOACTIVATE);
        }

        if (lastFrame) break;
        if (g_unloading.load(std::memory_order_relaxed)) break;

        // First frame is on screen; release the minimize hook it was holding.
        // One DwmFlush here matches the modern engine's handoff sync; subsequent
        // frames are paced by the timer below instead of a per-frame DwmFlush.
        if (firstFrame) {
            DwmFlush();
            QueryPerformanceCounter(&qpcStart); // the visible animation gets the full duration
            firstFrame = FALSE;
            if (data->hFirstFrameShown) SetEvent(data->hFirstFrameShown);
        }

        // Rate-limit to ~display refresh so we never busy-spin. The deadline is
        // measured from the START of the frame, so the period is
        // max(render, interval), not render + interval (a render faster than
        // the interval just sleeps the difference; a slower one runs as-is).
        // Capped wait keeps the g_unloading check responsive. When a frame
        // overruns its budget, SwitchToThread still yields the quantum.
        if (hFrameTimer) {
            LARGE_INTEGER qpcAfter;
            QueryPerformanceCounter(&qpcAfter);
            double spentMs = (qpcAfter.QuadPart - qpcFrameStart.QuadPart) * 1000.0 / qpcFreq.QuadPart;
            double waitMs = frameIntervalMs - spentMs;
            if (waitMs > 0.1) {
                LARGE_INTEGER due;
                due.QuadPart = -(LONGLONG)(waitMs * 10000.0);   // 100ns units
                SetWaitableTimer(hFrameTimer, &due, 0, nullptr, nullptr, FALSE);
                WaitForSingleObject(hFrameTimer, 100);
            } else {
                SwitchToThread();   // overran the budget - yield the quantum anyway
            }
        } else {
            Sleep(1);   // last resort: never spin
        }
    }

    if (hFrameTimer) { CloseHandle(hFrameTimer); hFrameTimer = nullptr; }

    // -------------------- COMMON TEARDOWN (identical to the modern engine) -----
    if (data->isRising) {
        // Reveal the real window the same way it was hidden.
        if (data->hiddenByCloak) {
            MacGenieSetCloak(data->hRealWnd, FALSE);
        } else {
            SetLayeredWindowAttributes(data->hRealWnd, 0, 255, LWA_ALPHA);
            if (!(data->originalExStyle & WS_EX_LAYERED)) {
                SetWindowLongPtrW(data->hRealWnd, GWL_EXSTYLE, data->originalExStyle);
            }
        }
    }

    // The auto-hide deferred path keeps transitions disabled through the real
    // (deferred) minimize below; every other path re-enables them now.
    if (!data->deferredMinimize) {
        MacGenieSetDwmTransitions(data->hRealWnd, TRUE);
    }

    if(hOldCanvas)SelectObject(hCanvasDC,hOldCanvas);
    gpu.Close();
    DeleteObject(hCanvas);
    CacheFinishedSnapshot(data);
    DeleteObject(data->hBitmap);
    DeleteDC(hCanvasDC);
    ReleaseDC(NULL, hScreenDC);
    DestroyWindow(hGhost);

    if (data->hFirstFrameShown) {
        SetEvent(data->hFirstFrameShown);
        CloseHandle(data->hFirstFrameShown);
    }

    // Auto-hide "unhide" completion (Potassiumuncher's engine) - same as the modern
    // thread, so the auto-hide feature behaves identically under the classic style.
    if (data->deferredMinimize) {
        int unhideMs = data->unhideDurationMs;
        int animMs = data->durationMs;
        if (data->requestedUnhide && unhideMs > animMs) {
            // Chunked sleep so the unload drain (which waits ~3 s for workers to
            // see g_unloading) never times out on the up-to-~5 s unhide wait and
            // leaves this thread in unmapped code.
            int remaining = unhideMs - animMs;
            while (remaining > 0 && !g_unloading.load(std::memory_order_relaxed)) {
                int chunk = remaining < 20 ? remaining : 20;
                Sleep(chunk);
                remaining -= chunk;
            }
        }
        if (!g_unloading.load(std::memory_order_relaxed) && IsWindow(data->hRealWnd)) {
            SetPropW(data->hRealWnd, L"GenieBypass", (HANDLE)1);
            SendMessageTimeoutW(data->hRealWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0, SMTO_NORMAL, 1000, NULL);
            RemovePropW(data->hRealWnd, L"GenieBypass");
        }
        if (IsWindow(data->hRealWnd)) {
            MacGenieSetCloak(data->hRealWnd, FALSE);
            MacGenieSetDwmTransitions(data->hRealWnd, TRUE);
        }
        if (data->requestedUnhide && !g_unloading.load(std::memory_order_relaxed)) {
            if (data->hNextApp && IsWindow(data->hNextApp) && IsWindowVisible(data->hNextApp)) {
                SetForegroundWindow(data->hNextApp);
            } else {
                SetForegroundWindow(FindWindowW(L"Progman", NULL));
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        g_AnimActive.erase(data->hRealWnd);
    }
    delete data;
    return 0;
}

// -------------------------------------------------------------------------
// Core Setup Engine & Smart Tracking Logic
// -------------------------------------------------------------------------
// Returns TRUE if the worker thread was spawned (the animation is now in flight).
// `originalExStyle` is the window's TRUE extended style, captured by the caller
// BEFORE any WS_EX_LAYERED was added for the hide (rising callers hide first).
// `cloakHidden` says how a rising caller hid the window: TRUE = DWM cloak (restore),
// FALSE = WS_EX_LAYERED + alpha 0 (launch). The deferred* args drive the auto-hide
// "unhide" path (falling only): the caller cloaks the window after our first frame
// and we perform the real minimize at teardown.
bool StartMacGenieAnim(HWND hWnd, BOOL rising, LONG_PTR originalExStyle,
                       BOOL cloakHidden = FALSE, BOOL deferredMinimize = FALSE,
                       BOOL requestedUnhide = FALSE, HWND hNextApp = NULL) {
    struct DpiScope {
        DPI_AWARENESS_CONTEXT old = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        ~DpiScope() { if (old) SetThreadDpiAwarenessContext(old); }
    } dpiScope;
    // Bound the snapshot cache: windows closed while minimized (the common
    // WM_DESTROY + PostQuitMessage path never reaches DefWindowProcW, where the
    // usual cleanup lives) would otherwise leave their full DIB behind until
    // unload. Cheap sweep at the single animation entry point.
    {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        for (auto it = g_SnapshotCache.begin(); it != g_SnapshotCache.end();) {
            if (!IsWindow(it->first)) {
                DeleteObject(it->second.hBmp);
                it = g_SnapshotCache.erase(it);
            } else {
                ++it;
            }
        }
    }

    // "Excluded programs" gate - single choke point for every animation path
    // (all six minimize/restore hooks AND the launch thread funnel through here).
    // Bail exactly like the other early-outs so hook-side state is restored: a
    // rising caller gets its hide undone, a falling caller gets DWM transitions
    // back so the DEFAULT system animation plays for the excluded app.
    if (MacGenieIsExcluded(hWnd) ||
        MacGenieIsTransparentOverlay(hWnd,originalExStyle,rising&&!cloakHidden)) {
        if (rising) MacGenieUndoRisingHide(hWnd, originalExStyle, cloakHidden);
        else MacGenieSetDwmTransitions(hWnd, TRUE);
        return false;
    }

    // Animation geometry via DWM extended frame bounds (Goal D / bug #4670):
    // GetWindowRect returns the legacy frame (invisible resize borders / shadow),
    // which is offset a few px from where the pixels actually are on composited
    // windows. Snapshot AND warp both use the extended-frame rect, so keyboard /
    // AutoHotkey minimizes are pixel-aligned. Scheme from Potassiumuncher's engine.
    RECT winRect;
    if (!GetWindowRect(hWnd, &winRect)) {
        if (rising) MacGenieUndoRisingHide(hWnd, originalExStyle, cloakHidden);
        else MacGenieSetDwmTransitions(hWnd, TRUE);
        return false;
    }
    RECT rect = winRect, extRect;
    if (SUCCEEDED(DwmGetWindowAttribute(hWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &extRect, sizeof(extRect)))) {
        rect = extRect;
    }
    HMONITOR sourceMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    DockEdge sourceEdge = GetDockEdge(sourceMonitor);
    RECT clipped;
    if (IntersectRect(&clipped, &rect, &sourceEdge.work)) rect = clipped;
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    int offsetX = rect.left - winRect.left;
    int offsetY = rect.top - winRect.top;
    int rawW = winRect.right - winRect.left;
    int rawH = winRect.bottom - winRect.top;

    if (w <= 0 || h <= 0 || rawW <= 0 || rawH <= 0) {
        if (rising) MacGenieUndoRisingHide(hWnd, originalExStyle, cloakHidden);
        else MacGenieSetDwmTransitions(hWnd, TRUE);
        return false;
    }

    // One genie per window at a time (MINE). Any window we animate also counts as
    // "seen", so the launch path won't fire on it later. Refused while unloading.
    bool blocked = g_unloading.load(std::memory_order_relaxed);
    if (!blocked) {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        blocked = g_unloading.load(std::memory_order_relaxed) || !g_AnimActive.insert(hWnd).second;
        if(!blocked)g_workerCount.fetch_add(1,std::memory_order_relaxed);
        if (!blocked && g_launchAnimation.load(std::memory_order_relaxed)) g_LaunchSeen.insert(hWnd);
    }
    if (blocked) {
        if (rising) {
            MacGenieUndoRisingHide(hWnd, originalExStyle, cloakHidden);
        } else {
            bool owned;
            {
                std::lock_guard<std::mutex> lock(g_CacheMutex);
                owned = g_AnimActive.count(hWnd) != 0;
            }
            if (!owned) MacGenieSetDwmTransitions(hWnd, TRUE);
        }
        return false;
    }

    struct PreparationLifetime {
        bool transferred=false;
        ~PreparationLifetime(){if(!transferred)g_workerCount.fetch_sub(1,std::memory_order_release);}
    } lifetime;

    // Monitor the genie targets. Multi-monitor support (experimental) targets the
    // window's monitor; otherwise the primary, which reproduces the old behavior.
    bool multiMon = g_multiMonitor.load(std::memory_order_relaxed);
    HMONITOR hMon = multiMon
        ? MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST)
        : MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hMon, &mi)) {
        mi.rcMonitor.left = 0; mi.rcMonitor.top = 0;
        mi.rcMonitor.right = GetSystemMetrics(SM_CXSCREEN);
        mi.rcMonitor.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    int monWidth = mi.rcMonitor.right - mi.rcMonitor.left;

    // Default dock target before UIA (Potassiumuncher's engine): left-aligned
    // taskbars start near the monitor's left, centered ones at its middle.
    DWORD alignVal = 1, dataSize = sizeof(alignVal);
    RegGetValueW(HKEY_CURRENT_USER,
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
                 L"TaskbarAl", RRF_RT_REG_DWORD, NULL, &alignVal, &dataSize);
    int learnedTargetX = (alignVal == 0) ? (mi.rcMonitor.left + 160)
                                         : (mi.rcMonitor.left + monWidth / 2);

    // --- Per-window targeting ---
    // UI Automation (GetTaskbarButtonX) matches taskbar buttons by title / process
    // name, which can't reliably tell apart several windows of the SAME process
    // (e.g. multiple Brave profiles): they tie on the process-name score and it
    // resolves them all to the first-opened window's button. So MINE's cursor +
    // per-window signal takes priority; UIA is only a fallback for a window we've
    // never seen minimized from the taskbar (frame 0 of the warp is the identity,
    // so targetDockX is only needed for later frames).
    //
    // 1. Cursor over the taskbar = the user clicked THIS window's own taskbar
    //    button, so its X is exactly the icon - the only fully reliable per-window
    //    signal when windows share a process. Learn it, keyed by HWND (never shared
    //    across sibling windows).
    POINT pt;
    GetCursorPos(&pt);
    RECT workArea;
    MONITORINFO cursorMi;
    cursorMi.cbSize = sizeof(cursorMi);
    if (GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &cursorMi)) {
        workArea = cursorMi.rcWork;
    } else {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    }

    HWND targetTray = FindTaskbarForMonitor(hMon);
    RECT trayRect{};
    if (targetTray && GetWindowRect(targetTray, &trayRect) && PtInRect(&trayRect, pt)) {
        learnedTargetX = pt.x;                        // steal the click X
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        DWORD pid=0;GetWindowThreadProcessId(hWnd,&pid);
        g_IconPositions[hWnd] = {learnedTargetX,pid,hMon,GetTickCount64()};       // remember it for this window
    } else {
        // Read a known target immediately; refresh in the background if stale.
        learnedTargetX = GetTaskbarButtonX(hWnd, learnedTargetX, hMon);
    }

    MacGenieAnimData* data = new MacGenieAnimData();
    data->hRealWnd = hWnd;
    data->targetRect = rect;
    data->hMon = hMon;
    data->width = w;
    data->height = h;
    data->isRising = rising;
    data->targetDockX = learnedTargetX;
    data->originalExStyle = originalExStyle;
    data->hiddenByCloak = cloakHidden;
    data->hFirstFrameShown = NULL;
    data->durationMs = g_durationMs.load(std::memory_order_relaxed);
    data->requestedUnhide = requestedUnhide;
    data->hNextApp = hNextApp;
    data->unhideDurationMs = g_unhideDurationMs.load(std::memory_order_relaxed);
    data->deferredMinimize = deferredMinimize;
    data->pBits = nullptr;
    data->hBitmap = NULL;

    HDC hScreenDC = GetDC(NULL);

    // Premultiplied 32-bit top-down DIB - D2D's CreateBitmap reads pBits directly.
    BITMAPINFO bmi = {{0}};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bool fromCache = false;
    if (rising) {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        auto it = g_SnapshotCache.find(hWnd);
        if (it != g_SnapshotCache.end()) {
            auto& cached = it->second;
            if (cached.w == w && cached.h == h) {
                data->hBitmap = cached.hBmp;
                data->pBits = cached.pBits;
                cached.hBmp = nullptr;
                fromCache = true;
            }
            DeleteObject(cached.hBmp);
            g_SnapshotCache.erase(it);
        }
    }
    if (!fromCache)
        data->hBitmap = CreateDIBSection(hScreenDC, &bmi, DIB_RGB_COLORS, &data->pBits, NULL, 0);

    if (!data->hBitmap || !data->pBits) {
        ReleaseDC(NULL, hScreenDC);
        if (rising) MacGenieUndoRisingHide(hWnd, originalExStyle, cloakHidden);
        else MacGenieSetDwmTransitions(hWnd, TRUE);
        if (data->hBitmap) DeleteObject(data->hBitmap);
        delete data;
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        g_AnimActive.erase(hWnd);
        return false;
    }

    // Copy a rawW x rawH PrintWindow capture into the w x h snapshot at the
    // extended-frame offset. Potassiumuncher's v1.5 grey-screen fix: PrintWindow
    // (PW_RENDERFULLCONTENT) returns pixels that are ALREADY premultiplied, so pass
    // them through untouched and keep a==0 pixels fully TRANSPARENT. The old code
    // forced every pixel opaque, which flattened acrylic / Mica backdrop regions
    // (alpha 0 or partial) into a solid grey - the "grey top" on translucent
    // windows. With alpha preserved, those regions stay see-through during the
    // genie and the desktop shows through them, matching the window's real look.
    auto CopySnapshot = [&](void* sourceBits) {
        const auto* source = static_cast<const uint32_t*>(sourceBits);
        auto* destination = static_cast<uint32_t*>(data->pBits);
        int x0 = std::max(0, -offsetX), x1 = std::min(w, rawW - offsetX);
        int y0 = std::max(0, -offsetY), y1 = std::min(h, rawH - offsetY);
        if (x0 >= x1 || y0 >= y1) { memset(destination, 0, (size_t)w*h*4); return; }
        if (x0 || y0 || x1 != w || y1 != h) memset(destination, 0, (size_t)w*h*4);
        size_t transparent = 0, count = (size_t)(x1-x0)*(y1-y0);
        for (int y=y0; y<y1; ++y) {
            const uint32_t* src = source + (size_t)(y+offsetY)*rawW + x0+offsetX;
            uint32_t* dst = destination + (size_t)y*w + x0;
            for (int x=0; x<x1-x0; ++x) {
                uint32_t pixel=src[x]; bool zero=(pixel>>24)==0;
                transparent += zero; dst[x]=zero ? 0 : pixel;
            }
        }
        // Preserve the upstream fallback for captures with missing backdrop alpha.
        if (transparent > count/2) for (int y=y0; y<y1; ++y) {
            const uint32_t* src = source + (size_t)(y+offsetY)*rawW + x0+offsetX;
            uint32_t* dst = destination + (size_t)y*w + x0;
            for (int x=0; x<x1-x0; ++x)
                if ((src[x]>>24)==0) dst[x]=src[x] | 0xff000000u;
        }
    };

    // Window-only capture: grab the window into a rawW x rawH DIB via PrintWindow,
    // then CopySnapshot into the extended-frame snapshot. PrintWindow renders only
    // the window's own content, so the taskbar / other windows can never bleed into
    // the animation (Potassiumuncher's taskbar fix) - and it also works on a
    // cloaked window (the rising-no-cache path), where a screen grab would not.
    auto CaptureNow = [&]() -> bool {
        GenieTiming timing{2};
        HDC hTempDC = CreateCompatibleDC(hScreenDC);
        BITMAPINFO bmiTemp = bmi;
        bmiTemp.bmiHeader.biWidth = rawW;
        bmiTemp.bmiHeader.biHeight = -rawH;
        void* pTempBits = nullptr;
        HBITMAP hTempBmp = CreateDIBSection(hScreenDC, &bmiTemp, DIB_RGB_COLORS, &pTempBits, NULL, 0);
        if (!hTempDC || !hTempBmp || !pTempBits) {
            if (hTempBmp) DeleteObject(hTempBmp);
            DeleteDC(hTempDC);
            return false;
        }
        HBITMAP hOldTempBmp = (HBITMAP)SelectObject(hTempDC, hTempBmp);
        BOOL captured = PrintWindow(hWnd, hTempDC, PW_RENDERFULLCONTENT);
        GdiFlush();
        if (captured) CopySnapshot(pTempBits);
        SelectObject(hTempDC, hOldTempBmp);
        DeleteObject(hTempBmp);
        DeleteDC(hTempDC);
        return captured != FALSE;
    };

    bool captureOK = true;
    if (rising) {
        // No cached minimize snapshot (e.g. window was already minimized before the
        // mod loaded): snapshot the freshly-restored (cloaked) window directly.
        if (!fromCache) captureOK = CaptureNow();
    } else {
        // Falling: capture the WINDOW ONLY via PrintWindow (Potassiumuncher's
        // method), NOT a screen BitBlt. A screen grab of the window rect also pulls
        // in whatever is behind / around the window - most visibly the taskbar on
        // maximized / fullscreen apps, which then warps inside the genie. PrintWindow
        // renders just the window's own content, so the taskbar (a separate window)
        // can never bleed in. Tradeoff: DWM acrylic/translucency backdrops aren't
        // part of the window's own render, so translucent windows animate opaque - a
        // documented limitation, far less jarring than the taskbar appearing.
        captureOK = CaptureNow();
        // The render worker transfers this DIB into the cache at completion.
    }

    ReleaseDC(NULL, hScreenDC);

    if (!captureOK) {
        if (rising) MacGenieUndoRisingHide(hWnd, originalExStyle, cloakHidden);
        else MacGenieSetDwmTransitions(hWnd, TRUE);
        DeleteObject(data->hBitmap);
        delete data;
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        g_AnimActive.erase(hWnd);
        return false;
    }

    // For a minimize (normal OR deferred) hold the caller back until the ghost's
    // first frame is composed - otherwise the window (or, on the deferred path, the
    // cloak-hide) would happen a few frames before the genie appears (a visible
    // gap, especially on power-throttled CPUs). Two handles to one event: the
    // thread owns its duplicate, we own and wait on the original.
    HANDLE hFirstShown = NULL;
    if (!rising) {
        hFirstShown = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (hFirstShown &&
            !DuplicateHandle(GetCurrentProcess(), hFirstShown, GetCurrentProcess(),
                             &data->hFirstFrameShown, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            data->hFirstFrameShown = NULL;
        }
    }
    bool waitForFirstFrame = (data->hFirstFrameShown != NULL);

    HANDLE hThread = CreateThread(NULL, 0, MacGenieAnimThreadClassic, data, 0, NULL);
    if (hThread) {
        lifetime.transferred=true;
        CloseHandle(hThread);
        if (hFirstShown) {
            if (waitForFirstFrame) {
                GenieTiming timing{3};
                WaitForSingleObject(hFirstShown, 200);   // capped: worst case = old behavior
            }
            CloseHandle(hFirstShown);
        }
        return true;
    }

    // Thread couldn't start - undo our state so the window isn't left invisible or
    // with transitions permanently disabled.
    if (rising) {
        MacGenieUndoRisingHide(hWnd, data->originalExStyle, data->hiddenByCloak);
    } else {
        MacGenieSetDwmTransitions(hWnd, TRUE);
    }
    if (hFirstShown) CloseHandle(hFirstShown);
    if (data->hFirstFrameShown) CloseHandle(data->hFirstFrameShown);
    DeleteObject(data->hBitmap);
    delete data;
    {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        g_AnimActive.erase(hWnd);
    }
    return false;
}

// -------------------------------------------------------------------------
// Hooks
// -------------------------------------------------------------------------
DWORD WINAPI MacGenieLaunchThread(LPVOID lpParam);   // defined below

// Arm the launch animation: check eligibility, kill DWM transition and hide the
// window (alpha 0) *before* the real show call, so the system's open animation
// never appears. Writes the TRUE pre-hide extended style to *origExOut.
static bool MacGenieLaunchPrepare(HWND hWnd, int nCmdShow, LONG_PTR* origExOut) {
    if (g_unloading.load(std::memory_order_relaxed)) return false;
    if (!g_launchAnimation.load(std::memory_order_relaxed)) return false;
    if (nCmdShow != SW_SHOW && nCmdShow != SW_SHOWNORMAL &&
        nCmdShow != SW_SHOWDEFAULT && nCmdShow != SW_SHOWMAXIMIZED) return false;
    if (IsWindowVisible(hWnd) || IsIconic(hWnd)) return false;
    if (!MacGenieIsLaunchWindow(hWnd)) return false;
    {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        if (!g_LaunchSeen.insert(hWnd).second) return false;
    }
    MacGenieSetDwmTransitions(hWnd, FALSE);
    LONG_PTR exStyle = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
    *origExOut = exStyle;
    SetWindowLongPtrW(hWnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hWnd, 0, 0, LWA_ALPHA);
    return true;
}

// Second half of the launch sequence: spawn the reveal worker (or roll back).
static void MacGenieLaunchCommit(HWND hWnd, LONG_PTR originalExStyle) {
    bool reserved;
    {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        reserved=!g_unloading.load(std::memory_order_relaxed);
        if(reserved)g_workerCount.fetch_add(1,std::memory_order_relaxed);
    }
    if(!reserved){MacGenieUndoRisingHide(hWnd,originalExStyle,FALSE);return;}
    MacGenieLaunchData* ld = new MacGenieLaunchData();
    ld->hWnd = hWnd;
    ld->originalExStyle = originalExStyle;
    HANDLE h = CreateThread(NULL, 0, MacGenieLaunchThread, ld, 0, NULL);
    if (h) {
        CloseHandle(h);
    } else {
        g_workerCount.fetch_sub(1, std::memory_order_release);
        delete ld;
        SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA);
        if (!(originalExStyle & WS_EX_LAYERED)) {
            SetWindowLongPtrW(hWnd, GWL_EXSTYLE, originalExStyle);
        }
        MacGenieSetDwmTransitions(hWnd, TRUE);
    }
}

BOOL WINAPI ShowWindow_Hook(HWND hWnd, int nCmdShow) {
    if (nCmdShow == SW_MINIMIZE || nCmdShow == SW_SHOWMINIMIZED || nCmdShow == SW_SHOWMINNOACTIVE) {
        if (MacGenieShouldAnimate(hWnd)) {
            MacGenieSetDwmTransitions(hWnd, FALSE);
            StartMacGenieAnim(hWnd, FALSE, GetWindowLongPtrW(hWnd, GWL_EXSTYLE));
        }
        return ShowWindow_Original(hWnd, nCmdShow);
    }

    if ((nCmdShow == SW_RESTORE || nCmdShow == SW_SHOWNORMAL) && IsIconic(hWnd)) {
        if (g_openAnimation.load(std::memory_order_relaxed) && MacGenieShouldAnimate(hWnd)) {
            MacGenieSetDwmTransitions(hWnd, FALSE);
            // Cloak (frame-level hide, no layered-surface rebuild flash) - MINE.
            MacGenieSetCloak(hWnd, TRUE);
            BOOL res = ShowWindow_Original(hWnd, nCmdShow);
            StartMacGenieAnim(hWnd, TRUE, GetWindowLongPtrW(hWnd, GWL_EXSTYLE), TRUE);
            return res;
        }
        return ShowWindow_Original(hWnd, nCmdShow);
    }

    LONG_PTR launchOrigEx;
    if (MacGenieLaunchPrepare(hWnd, nCmdShow, &launchOrigEx)) {
        BOOL res = ShowWindow_Original(hWnd, nCmdShow);
        MacGenieLaunchCommit(hWnd, launchOrigEx);
        return res;
    }

    return ShowWindow_Original(hWnd, nCmdShow);
}

LRESULT WINAPI DefWindowProcW_Hook(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    if(Msg==WM_ACTIVATE && LOWORD(wParam)!=WA_INACTIVE)WarmTaskbarTarget(hWnd);
    if (Msg == WM_DESTROY) {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        auto it = g_SnapshotCache.find(hWnd);
        if (it != g_SnapshotCache.end()) {
            DeleteObject(it->second.hBmp);
            g_SnapshotCache.erase(it);
        }
        g_IconPositions.erase(hWnd);
        g_TargetRequested.erase(hWnd);
        g_LaunchSeen.erase(hWnd);
    }

    if (Msg == WM_SYSCOMMAND) {
        UINT cmd = wParam & 0xFFF0;
        if (cmd == SC_MINIMIZE) {
            // Our own deferred minimize (auto-hide path) re-enters here via
            // SendMessageTimeoutW - let it pass straight through, no recursion.
            if (GetPropW(hWnd, L"GenieBypass")) {
                return DefWindowProcW_Original(hWnd, Msg, wParam, lParam);
            }
            if (MacGenieShouldAnimate(hWnd)) {
                // Auto-hide taskbar? Reveal it, defer the real minimize until the
                // animation finishes so it doesn't slide away mid-genie.
                // (Potassiumuncher's engine, reconciled with MINE's cloak hide.)
                BOOL requestedUnhide = FALSE;
                HWND hNext = NULL;
                if (g_unhideEnabled.load(std::memory_order_relaxed)) {
                    HWND hTray = FindTaskbarForMonitor(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST));
                    if (hTray) {
                        APPBARDATA abd = { sizeof(APPBARDATA) };
                        abd.hWnd = hTray;
                        UINT uState = (UINT)SHAppBarMessage(ABM_GETSTATE, &abd);
                        if (uState & ABS_AUTOHIDE) {
                            requestedUnhide = TRUE;
                            HWND hwndIter = GetWindow(hWnd, GW_HWNDNEXT);
                            while (hwndIter) {
                                if (IsWindowVisible(hwndIter) && !IsIconic(hwndIter) &&
                                    GetAncestor(hwndIter, GA_ROOT) == hwndIter &&
                                    GetWindowTextLengthW(hwndIter) > 0 &&
                                    hwndIter != hWnd && hwndIter != hTray) {
                                    DWORD exStyle = GetWindowLongPtrW(hwndIter, GWL_EXSTYLE);
                                    if (!(exStyle & WS_EX_TOOLWINDOW)) { hNext = hwndIter; break; }
                                }
                                hwndIter = GetWindow(hwndIter, GW_HWNDNEXT);
                            }
                            SetForegroundWindow(hTray);
                        }
                    }
                }

                MacGenieSetDwmTransitions(hWnd, FALSE);
                if (requestedUnhide) {
                    // Deferred path: animate, then (in the worker) cloak-hide is
                    // applied by us here after the first frame, and the worker does
                    // the real minimize + focus juggle at the end.
                    bool ok = StartMacGenieAnim(hWnd, FALSE, GetWindowLongPtrW(hWnd, GWL_EXSTYLE),
                                                FALSE, TRUE, TRUE, hNext);
                    if (ok) {
                        // First frame is up (StartMacGenieAnim waited): hide the real
                        // window with a flash-free cloak; worker minimizes it later.
                        MacGenieSetCloak(hWnd, TRUE);
                        return 0;
                    }
                    // Spawn failed: fall back to a normal immediate minimize.
                    return DefWindowProcW_Original(hWnd, Msg, wParam, lParam);
                }
                // Normal minimize (MINE): first-frame gate, then real minimize.
                StartMacGenieAnim(hWnd, FALSE, GetWindowLongPtrW(hWnd, GWL_EXSTYLE));
            }
            return DefWindowProcW_Original(hWnd, Msg, wParam, lParam);
        }
        else if (cmd == SC_RESTORE && IsIconic(hWnd) &&
                 g_openAnimation.load(std::memory_order_relaxed) &&
                 MacGenieShouldAnimate(hWnd)) {
            MacGenieSetDwmTransitions(hWnd, FALSE);
            MacGenieSetCloak(hWnd, TRUE);
            LRESULT res = DefWindowProcW_Original(hWnd, Msg, wParam, lParam);
            StartMacGenieAnim(hWnd, TRUE, GetWindowLongPtrW(hWnd, GWL_EXSTYLE), TRUE);
            return res;
        }
    }
    return DefWindowProcW_Original(hWnd, Msg, wParam, lParam);
}

// Custom-title-bar apps (Zed/GPUI, some Electron) minimize via ShowWindowAsync,
// SetWindowPlacement, or CloseWindow. Shared minimize kick-off for those paths (no
// unhide handling - that lives on the SC_MINIMIZE path only).
static void MacGenieTryMinimizeAnim(HWND hWnd) {
    if (!IsWindowVisible(hWnd) || IsIconic(hWnd)) return;
    if (!MacGenieShouldAnimate(hWnd)) return;
    MacGenieSetDwmTransitions(hWnd, FALSE);
    StartMacGenieAnim(hWnd, FALSE, GetWindowLongPtrW(hWnd, GWL_EXSTYLE));
}

BOOL WINAPI ShowWindowAsync_Hook(HWND hWnd, int nCmdShow) {
    if (nCmdShow == SW_MINIMIZE || nCmdShow == SW_SHOWMINIMIZED ||
        nCmdShow == SW_SHOWMINNOACTIVE) {
        MacGenieTryMinimizeAnim(hWnd);
    } else {
        LONG_PTR launchOrigEx;
        if (MacGenieLaunchPrepare(hWnd, nCmdShow, &launchOrigEx)) {
            BOOL res = ShowWindowAsync_Original(hWnd, nCmdShow);
            MacGenieLaunchCommit(hWnd, launchOrigEx);
            return res;
        }
    }
    return ShowWindowAsync_Original(hWnd, nCmdShow);
}

BOOL WINAPI SetWindowPlacement_Hook(HWND hWnd, const WINDOWPLACEMENT* lpwndpl) {
    if (lpwndpl && (lpwndpl->showCmd == SW_MINIMIZE ||
                    lpwndpl->showCmd == SW_SHOWMINIMIZED ||
                    lpwndpl->showCmd == SW_SHOWMINNOACTIVE)) {
        MacGenieTryMinimizeAnim(hWnd);
    }
    return SetWindowPlacement_Original(hWnd, lpwndpl);
}

BOOL WINAPI CloseWindow_Hook(HWND hWnd) {
    MacGenieTryMinimizeAnim(hWnd);
    return CloseWindow_Original(hWnd);
}

// Store / UWP apps show their first window via SetWindowPos + SWP_SHOWWINDOW.
BOOL WINAPI SetWindowPos_Hook(HWND hWnd, HWND hWndInsertAfter, int X, int Y,
                              int cx, int cy, UINT uFlags) {
    if (uFlags & SWP_SHOWWINDOW) {
        LONG_PTR launchOrigEx;
        if (MacGenieLaunchPrepare(hWnd, SW_SHOW, &launchOrigEx)) {
            BOOL res = SetWindowPos_Original(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
            MacGenieLaunchCommit(hWnd, launchOrigEx);
            return res;
        }
    }
    return SetWindowPos_Original(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

// -------------------------------------------------------------------------
// App-launch animation (experimental)
// -------------------------------------------------------------------------
DWORD WINAPI MacGenieLaunchThread(LPVOID lpParam) {
    MacGenieLaunchData* ld = (MacGenieLaunchData*)lpParam;
    HWND hWnd = ld->hWnd;
    LONG_PTR originalExStyle = ld->originalExStyle;
    delete ld;

    // Let the freshly-shown (still hidden) window paint; UWP apps stay DWM-cloaked
    // behind their splash for a while - wait (bounded) for the cloak to lift.
    Sleep(60);
    for (int i = 0; i < 30; ++i) {
        if (!IsWindow(hWnd) || g_unloading.load(std::memory_order_relaxed)) break;
        UINT cloaked = 0;
        if (FAILED(DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) ||
            !cloaked) {
            break;
        }
        Sleep(50);
    }

    if (g_unloading.load(std::memory_order_relaxed) ||
        !IsWindow(hWnd) || IsIconic(hWnd) || !IsWindowVisible(hWnd)) {
        if (IsWindow(hWnd)) {
            SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA);
            if (!(originalExStyle & WS_EX_LAYERED)) {
                SetWindowLongPtrW(hWnd, GWL_EXSTYLE, originalExStyle);
            }
            MacGenieSetDwmTransitions(hWnd, TRUE);
        }
        g_workerCount.fetch_sub(1, std::memory_order_release);
        return 0;
    }

    // Rising genie; reveals the real window. (Launch uses layered+alpha hide, not
    // cloak - cloakHidden defaults FALSE - so the UWP uncloak-wait above still works.)
    StartMacGenieAnim(hWnd, TRUE, originalExStyle);
    g_workerCount.fetch_sub(1, std::memory_order_release);
    return 0;
}

BOOL Wh_ModInit() {
    MacGenieLoadSettings();
    HWND foreground=GetForegroundWindow();DWORD owner=0;
    GetWindowThreadProcessId(foreground,&owner);
    if(owner==GetCurrentProcessId())WarmTaskbarTarget(foreground);

    Wh_SetFunctionHook((void*)DefWindowProcW, (void*)DefWindowProcW_Hook, (void**)&DefWindowProcW_Original);
    Wh_SetFunctionHook((void*)ShowWindow, (void*)ShowWindow_Hook, (void**)&ShowWindow_Original);
    Wh_SetFunctionHook((void*)ShowWindowAsync, (void*)ShowWindowAsync_Hook, (void**)&ShowWindowAsync_Original);
    Wh_SetFunctionHook((void*)SetWindowPlacement, (void*)SetWindowPlacement_Hook, (void**)&SetWindowPlacement_Original);
    Wh_SetFunctionHook((void*)CloseWindow, (void*)CloseWindow_Hook, (void**)&CloseWindow_Original);
    Wh_SetFunctionHook((void*)SetWindowPos, (void*)SetWindowPos_Hook, (void**)&SetWindowPos_Original);
    return TRUE;
}

void Wh_ModSettingsChanged() {
    MacGenieLoadSettings();
}

// Windhawk unmaps the mod DLL right after Wh_ModUninit returns, so any worker
// thread still running mod code would crash its host. Signal workers to abort and
// wait for them to drain (MINE's drain model - the animation loop checks the flag
// every frame, the launch worker every 50 ms; the cap only guards a hung DWM).
void Wh_ModBeforeUninit() {
    {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        g_unloading.store(true,std::memory_order_release);
        g_TargetQueue.clear();
    }
    while (g_workerCount.load(std::memory_order_acquire) > 0) {
        Sleep(10);
    }
}

void Wh_ModUninit() {
    ResetGenieGpu();
    std::lock_guard<std::mutex> lock(g_CacheMutex);
    for (auto& pair : g_SnapshotCache) {
        DeleteObject(pair.second.hBmp);
    }
    g_SnapshotCache.clear();
    g_IconPositions.clear();
    g_ProcessIconPositions.clear();
    g_TargetRequested.clear();g_TargetPending.clear();g_TargetQueue.clear();
    g_LaunchSeen.clear();
}
