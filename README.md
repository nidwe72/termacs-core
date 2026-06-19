# termacs-core

The C++ core of **termacs**: the widget framework, cell-based layout engine,
signals, themes, the [tvision](https://github.com/magiblot/tvision) terminal
backend, and the flat **C ABI** that language bindings link against.

This repo is the **API source of truth** — the full design spec lives here in
[`termacs.md`](termacs.md). termacs-core knows nothing about any binding;
downstream repos ([`termacs-java`](https://github.com/nidwe72/termacs-java),
later `termacs-python` / `termacs-dart`) consume it as a **source dependency**.

## Build & test

```bash
git clone --recursive https://github.com/nidwe72/termacs-core.git   # tvision is a submodule
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure        # tc_snapshot, tc_demo, tc_csmoke
```

Requirements: CMake ≥ 3.16, a C++17 compiler, ncursesw/terminfo at runtime.
(Already cloned without `--recursive`? `git submodule update --init`.)

## Outputs

- `libtermacs.a` — static core (framework + C ABI + tvision)
- `libtermacs.so` — shared C ABI (`tm_*` — what Python/Dart load)
- public headers under `include/termacs/` (`include/termacs/c/termacs.h` is the C ABI)

`cmake --install build` lays out headers + libs for downstream use.

## Consumed downstream

A binding pulls core by tag via CMake `FetchContent` — **source, never a
published binary** (spec §15.1):

```cmake
include(FetchContent)
FetchContent_Declare(termacs_core
    GIT_REPOSITORY https://github.com/nidwe72/termacs-core.git
    GIT_TAG v0.1.0)
FetchContent_MakeAvailable(termacs_core)
target_link_libraries(yourlib PRIVATE termacs)
```
