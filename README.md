# SeewoPenTweaker

一个运行在 Windows 系统托盘中的快捷键鼠标转换工具。

## 功能

- `Ctrl+Shift+Alt+P`：等待 300ms 后按住鼠标左键
- `Ctrl+Shift+Alt+Q`：释放鼠标左键
- 在 `P` 后 200ms 内按 `Q`：释放左键并执行一次鼠标右键点击
- 通过系统托盘菜单退出程序

## 使用

1. 从 Release 下载 `SeewoPenTweaker.exe`。
2. 运行程序，图标会出现在系统托盘中。
3. 使用上述快捷键操作。
4. 右键点击托盘图标并选择“退出”关闭程序。

如果快捷键注册失败，通常是因为相同快捷键已被其他程序占用。

## 本地编译

需要安装 .NET SDK 10.0 或更高版本：

```powershell
dotnet build .\SeewoPenTweaker.csproj
```

生成单文件 Windows 可执行文件：

```powershell
dotnet publish .\SeewoPenTweaker.csproj -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true
```

发布文件位于：

```text
bin\Release\net10.0-windows\win-x64\publish\SeewoPenTweaker.exe
```

## 技术栈

- C#
- WinForms
- Windows `RegisterHotKey`
- Windows `mouse_event`
