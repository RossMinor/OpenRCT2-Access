# Speech runtime libraries

The DLLs the mod needs beside `openrct2.exe` in order to talk. They are tracked here, in the
source tree, rather than being dropped into `bin\` by hand, because `bin\` is a build output
directory that `.gitignore` excludes - so a fresh clone used to produce a game that built and ran
but could not speak. The `CopySpeechLibraries` target in `openrct2.targets` copies everything in
this folder next to the executable on every build.

| File | Purpose | Licence |
| --- | --- | --- |
| `prism.dll` | [Prism](https://github.com/ethindp/prism) v0.18.2 - the screen-reader abstraction all speech goes through. | MPL-2.0 |
| `tolk.dll` | Runtime dependency of Prism's Windows backends. Prism loads it on demand; shipping `prism.dll` alone is not enough. | LGPL-2.1 |
| `nvdaControllerClient64.dll` | The NVDA Controller Client. Only used by the fallback path in `ScreenReader.cpp`, for the case where `prism.dll` is missing or fails to initialise. | LGPL-2.1 |

All three are redistributed unmodified. Prism's licence text is kept next to its headers in
[`../prism/LICENSE-MPL-2.0.txt`](../prism/LICENSE-MPL-2.0.txt).

## Why the failure this prevents is easy to miss

Losing `prism.dll` does not crash the game and does not produce an error the player will see.
`ScreenReader.cpp` quietly falls back to driving NVDA directly, so speech keeps working for NVDA
users and only breaks for everyone on JAWS, Narrator, System Access and the rest. That is exactly
the kind of regression that ships unnoticed, which is why these files are version-controlled and
copied automatically rather than left to the packaging step to remember.

## Updating them

`prism.dll` and `tolk.dll` come from the `prism-windows-x64.zip` asset on the
[Prism releases page](https://github.com/ethindp/prism/releases), under `dynamic/release/bin/`.
Update the headers in [`../prism/`](../prism/) at the same time and from the same release, so the
declarations the mod compiles against always match the binary it loads - see that folder's README.
