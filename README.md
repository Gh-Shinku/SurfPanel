# SurfPanel

[**中文**](./README_zh.md)

SurfPanel is a lightweight desktop command palette for quick access to
bookmarks and text snippets. It runs in the system tray, supports a global
hotkey, and loads items from simple TOML files so you can customize it without
rebuilding.

![SurfPanel](./assets/SurfPanel.png)

## Development Requirements

Install Qt6 (Core and Widgets) and Ninja, then set `SURFPANEL_QT_ROOT` to the
Qt installation prefix—the directory that contains `lib/cmake/Qt6/Qt6Config.cmake`.
CMake deliberately stops with a clear error when this variable is absent or
invalid.

PowerShell, for the current terminal:

```powershell
$env:SURFPANEL_QT_ROOT = "C:\Qt\6.8.0\mingw_64"
```

To persist it for future terminals on Windows:

```powershell
[Environment]::SetEnvironmentVariable("SURFPANEL_QT_ROOT", "C:\Qt\6.8.0\mingw_64", "User")
```

To build the Windows installer, also set `SURFPANEL_MINGW_ROOT` to the MinGW
prefix containing `bin/libstdc++-6.dll`. `pack.bat` uses the standard Inno
Setup 6 location by default; set `SURFPANEL_ISCC` to the full path of
`ISCC.exe` when it is installed elsewhere.

The installer performs in-place upgrades using the stable Inno Setup `AppId`.
If SurfPanel is running, Windows Restart Manager lists it on the Preparing to
Install page and offers to close it automatically before files are replaced.
After an upgrade, Restart Manager starts the updated daemon again; on a fresh
install, the Finish page offers to launch it.

## Palette Appearance

On Windows 11 22H2 and later, the palette uses the system Desktop Acrylic
backdrop, small rounded corners, and DWM framing/shadow. Older Windows versions
and unavailable native effects use a simple translucent panel and thin border.
No wallpaper sampling or Qt drop-shadow effect is used.

The palette is 660 logical pixels wide and opens on the screen containing the
cursor, horizontally centered with its top edge around 35% of the available
screen height. Height follows the results, displaying up to six 44-pixel rows
before scrolling; empty results show an informational, non-actionable row.
Type icons and `Link`, `Snippet`, and `Plugin` labels identify actions.

Light/dark appearance follows Qt's system color scheme, with a compatibility
fallback for older Qt versions. The Windows accent marks search focus and the
selected row. A 90 ms, 4-pixel opening animation follows the Windows animation
setting and never delays keyboard input.

For UI verification, set `SURFPANEL_UI_CAPTURE_DIR` to a build-artifact directory
and run `test_mainwindow` through CTest. It exports light/dark native and fallback
previews; normal test runs do not capture the desktop.

## Configuration

SurfPanel uses a single main TOML file for everyday configuration. Optional
imports can pull in package files when you want to share or reuse item groups.

### Config Location

On first launch, SurfPanel copies the default `config/` next to the executable
(or `../config/` for local debug builds) into its user configuration directory.
It subsequently reads and writes only that user copy, so application updates do
not overwrite your configuration. The exact location follows Qt's per-user
`AppConfigLocation` for SurfPanel.

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

### Reloading Config

Use the tray menu option **Reload Config** to re-read `config/items.toml` and
its imports without restarting the app.

### Optional PDF Clipboard Filter

Plugins are first-class, statically linked application modules. Each plugin has
its own `config/plugins/<plugin-id>.toml`; one plugin's invalid configuration
disables only that plugin, without preventing the command palette or other
plugins from starting.

The Windows clipboard filter (`clipboard-filter`) is disabled by default. To
monitor text copied from SumatraPDF, edit
`config/plugins/clipboard-filter.toml` and reload config:

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

The bundled configuration includes this item. Existing installations retain
their user configuration; add the example manually and reload config. Failed
calls show a tray notification and are not added to recently used items.

### Plugin Architecture

Built-in plugins implement the versioned interface under `src/plugin/`, are
registered in the built-in registry, and are managed uniformly for
configuration, startup, shutdown, logging, and native events. Plugin
implementations live under `src/plugins/<plugin-id>/`. SurfPanel does not load
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
snippet = "{{date:yyyy年M月d日 dddd}}" # 2026年5月17日 星期日
snippet = "{{datetime:yyyy-MM-dd'T'HH:mm}}"
```

The pattern is everything after the first colon, so time fields are fine:
`{{time:HH:mm}}`. Bare variables keep the configured defaults below.

To change the defaults for every item, add a `[datetime]` table to
`items.toml` and reload the config from the tray menu:

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

## Open Source Notice

SurfPanel uses the following open-source libraries:

- Qt6 for the application framework and UI.
- [toml11](https://github.com/ToruNiina/toml11) for TOML parsing and configuration loading.
