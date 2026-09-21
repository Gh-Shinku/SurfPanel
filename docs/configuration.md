# Configuration

[中文](configuration_zh.md) · [Home](../README.md)

SurfPanel uses a single main TOML file for everyday configuration. Optional
imports can pull in package files when you want to share or reuse item groups.

### Config Location

New installations start with an empty `items.toml` (`items = []`), without
example actions, imports, or enabled plugins. Add the examples below yourself.
More optional examples are under [examples/](examples/README.md).
SurfPanel reads and writes its user configuration directory under Qt's per-user
`AppConfigLocation` for SurfPanel. Application updates never overwrite this
directory, and the installer keeps existing executable-adjacent configuration
without a reset prompt. Legacy executable-adjacent configuration is copied into
the user directory only when that user directory does not yet exist.

### Directory Layout

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

### Main Config

Put your items in `config/items.toml`:

```toml
[[items]]
name = "Open GitHub"
type = "url"
keywords = ["git", "code"]
[items.payload]
url = "https://github.com"
```

Search prefixes are optional. By default, `s ` searches only snippets and
`u ` searches only URLs. Configure `[search.prefixes]` in `config/items.toml`
to change those tokens:

```toml
[search.prefixes]
snippet = "s"
url = "u"
```

### Theme

Set the theme in the main `items.toml` (not an imported package):

```toml
[appearance]
theme = "dark"
```

Values are `"dark"`, `"light"`, or `"system"` (the default). Explicit light/dark
settings override the system theme for both the palette and tray menu. Saving
applies changes automatically without restarting or replacing the native window.
Remove the setting or choose `"system"` to follow system changes again. Invalid
values produce a configuration warning and follow the system theme. Last-good
cache fallback preserves the last successfully loaded theme.

### Packages

To import a shared configuration, drop it under `packages/<name>/` and list
its file or directory in `config/items.toml`:

```toml
imports = [
	"packages/my_pack/items.toml"
]
```

Import paths must be relative to, and remain within, `config/`. Directories load
all `.toml` files in filename order. Imported items load first, then local `[[items]]` in
`config/items.toml` load after them, so local items can override or disable
package items.

Disable an imported item:

```toml
[[items]]
name = "Docs"
type = "url"
disabled = true
```

Override an imported item:

```toml
[[items]]
name = "Open GitHub"
type = "url"
keywords = ["git", "code"]
[items.payload]
url = "https://example.com/github"
```

If you add an optional `id` field to an item, use the same `id` when
overriding or disabling it later.

### Item Format

URL item:

```toml
[[items]]
name = "Docs"
type = "url"
keywords = ["docs"]
[items.payload]
url = "https://example.com/docs"
```

Snippet item:

```toml
[[items]]
name = "Date"
type = "snippet"
keywords = ["date"]
[items.payload]
snippet = "{{date}}"
```

### Automatic Hot Reload and Diagnostics

Saving configuration reloads it automatically; no manual reload menu is needed.
SurfPanel watches source TOML files throughout the user configuration directory,
including imports and plugin files. Changes are debounced for 250 ms, and atomic
editor saves, new files, removals, and recreated directories remain monitored.
Generated `cache/` files and non-TOML temporary files do not trigger reloads.

Logs include timestamps, changed source paths, watch/read failures, parsing and
plugin diagnostics, fallback usage, item counts, and reload duration; no file
contents are logged by the watcher. Logs are written under Qt's `AppDataLocation`
as `log/SurfPanel.log` (normally `%APPDATA%\SurfPanel\log\SurfPanel.log` on Windows).
If configuration is invalid, inspect the diagnostics and correct the file;
saving again reloads it. The last-good cache fallback is described below.

### Optional PDF Clipboard Filter

Plugins are first-class, statically linked application modules. Each plugin has
its own `config/plugins/<plugin-id>.toml`; one plugin's invalid configuration
disables only that plugin, without preventing the command palette or other
plugins from starting.

The Windows clipboard filter (`clipboard-filter`) is disabled by default. To
monitor text copied from SumatraPDF, edit
`config/plugins/clipboard-filter.toml` (create it if missing) and save:

```toml
enabled = true
source_processes = ["SumatraPDF.exe"]
```

Only Unicode text owned by a configured source process is considered. The
PDF transformer joins single line breaks while preserving blank-line paragraph
boundaries, removes English end-of-line hyphenation when the next line starts
with a lowercase letter, and removes unwanted spacing between CJK and Latin
text or numbers. A transformed write replaces the clipboard with Unicode text
only. Matching text is written back once even when normalization leaves it
unchanged, so single-line copies also reach clipboard history under SurfPanel's
ownership. Configure Ditto separately to exclude `SumatraPDF.exe` if you want Ditto
to capture SurfPanel's processed write instead of the original.

For compatibility, an existing `[clipboard_filter]` table in `items.toml` is
still accepted when the dedicated plugin file does not exist. This legacy form
emits a deprecation warning; the dedicated file always takes precedence.

### Calling Plugin Functions

Plugin functions use the same configurable items as URLs and snippets:

```toml
[[items]]
name = "Filter Clipboard"
type = "plugin"
keywords = ["filter", "clipboard", "my-copy-alias"]
[items.payload]
plugin = "clipboard-filter"
function = "filter"
```

`plugin` is the stable plugin ID, not its display name. `function` is the
published function name; both are non-empty strings matched exactly. Customize
`name` and `keywords` freely without changing the target. Imports, overrides,
disabled items, configuration fallback, and recently used items work as usual.
To optionally search only plugin items with `p `, set
`plugin = "p"` under `[search.prefixes]`.

| Plugin ID | Function | Behavior | Platform |
| --- | --- | --- | --- |
| `clipboard-filter` | `filter` | Normalize current Unicode clipboard text and write it back under SurfPanel's ownership. | Windows |

The `filter` function uses the PDF normalization rules described above, accepts
text from any source, and works even when automatic monitoring is disabled or
its configuration is missing or invalid. It reads the current system clipboard,
not Ditto's history, and does not paste into the active application. Unchanged
text is also written back once. Empty or non-text content produces a failure
notification. Brief clipboard contention is retried asynchronously; if another
copy replaces the content, the operation is cancelled rather than overwriting
the new copy. Reloading config or shutting down cancels pending work.

New installations do not include this item. Existing installations retain
their user configuration; add the example manually and save. Failed
calls show a tray notification and are not added to recently used items.

### Plugin Architecture

Built-in plugins implement the versioned interface under `src/plugins/api/`,
are registered through `src/plugins/host/`, and are managed uniformly for
configuration, startup, shutdown, logging, and native events. Plugin
implementations live under `src/plugins/<module_name>/`; source directories use
snake_case while runtime plugin IDs remain kebab-case. SurfPanel does not load
third-party DLLs, so the plugin boundary stays type-safe and avoids a public
binary ABI while the extension model evolves.

### Realtime Variables

URL and snippet payloads can include realtime variables. SurfPanel resolves
these when you activate an item, using the local system time:

- `{{date}}`: current date as `yyyy/MM/dd`
- `{{time}}`: current time as `HH:mm:ss`
- `{{datetime}}`: current date and time as `yyyy/MM/dd HH:mm:ss`

Add a colon to format a single usage with your own pattern:

```toml
[items.payload]
snippet = "{{date:yyyy-MM-dd}}"       # 2026-05-17
# Other alternatives (use one snippet value per payload):
# snippet = "{{date:yyyy年M月d日 dddd}}" # 2026年5月17日 星期日
# snippet = "{{datetime:yyyy-MM-dd'T'HH:mm}}"
```

The pattern is everything after the first colon, so time fields are fine:
`{{time:HH:mm}}`. Bare variables keep the configured defaults below.

To change the defaults for every item, add a `[datetime]` table to
`items.toml` and save:

```toml
[datetime]
date_format = "yyyy-MM-dd"
time_format = "HH:mm"
datetime_format = "yyyy-MM-dd HH:mm:ss"
```

Patterns use Qt date/time syntax, not `strftime`: `yyyy` year, `MM` month,
`dd` day, `HH` hour, `mm` minutes, `ss` seconds, `dddd` weekday name,
`MMM`/`MMMM` month name. Enclose literal letters in single quotes, as in
`yyyy-MM-dd'T'HH:mm`. Weekday and month names follow the system language;
numeric fields are locale independent.

Common mistakes are reported as config warnings and fall back to the default
format: `%Y`-style `strftime` placeholders, patterns without any date or time
field, and empty values. A pattern such as `yyyy-mm-dd` is accepted with a
warning because lowercase `m` means minutes.

Unknown variables are left unchanged.

### Fallback Behavior

If config loading fails, SurfPanel falls back to `cache/compiled.toml` if it
exists. This keeps the app usable after a successful previous load even if the
current config file has syntax errors.
