# 配置指南

[English](configuration.md) · [返回首页](../README_zh.md)

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
Unicode 文本。匹配来源的文本即使无需修改，也会由 SurfPanel 原样写回一次，
确保单行复制同样能进入剪贴板历史。若希望 Ditto 捕获 SurfPanel 处理后的写入，请在 Ditto 中单独
排除 `SumatraPDF.exe`，避免捕获原始内容。

为兼容旧配置，当独立插件配置文件不存在时，仍会读取 `items.toml` 中的
`[clipboard_filter]`。该旧格式会产生弃用警告；独立插件配置文件始终优先。

### 调用插件功能

通过原有配置入口添加插件 item，名称和搜索关键词可以自由修改：

```toml
[[items]]
name = "过滤剪贴板"
type = "plugin"
keywords = ["filter", "clipboard", "剪贴板", "过滤"]
[items.payload]
plugin = "clipboard-filter"
function = "filter"
```

`plugin` 使用稳定插件 ID，而非显示名称；`function` 使用公开的功能名。
两个字段必须为非空字符串，精确匹配，与搜索 `keywords` 无关。
插件 item 同样支持配置导入、覆盖、禁用、缓存回退和最近使用。
如需使用 `p ` 只搜索插件，可在 `[search.prefixes]` 中配置 `plugin = "p"`。

| 插件 ID | 功能名 | 行为 | 平台 |
| --- | --- | --- | --- |
| `clipboard-filter` | `filter` | 使用现有 PDF 文本规则过滤当前剪贴板，并以 SurfPanel 的 owner 写回。 | Windows |

`filter` 接受任意来源的 Unicode 文本，即使自动监听关闭、配置缺失或无效也可调用。
它只读取当前系统剪贴板，不读取 Ditto 历史，也不自动粘贴。无需修改的内容原样写回一次。
空文本或非文本会提示失败。遇到短暂占用时异步重试；期间出现新的复制则取消，
避免覆盖新内容。配置 reload 或退出会取消未完成任务。只有成功执行才记录最近使用。

默认配置已包含该 item。已有安装会保留用户配置，请手动添加上述示例并 reload。

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

在名称后加冒号，可只对当前这处使用自定义格式：

```toml
[items.payload]
snippet = "{{date:yyyy-MM-dd}}"        # 2026-05-17
# 其他可选写法（每个 payload 只设置一个 snippet）：
# snippet = "{{date:yyyy年M月d日 dddd}}"  # 2026年5月17日 星期日
# snippet = "{{datetime:yyyy-MM-dd'T'HH:mm}}"
```

格式串取第一个冒号之后的全部内容，所以时间字段不受影响：`{{time:HH:mm}}`。
不带格式的变量使用下面配置的默认值。

若想修改所有项目的默认格式，在 `items.toml` 中加入 `[datetime]` 表，
并通过托盘菜单的 Reload Config 重新加载：

```toml
[datetime]
date_format = "yyyy-MM-dd"
time_format = "HH:mm"
datetime_format = "yyyy-MM-dd HH:mm:ss"
```

格式使用 Qt 的日期时间语法，而不是 `strftime`：`yyyy` 年、`MM` 月、`dd` 日、
`HH` 时、`mm` 分、`ss` 秒、`dddd` 星期名、`MMM`/`MMMM` 月名。需要原样输出的
字母用单引号包裹，例如 `yyyy-MM-dd'T'HH:mm`。星期名和月名跟随系统语言，
数字字段与语言无关。

常见错误会在加载配置时给出警告，并回退到默认格式：`%Y` 这类 `strftime`
占位符、不含任何日期时间字段的格式串、空值。像 `yyyy-mm-dd` 这样的格式会被
接受但给出警告，因为小写 `m` 表示分钟。

未知变量保持不变。

### 回退行为

如果配置加载失败，SurfPanel 会回退到 `cache/compiled.toml`（如果存在）。即使当前配置文件存在语法错误，这也能让应用在成功加载后继续可用。
