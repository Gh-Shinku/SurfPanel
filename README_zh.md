# SurfPanel

SurfPanel 是一个轻量级桌面命令面板，用于快速访问书签和文本片段。它运行在系统托盘中，支持全局热键，并通过简单的 TOML 文件加载项目，无需重新构建即可自定义。

![SurfPanel](./assets/SurfPanel.png)

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

## 配置

SurfPanel 使用单个主 TOML 文件进行日常配置。可选的导入功能可以在需要共享或复用项目组时拉取包文件。

### 配置文件位置

首次启动时，SurfPanel 会将可执行文件旁的 `config/` 目录（本地调试构建时为
相对路径下的 `../config/`）复制到用户配置目录。之后仅从该用户副本读取和写入，
因此应用更新不会覆盖你的配置。实际路径遵循 Qt 为 SurfPanel 提供的用户级
`AppConfigLocation`。

### 目录结构

```
config/
	items.toml
	plugins/
		<plugin-id>.toml
	packages/
		<package-name>/
			items.toml
	cache/
		compiled.toml
```

### 主配置

将你的项目放在 `config/items.toml` 中：

```toml
[[items]]
name = "打开 GitHub"
type = "url"
keywords = ["git", "code"]
[items.payload]
url = "https://github.com"
```

搜索前缀是可选的。默认情况下，`s ` 只搜索片段，`u ` 只搜索 URL。在 `config/items.toml` 中配置 `[search.prefixes]` 来更改这些标记：

```toml
[search.prefixes]
snippet = "s"
url = "u"
```

### 包

要导入共享配置，将其放在 `packages/<name>/` 下，并在 `config/items.toml` 中列出其文件或目录：

```toml
imports = [
	"packages/my_pack/items.toml"
]
```

导入路径必须相对于 `config/` 且不能超出该目录。目录会按文件名顺序加载所有 `.toml` 文件。导入的项目先加载，然后 `config/items.toml` 中的本地 `[[items]]` 后加载，因此本地项目可以覆盖或禁用包中的项目。

禁用一个导入的项目：

```toml
[[items]]
name = "文档"
type = "url"
disabled = true
```

覆盖一个导入的项目：

```toml
[[items]]
name = "打开 GitHub"
type = "url"
keywords = ["git", "code"]
[items.payload]
url = "https://example.com/github"
```

如果给项目添加了可选的 `id` 字段，后续覆盖或禁用时使用相同的 `id`。

### 项目格式

URL 项目：

```toml
[[items]]
name = "文档"
type = "url"
keywords = ["docs"]
[items.payload]
url = "https://example.com/docs"
```

片段项目：

```toml
[[items]]
name = "日期"
type = "snippet"
keywords = ["date"]
[items.payload]
snippet = "{{date}}"
```

### 重新加载配置

使用托盘菜单中的 **Reload Config** 选项重新读取 `config/items.toml` 及其导入，无需重启应用。

### 可选 PDF 剪贴板过滤器

插件是静态链接到应用中的一等模块。每个插件使用独立的
`config/plugins/<plugin-id>.toml`；某个插件配置无效时只会禁用该插件，
不会阻止命令面板或其他插件启动。

Windows 剪贴板过滤器（`clipboard-filter`）默认关闭。若要监听从 SumatraPDF
复制的文本，请编辑 `config/plugins/clipboard-filter.toml` 并重新加载配置：

```toml
enabled = true
source_processes = ["SumatraPDF.exe"]
```

仅处理由配置来源进程拥有的 Unicode 文本。PDF 转换器会合并段落内的单换行、
保留空行表示的段落边界、在下一行以小写字母开头时移除英文行尾断词连字符，
并清理中日韩文字与拉丁字母或数字之间的异常空格。转换后的写入仅保留
Unicode 文本。若希望 Ditto 捕获 SurfPanel 处理后的写入，请在 Ditto 中单独
排除 `SumatraPDF.exe`，避免捕获原始内容。

为兼容旧配置，当独立插件配置文件不存在时，仍会读取 `items.toml` 中的
`[clipboard_filter]`。该旧格式会产生弃用警告；独立插件配置文件始终优先。

### 插件架构

内置插件实现 `src/plugin/` 下带版本号的统一接口，通过内置注册表加入运行时，
并由插件管理器统一处理配置、启动、停止、日志和原生事件。具体插件位于
`src/plugins/<plugin-id>/`。SurfPanel 当前不动态加载第三方 DLL，因此扩展模型
演进期间无需维护公开二进制 ABI，同时保留类型安全的边界。

### 实时变量

URL 和片段负载可以包含实时变量。SurfPanel 在激活项目时根据本地系统时间解析这些变量：

- `{{date}}`：当前日期 `yyyy/MM/dd`
- `{{time}}`：当前时间 `HH:mm:ss`
- `{{datetime}}`：当前日期和时间 `yyyy/MM/dd HH:mm:ss`

未知变量保持不变。

### 回退行为

如果配置加载失败，SurfPanel 会回退到 `cache/compiled.toml`（如果存在）。即使当前配置文件存在语法错误，这也能让应用在成功加载后继续可用。

## 开源声明

SurfPanel 使用了以下开源库：

- Qt6 作为应用框架和 UI
- [toml11](https://github.com/ToruNiina/toml11) 用于 TOML 解析和配置加载
