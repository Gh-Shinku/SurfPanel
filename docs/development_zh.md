# 开发指南

[English](development.md) · [返回首页](../README_zh.md)

## 开发环境要求

安装 Qt6（Core 和 Widgets）与 Ninja，并将 `SURFPANEL_QT_ROOT` 设置为 Qt
安装前缀，即包含 `lib/cmake/Qt6/Qt6Config.cmake` 的目录。该变量缺失或路径无效时，
CMake 会明确报错并停止配置。

PowerShell 当前终端示例：

```powershell
$env:SURFPANEL_QT_ROOT = "C:\Qt\6.8.0\mingw_64"
```

在 Windows 中持久写入用户环境变量：

```powershell
[Environment]::SetEnvironmentVariable("SURFPANEL_QT_ROOT", "C:\Qt\6.8.0\mingw_64", "User")
```

构建 Windows 安装包时，还需将 `SURFPANEL_MINGW_ROOT` 设置为包含
`bin/libstdc++-6.dll` 的 MinGW 安装前缀。`pack.bat` 默认使用 Inno Setup 6
的标准安装位置；若安装在其他位置，将 `SURFPANEL_ISCC` 设置为 `ISCC.exe`
的完整路径。

安装程序通过稳定的 Inno Setup `AppId` 执行原地升级。若 SurfPanel 正在运行，
Windows Restart Manager 会在“准备安装”页面列出它，并允许在替换文件前自动
关闭；升级完成后会由 Restart Manager 重新启动守护程序。全新安装则会在完成
页面提供启动选项。

## Palette 外观

Windows 11 22H2 及以上使用系统 Desktop Acrylic 背景、小圆角及 DWM 边框/阴影。
旧版 Windows 或原生效果不可用时使用单一半透明填充和细边框，不采样壁纸，
不使用 Qt drop-shadow。

窗口默认宽 660 个逻辑像素，打开在鼠标所在屏幕，水平居中，顶边约位于可用高度的 35%。
高度随结果数量调整，最多显示六行 44 px 结果，其余通过滚动查看。
没有结果时显示不可执行的提示行；图标与 `Link`、`Snippet`、`Plugin` 标签区分功能类型。

深浅主题优先使用配置中的 `[appearance].theme`，未指定时跟随 Qt 系统 color scheme，
旧版 Qt 使用兼容回退。
Windows accent 仅用于搜索焦点与选中行等小范围提示。
90 ms、4 px 的打开动画尊重系统动画设置，不延迟键盘操作。

UI 验证可设置 `SURFPANEL_UI_CAPTURE_DIR` 为构建产物目录，再通过 CTest 运行
`test_mainwindow`，导出深浅主题的原生与 fallback 预览；常规测试不会截图。

同时导出 `readme-light.png`、`readme-dark.png` 和 `tray.png`。
README 面板展示图使用回退外观和窗口自身渲染，不依赖可截图的桌面会话，
也不包含 DWM 原生效果。检查后可分别复制到 `assets/SurfPanel.png`、
`assets/SurfPanel-dark.png` 和 `assets/tray.png` 来更新 README 图像。

## 构建与测试

配置环境变量后，在仓库根目录执行：

```powershell
git submodule update --init --recursive
make cg-debug
make build
make test
```

构建发布安装包时，使用 `make cg-release` 配置、`make build` 构建，再执行
`make pack`。需要与 Qt 工具链匹配的 C++17 编译器、CMake、Ninja 和 GNU Make。
不要提交 `build/`、`Output/` 等生成文件。

贡献规范和插件约定见 [AGENTS.md](../AGENTS.md)。
