# SurfPanel contributor guide

## Stack and commands

- C++17 Qt6 Widgets desktop application, built with CMake and Ninja.
- `toml11` is a git submodule dependency; keep third-party code untouched unless
  explicitly updating it.
- Before configuring, set `SURFPANEL_QT_ROOT` to the Qt installation prefix
  containing `lib/cmake/Qt6/Qt6Config.cmake`; CMake fails clearly when it is
  missing or invalid. Configure with `make cg-debug` or `make cg-release`.
- Build with `make build`; run tests with `make test`. CTest configures Qt and
  compiler runtime paths automatically on Windows.
- Packaging requires `SURFPANEL_MINGW_ROOT`; set `SURFPANEL_ISCC` only when
  Inno Setup 6 is not installed in its standard location. Run `make pack` after
  a release build.

## Code conventions

- Keep code portable where practical; isolate Windows-specific code behind
  `Q_OS_WIN`.
- Use C++17 and Qt value types. Keep UI, platform integration, and business
  logic separated.
- Keep production code inside the established `src/app`, `src/core`,
  `src/platform`, `src/plugins`, and `src/ui` modules. Do not add source files
  directly under `src/`.
- Include project headers by their full path relative to `src`, such as
  `core/search/item.h`. Keep dependency flow toward lower layers: app may use
  UI, UI may use core/platform/plugin services, and core must not depend on UI
  or concrete plugins.
- Use lowercase snake_case for new source directories and filenames. Keep
  tests in the matching module below `tests/`; shared test-only code belongs in
  `tests/support/`. Run `make check-layout` after structural changes.
- Run the repository's clang-format configuration on edited C++ files.
- Add or update focused tests under `tests/` for behavior changes. Do not commit
  generated `build/`, `Output/`, cache, or local configuration artifacts.

## Plugin conventions

- Treat built-in plugins as first-class modules under
  `src/plugins/<module_name>/`. Module directories use lowercase snake_case;
  runtime IDs remain stable lowercase ASCII names containing only letters,
  digits, and hyphens.
- Implement `IPlugin` from `src/plugins/api/plugin.h`, register through
  `src/plugins/host/builtin_plugins.cpp`, and keep plugin-specific
  configuration in `config/plugins/<plugin-id>.toml`.
- Keep plugin lifecycle methods idempotent. A plugin must release native
  resources and invalidate pending asynchronous work in `stop()`.
- Use `PluginHostContext` for host services and native-event delivery. Do not
  couple plugin implementations directly to `MainWindow`.
- Invalid plugin configuration must disable only that plugin. Cover parsing,
  lifecycle, and isolation behavior with focused tests.
- Publish callable functions through `IPlugin::functions()` and
  `invokeFunction()`, and document stable plugin IDs and function names in
  `docs/configuration.md` and `docs/configuration_zh.md`. Palette items use
  `type = "plugin"` with `payload.plugin` and
  `payload.function`; names and search keywords remain user-configurable.
- Functions run on the GUI event loop and may finish asynchronously. Complete
  each call once, release resources in `stop()`, and keep manual functions
  independent of automatic-monitoring configuration where applicable.

## Commits

- Follow the existing format: `[type] concise imperative summary`.
- Common types: `feat`, `fix`, `docs`, `chore`, `refactor`, `test`, `clear`;
  scoped types such as `[feat/ui]` are acceptable.
- Keep each commit to one coherent change and avoid machine-specific paths,
  credentials, or generated files.
