# OpenRCT2-Access installer

Installs the accessibility mod into an OpenRCT2 installation you already have, instead of running
the mod as a separate standalone copy of the game. Your saves, settings and scenarios stay where
they are.

## For players

1. Unzip the download somewhere that is **not** your OpenRCT2 folder - your Downloads folder is fine.
2. Close OpenRCT2 if it is running.
3. Run **Install-OpenRCT2Access.bat**. It finds your installation, tells you what it is about to
   change, and asks you to type `yes` before touching anything.
4. Start your screen reader and launch OpenRCT2 the way you normally do.

To remove it again, run **Uninstall-OpenRCT2Access.bat**. That puts your original `openrct2.exe`
back and deletes everything the installer added.

If it says it cannot write to the folder, close the window, then right-click the .bat file and
choose **Run as administrator**. That happens when OpenRCT2 is installed under Program Files.

## What it changes

| File | What happens |
| --- | --- |
| `openrct2.exe` | Replaced. The original is kept beside it as `openrct2.exe.pre-access-backup`. |
| `prism.dll`, `tolk.dll`, `nvdaControllerClient64.dll` | Added. These are how the mod talks to your screen reader. |
| `data\sounds\access\` | Added. The mod's own sound cues. |

**Nothing belonging to the stock game is edited.** Every file the installer places except the
executable is a new one, which is why uninstalling is a real restore rather than an approximation.

## The version rule

Each mod build works with exactly one version of OpenRCT2, and the installer will refuse to install
onto any other. That is not caution for its own sake: `g2.dat` is validated against a sprite count
compiled into the executable, so mixing a mod build of one version with another version's `data`
folder gives missing or wrong graphics rather than a clear error message.

If your OpenRCT2 is newer than the newest mod build, wait for the mod to catch up, or install the
matching older OpenRCT2. The refusal message names both versions so you know which you need.

**OpenRCT2's own updater will overwrite the modded executable** when it updates the game, silently
removing the mod. If speech stops after a game update, run the installer again with a build matching
your new version.

## How it recognises things

Both version numbers are read straight out of the executables as text - the engine stamps
`OpenRCT2, v0.5.4` into its binary and the mod stamps `OpenRCT2-Access, v0.99` beside it
(`kAccessVersionBanner` in [Version.h](../../src/openrct2/Version.h)). Reading the binary works for
any build, official or self-compiled, without running it, and without trusting the Windows file
version resource - OpenRCT2 leaves that at `0.0.0.0` on builds made outside its release pipeline.

If you change how those banners are formatted, update `Get-ExeVersions` in the script to match.

## For maintainers

- `OpenRCT2Access-Installer.ps1` holds all the logic; the two `.bat` files only launch it with
  `-ExecutionPolicy Bypass -NoProfile`, since PowerShell blocks downloaded scripts by default and a
  user's own profile could otherwise print noise into output that is being read aloud.
- Parameters: `-TargetPath <folder>` to skip detection, `-Uninstall`, `-Yes` to skip the
  confirmation prompt. `-Yes` exists for testing; a player should never need it.
- The script targets **Windows PowerShell 5.1** - no ternary, no null-coalescing, no `&&`.
- Re-installing never overwrites an existing backup, so a second install cannot replace the pristine
  executable with a modded one and strand the player without an uninstall.
- Output is plain sequential text with no progress bars, spinners or colour-carried meaning, because
  it is read aloud.
