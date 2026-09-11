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

## Code conventions

- Keep code portable where practical; isolate Windows-specific code behind
  `Q_OS_WIN`.
- Use C++17 and Qt value types. Keep UI, platform integration, and business
  logic separated.
- Run the repository's clang-format configuration on edited C++ files.
- Add or update focused tests under `tests/` for behavior changes. Do not commit
  generated `build/`, `Output/`, cache, or local configuration artifacts.

## Plugin conventions

- Treat built-in plugins as first-class modules under `src/plugins/<plugin-id>/`.
  IDs must be stable lowercase ASCII names containing only letters, digits, and
  hyphens.
- Implement `IPlugin`, register through `src/plugin/builtin_plugins.cpp`, and
  keep plugin-specific configuration in `config/plugins/<plugin-id>.toml`.
- Keep plugin lifecycle methods idempotent. A plugin must release native
  resources and invalidate pending asynchronous work in `stop()`.
- Use `PluginHostContext` for host services and native-event delivery. Do not
  couple plugin implementations directly to `MainWindow`.
- Invalid plugin configuration must disable only that plugin. Cover parsing,
  lifecycle, and isolation behavior with focused tests.

## Commits

- Follow the existing format: `[type] concise imperative summary`.
- Common types: `feat`, `fix`, `docs`, `chore`, `refactor`, `test`, `clear`;
  scoped types such as `[feat/ui]` are acceptable.
- Keep each commit to one coherent change and avoid machine-specific paths,
  credentials, or generated files.
