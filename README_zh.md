# SurfPanel

SurfPanel 是一个轻量级桌面命令面板，用于快速访问书签和文本片段。它运行在系统托盘中，支持全局热键，并通过简单的 TOML 文件加载项目，无需重新构建即可自定义。

![SurfPanel](./assets/SurfPanel.png)

## 配置

SurfPanel 使用单个主 TOML 文件进行日常配置。可选的导入功能可以在需要共享或复用项目组时拉取包文件。

### 配置文件位置

SurfPanel 按以下顺序搜索配置根目录：

1. 可执行文件旁边的 `config/` 目录
2. 可执行文件相对路径下的 `../config/` 目录，用于本地调试构建

### 目录结构

```
config/
	items.toml
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

导入路径相对于 `config/`。目录会按文件名顺序加载所有 `.toml` 文件。导入的项目先加载，然后 `config/items.toml` 中的本地 `[[items]]` 后加载，因此本地项目可以覆盖或禁用包中的项目。

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
