# SurfPanel

## Configuration

SurfPanel uses a component-based configuration system inspired by RIME:
profiles select modules, packages provide shareable modules, and user patches
or overrides customize behavior without editing shared files.

### Config Location

SurfPanel searches for the config root in this order:

1. `SURFPANEL_CONFIG_DIR` environment variable (if set)
2. `config/` in the current working directory
3. `config/` next to the executable

### Directory Layout

```
config/
	defaults/
		items.toml
	profiles/
		default.profile.toml
	packages/
		<package-name>/
			schema/
				items.toml
			manifest.toml
	user/
		patches/
		overrides/
	cache/
		compiled.toml
		last_good.toml
	test.toml (legacy single-file config)
```

### Profile Files

`profiles/default.profile.toml` lists the module sources to load in order.
Sources can be files or directories (directories load all `.toml` files).

```
name = "Default"
sources = [
	"defaults/items.toml",
	"packages/my_pack/schema/items.toml"
]
```

### Packages

To import a shared configuration, drop it under `packages/<name>/` and add
its module paths to your profile. `manifest.toml` is optional metadata; the
loader only requires the module paths. This keeps shared config separate from
your personal changes.

### Patches and Overrides

Files in `user/patches/` are applied after profile sources, and
`user/overrides/` are applied last. Later entries override earlier ones.

Disable an existing item:

```
[[items]]
name = "Docs"
type = "url"
disabled = true
```

If you add an optional `id` field to an item, use the same `id` when
overriding or disabling it later.

Override an existing item:

```
[[items]]
name = "Open GitHub"
type = "url"
keywords = ["git", "code"]
[items.payload]
url = "https://example.com/github"
```

### Reloading Config

Use the tray menu option **Reload Config** to re-read the profile, packages,
and user overrides without restarting the app.

### Fallback Behavior

If config loading fails, SurfPanel falls back to `cache/last_good.toml`, then
to `defaults/items.toml`, and finally to `test.toml` if present. This keeps the
app usable even if a module has syntax errors.

## Open Source Notice

SurfPanel uses the following open-source libraries:

- Qt6 for the application framework and UI.
- [toml11](https://github.com/ToruNiina/toml11) for TOML parsing and configuration loading.
