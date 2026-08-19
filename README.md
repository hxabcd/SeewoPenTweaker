# SeewoPenTweaker

一个运行在 Windows 系统托盘中的快捷键鼠标转换工具。

[下载](https://github.com/hxabcd/SeewoPenTweaker/releases/latest/download/SeewoPenTweaker.exe) | [镜像下载](https://ghproxy.net/https://github.com/hxabcd/SeewoPenTweaker/releases/latest/download/SeewoPenTweaker.exe)

## 功能

- `Ctrl+Shift+Alt+P`：等待 200ms 后按住鼠标左键
- `Ctrl+Shift+Alt+Q`：释放鼠标左键
- 在 `P` 后 200ms 内按 `Q`：释放左键并执行一次鼠标右键点击
- 通过系统托盘菜单退出程序
- 防止程序重复启动
- 支持设置开机启动
- 支持手动检查更新和自动检查更新提醒
- 启动失败时显示具体错误信息

## 使用

1. 从 Release 下载 `SeewoPenTweaker.exe`。
2. 运行程序，图标会出现在系统托盘中。
3. 使用上述快捷键操作。
4. 右键点击托盘图标并选择“退出”关闭程序。

如果快捷键注册失败，通常是因为相同快捷键已被其他程序占用。

“自动检查更新”默认关闭。开启后，程序启动约 5 秒后检查一次 GitHub 最新 Release；发现新版本时只弹出提醒，不会自动下载或替换程序。

## 系统兼容性

- Windows 7：使用系统 DPI 感知回退，支持基本功能。
- Windows 10 及 Windows 11：支持 Per-Monitor V2 高 DPI 模式。
- 当前发布版本为 x64 架构。

## 本地编译

需要安装 MinGW-w64 G++，然后运行：

```powershell
.\build.ps1
```

也可以手动执行：

```powershell
New-Item -ItemType Directory -Force .\bin\Release\native-cpp | Out-Null
windres .\SeewoPenTweaker.rc -O coff -o .\bin\Release\native-cpp\SeewoPenTweaker-res.o
g++ .\SeewoPenTweaker.cpp .\Config.cpp .\SettingsWindow.cpp .\AboutWindow.cpp .\bin\Release\native-cpp\SeewoPenTweaker-res.o -o .\bin\Release\native-cpp\SeewoPenTweaker.exe -std=c++17 -mwindows -municode -O2 -s -static -static-libgcc -static-libstdc++ -luser32 -lshell32 -ladvapi32 -lwinhttp
```

生成的原生 Windows EXE 不需要安装 .NET 或其他运行时，体积约为 217KB。

托盘图标使用 DPI 适配尺寸加载；程序内嵌 Per-Monitor V2 DPI 清单，并在显示菜单时设置对应的线程 DPI 上下文。

托盘图标使用 `SeewoPenTweaker.ico`，包含多个尺寸，图案为蓝底白色三栏。

## 技术栈

- C++
- Win32 API
- Windows `RegisterHotKey`
- Windows `SendInput`
