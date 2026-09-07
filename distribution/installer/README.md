# OpenRCT2-Access

This download is a complete copy of OpenRCT2 with the accessibility mod built into it, plus an
installer for adding the mod to an OpenRCT2 you already have. Which one you want depends on whether
you already play OpenRCT2.

## If you do not have OpenRCT2

There is nothing to install.

1. Unzip this download wherever you would like to keep the game.
2. Start your screen reader.
3. Run **openrct2.exe**.

The first launch looks for your RollerCoaster Tycoon 2 files and finds them automatically if you own
the game on Steam. Ignore the `.bat` files - they are only for the case below. Move the folder
wherever you like, and make a shortcut to `openrct2.exe` if you want one.

## If you already have OpenRCT2

Use the installer instead, so the mod goes into the copy you already play and your existing setup
stays where it is.

1. Unzip this download somewhere that is **not** your OpenRCT2 folder - your Downloads folder is fine.
2. Close OpenRCT2 if it is running.
3. Run **Install-OpenRCT2Access.bat**. It finds your installation, tells you what it is about to
   change, and asks you to type `yes` before touching anything.
4. Start your screen reader and launch OpenRCT2 the way you normally do.

To remove it again, run **Uninstall-OpenRCT2Access.bat**. That puts your original `openrct2.exe`
back and deletes everything the installer added.

If it says it cannot write to the folder, close the window, then right-click the .bat file and
choose **Run as administrator**. That happens when OpenRCT2 is installed under Program Files.

## What the installer changes

| File | What happens |
| --- | --- |
| `openrct2.exe` | Replaced. The original is kept beside it as `openrct2.exe.pre-access-backup`. |
| `prism.dll`, `tolk.dll`, `nvdaControllerClient64.dll` | Added. These are how the mod talks to your screen reader. |
| `data\sounds\access\` | Added. The mod's own sound cues. |
| `data\` | Only when your OpenRCT2 is older than this build - see below. |

Your saved parks, settings and RollerCoaster Tycoon 2 files are never touched. They live in
`Documents\OpenRCT2`, outside the game folder.

## The version rule

`openrct2.exe` is version-locked to the `data` folder beside it: `g2.dat` is validated against a
sprite count compiled into the executable, so an executable from one OpenRCT2 release next to
another release's data gives missing or wrong graphics rather than a clear error.

This download carries both, which is what makes that a non-issue. If your OpenRCT2 is **older** than
this build, the installer brings it up to date at the same time - nothing extra to download and
nothing to choose.

If your OpenRCT2 is **newer** than this build, the installer stops. It could install, since matching
data ships here, but it would move your game back a version and a park saved by the newer version
might not open afterwards. Wait for a mod build for your version; the message names both versions.

One consequence worth knowing: **OpenRCT2's own updater will overwrite the modded executable** when
it updates the game, silently removing the mod. If speech stops after a game update, run this
installer again.

## How it recognises things

Both version numbers are read straight out of the executables as text - the engine stamps
`OpenRCT2, v0.5.5` into its binary and the mod stamps `OpenRCT2-Access, v1.0` beside it
(`kAccessVersionBanner` in [Version.h](../../src/openrct2/Version.h)). Reading the binary works for
any build, official or self-compiled, without running it, and without trusting the Windows file
version resource - OpenRCT2 leaves that at `0.0.0.0` on builds made outside its release pipeline.

If you change how those banners are formatted, update `Get-ExeVersions` in the script to match.

## For maintainers

- `OpenRCT2Access-Installer.ps1` holds all the logic; the two `.bat` files only launch it with
  `-ExecutionPolicy Bypass -NoProfile`, since PowerShell blocks downloaded scripts by default and a
  user's own profile could otherwise print noise into output that is being read aloud.
- Parameters: `-TargetPath <folder>` to skip detection (it may name a new or empty folder to set up a
  fresh installation there), `-Uninstall`, `-Yes` to skip the confirmation prompt, `-NoPause` to skip
  the closing "Press Enter". The last two are for callers rather than people: the mod's in-game
  updater runs this script from a minimised window where a prompt nobody can see would hang the
  update forever.
- With no OpenRCT2 found, the installer offers to set one up in
  `%LOCALAPPDATA%\Programs\OpenRCT2` - no elevation needed, and already one of the folders it
  searches, so later updates find it. Such an installation gets an
  `installed-by-openrct2-access.txt` marker, because uninstalling it has no original to restore and
  the uninstaller must say "delete the folder" rather than "reinstall OpenRCT2".
- **The in-game updater installs through this script**, rather than copying files itself, so an
  automatic update gets the same version handling, the same backup and the same limited file set as a
  manual one. If the installer refuses, the updater leaves the game untouched, writes
  `openrct2-access-update-failed.txt` into the temp folder next to `openrct2-access-update.log`, and
  the mod announces the failure on the next launch. Renaming this script or changing its parameters
  therefore breaks automatic updates - see `FinishInstall` in
  [AccessUpdate.cpp](../../src/openrct2-ui/accessibility/AccessUpdate.cpp).
- The script targets **Windows PowerShell 5.1** - no ternary, no null-coalescing, no `&&`.
- Re-installing never overwrites an existing backup, so a second install cannot replace the pristine
  executable with a modded one and strand the player without an uninstall. The exception is a version
  change, which drops the backup deliberately: it would no longer match the new data.
- Output is plain sequential text with no progress bars, spinners or colour-carried meaning, because
  it is read aloud.
