# SurfPanel contributor guide

## Stack and commands

- C++17 Qt6 Widgets desktop application, built with CMake and Ninja.
- `toml11` is a git submodule dependency; keep third-party code untouched unless
  explicitly updating it.
- Configure with `make cg-debug` or `make cg-release`. Pass local CMake options
  without committing them, e.g. `make cg-debug CMAKE_ARGS='-DCMAKE_PREFIX_PATH=/path/to/Qt6'`.
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

## Commits

- Follow the existing format: `[type] concise imperative summary`.
- Common types: `feat`, `fix`, `docs`, `chore`, `refactor`, `test`, `clear`;
  scoped types such as `[feat/ui]` are acceptable.
- Keep each commit to one coherent change and avoid machine-specific paths,
  credentials, or generated files.
