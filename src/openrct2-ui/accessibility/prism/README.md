# Prism (vendored headers)

[Prism](https://github.com/ethindp/prism) - the Platform-agnostic Reader Interface for Speech and
Messages - is the screen-reader abstraction the mod speaks through. It replaced the mod's
direct NVDA Controller Client binding, so the same `ScreenReaderSpeak` call now reaches NVDA,
JAWS, Narrator/OneCore, SAPI, System Access, ZDSR and others on Windows, VoiceOver on macOS, and
Speech Dispatcher or Orca on Linux.

## What is here

- `prism.h`, `prism_version.h` - the public C API, taken verbatim from the `prism-windows-x64`
  release package of **Prism v0.18.2**.
- `LICENSE-MPL-2.0.txt` - Prism is licensed under the Mozilla Public License 2.0.

## The one modification

`prism.h` includes its version header as `<prism_version.h>`, which requires this directory to be
on the compiler's include path. That line is changed to a quoted include so the header is
self-contained and no build-system change is needed. The edit is marked with a comment in the
file. Nothing else is altered.

## Why only headers

The mod does not link against `prism.lib`. `ScreenReader.cpp` loads `prism.dll` at runtime with
`LoadLibrary` and resolves each entry point with `GetProcAddress`, exactly as it used to load
`nvdaControllerClient64.dll`. That choice keeps the game launchable when the DLL is missing -
important, because a hard link failure would leave a blind player with a game that will not
start, and the mod's own updater copies files over an existing install where a stale folder is
possible. These headers supply the types, enums and backend identifiers; `PRISM_STATIC` is
defined before including them so the declarations carry no `dllimport`.

## Updating Prism

1. Download `prism-windows-x64.zip` from the
   [Prism releases page](https://github.com/ethindp/prism/releases).
2. Copy `include/prism.h` and `include/prism_version.h` here, then re-apply the quoted-include
   edit above.
3. Copy `dynamic/release/bin/prism.dll` and `dynamic/release/bin/tolk.dll` into
   [`../runtime/`](../runtime/), which is version-controlled and copied next to the executable on
   every build. Take the binaries and the headers from the *same* release so the declarations the
   mod compiles against match the DLL it loads.
4. Check `PRISM_CONFIG_VERSION` in the new header. `ScreenReader.cpp` obtains its config from
   `prism_config_init()` rather than filling the struct itself, so a version bump is normally
   transparent, but a new field worth setting would show up here.
