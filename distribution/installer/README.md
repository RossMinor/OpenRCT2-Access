# OpenRCT2-Access

This download is a complete copy of OpenRCT2 with the accessibility mod built into it. You do not
need OpenRCT2 already, and there is nothing to install.

## Playing

1. Unzip this download anywhere you like. Your Downloads folder is fine.
2. Start your screen reader.
3. Run **openrct2.exe** from the folder you unzipped.

That is the whole procedure, and it is the same whether or not you already have OpenRCT2. If you
want it somewhere easier to reach, right-click `openrct2.exe` and choose **Send to > Desktop
(create shortcut)**, or pin it to your Start Menu - but that is your choice, not a required step.

The first launch looks for your RollerCoaster Tycoon 2 files and finds them automatically if you own
the game on Steam.

To remove it, delete the folder. Nothing was put anywhere else.

## What this does and does not touch

Your saved parks and settings live in `Documents\OpenRCT2`, outside this folder. That is also where
a separate OpenRCT2 keeps them, so if you already have one you will see the same saved parks in
both, and this folder can be deleted at any time without losing them.

Nothing is written into an existing OpenRCT2 installation. It keeps its own executable and its own
shortcut, and it stays exactly as unmodded as it was. The two do not interfere; just don't run both
at once, since they share one settings file.

## The version rule, and why it cannot bite you

`openrct2.exe` is version-locked to the `data` folder beside it: `g2.dat` is validated against a
sprite count compiled into the executable, so an executable from one OpenRCT2 release next to
another release's data gives missing or wrong graphics rather than a clear error.

Both travel together in this download and neither is ever fitted to files from anywhere else, which
is what makes that a non-issue rather than something you have to think about.

The mod checks for its own updates and offers to install them in-game. There is no separate OpenRCT2
version to keep in step, because this folder carries its own.

## For maintainers

- The zip root **is** the game: `openrct2.exe`, the three speech DLLs, and the full `data\` tree.
  `scripts\build-access-release.ps1` builds it and verifies the result; it now refuses to package a
  `.bat`, so an install step cannot creep back in.
- `OpenRCT2Access-Installer.ps1` still ships, but nothing in a current build runs it and no player
  should. Builds released **before** the switch to unzip-and-run update themselves by invoking it
  out of the unpacked staging folder, so removing it from the package would break the in-game update
  for anyone still on those builds. The current `FinishInstall` deletes it from the staging copy so
  it never lands in a player's folder. Drop it from the package once nobody is updating across that
  change. Treat the script as frozen until then - it is a compatibility shim, and its own messages
  still refer to the `.bat` launchers that no longer ship.
- **In-game update** (`FinishInstall` in
  [AccessUpdate.cpp](../../src/openrct2-ui/accessibility/AccessUpdate.cpp)): the helper waits for the
  game to exit, `xcopy`s the downloaded release over the game folder, and relaunches. Copying
  wholesale is safe precisely because a release is a whole portable game - exe and data always arrive
  from the same build. If the copy fails, the helper writes
  `openrct2-access-update-failed.txt` beside `openrct2-access-update.log` in the temp folder and the
  mod reports it on the next launch, telling the player to unzip the release over the folder by hand
  (a part-finished copy is not safe to leave silent).
- Both version numbers are read out of the executables as text where anything needs them - the
  engine stamps `OpenRCT2, v0.5.5` into its binary and the mod stamps `OpenRCT2-Access, v1.01`
  beside it (`kAccessVersionBanner` in [Version.h](../../src/openrct2/Version.h)). That works on any
  build, official or self-compiled, without running it, and without trusting the Windows file version
  resource - OpenRCT2 leaves that at `0.0.0.0` on builds made outside its release pipeline. If you
  change how those banners are formatted, update the packaging script's regexes to match.
- Scripts here target **Windows PowerShell 5.1** - no ternary, no null-coalescing, no `&&`.
- Player-facing output is plain sequential text with no progress bars, spinners or colour-carried
  meaning, because it is read aloud.
