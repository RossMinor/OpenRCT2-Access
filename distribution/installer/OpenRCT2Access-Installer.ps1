# OpenRCT2-Access installer.
#
# Installs the accessibility mod into an existing OpenRCT2 installation, and uninstalls it again.
#
# The mod is compiled into openrct2.exe, so "installing" normally means swapping that one file and
# adding three DLLs plus a sounds folder. Nothing belonging to the stock game is edited, which is
# what makes the uninstall a genuine restore rather than a best effort.
#
# The exe and the game's data\ folder are version-locked: g2.dat is validated against a sprite count
# compiled into the executable, so an exe from one OpenRCT2 release beside another release's data
# produces missing or wrong graphics rather than a clean error.
#
# The download carries the matching data\ tree, so when the player's OpenRCT2 is OLDER than this
# build, both are simply installed together - nothing to fetch and nothing to ask. That case does
# give up the backup, because a saved executable would belong to the version being replaced.
#
# When their OpenRCT2 is NEWER the installer stops. Installing would work, but it would move their
# game back a version and a park saved by the newer engine may not open afterwards. That case wants
# a mod build for their version, not a downgrade.
#
# Written for Windows PowerShell 5.1 - no ternary, no null-coalescing, no && chaining.

[CmdletBinding()]
param(
    # Where OpenRCT2 is installed. Omit to search the usual places and then ask.
    [string] $TargetPath,

    # Put the original executable back and remove everything this installer added.
    [switch] $Uninstall,

    # Answer yes to the confirmation prompt. For scripted use; a person should not need it.
    [switch] $Yes,

    # Skip the "Press Enter to close" at the end. Required when something else is driving this
    # script - the in-game updater runs it from a background window where a prompt nobody can see
    # would hang the update forever.
    [switch] $NoPause
)

$ErrorActionPreference = 'Stop'

# Everything we place into the install. The exe replaces a stock file; the rest are additions.
$script:PayloadExe    = 'openrct2.exe'
$script:PayloadDlls   = @('prism.dll', 'tolk.dll', 'nvdaControllerClient64.dll')
$script:PayloadDirs   = @('data\sounds\access')
$script:BackupName    = 'openrct2.exe.pre-access-backup'

# Where OpenRCT2 goes when the player does not have one. Under LOCALAPPDATA rather than Program
# Files so no elevation is needed, and it is already one of the folders Find-OpenRCT2Install looks
# in, so a later re-install or update finds it without being told.
$script:FreshInstallPath = Join-Path $env:LOCALAPPDATA 'Programs\OpenRCT2'

# Dropped into an installation this installer created from scratch. Uninstalling one of those has no
# original to restore - the whole folder is ours - so the uninstaller needs to know the difference.
$script:FreshMarker   = 'installed-by-openrct2-access.txt'
$script:IsFreshInstall = $false

function Write-Step { param([string] $Text) Write-Host ''; Write-Host $Text }
function Write-Info { param([string] $Text) Write-Host "  $Text" }

# Reads a version banner straight out of a compiled executable. Both the engine and the mod stamp a
# plain literal into the binary ("OpenRCT2, v0.5.4" and "OpenRCT2-Access, v0.99"), so this works on
# any build, official or self-compiled, without running it or trusting a file-version resource -
# which OpenRCT2 leaves at 0.0.0.0 on builds made outside its release pipeline.
function Get-ExeVersions {
    param([Parameter(Mandatory)] [string] $ExePath)

    $result = [pscustomobject]@{ Engine = $null; Mod = $null }
    if (-not (Test-Path -LiteralPath $ExePath)) { return $result }

    $bytes = [System.IO.File]::ReadAllBytes($ExePath)
    $text  = [System.Text.Encoding]::ASCII.GetString($bytes)

    $m = [regex]::Match($text, 'OpenRCT2, v([0-9]+\.[0-9]+\.[0-9]+)')
    if ($m.Success) { $result.Engine = $m.Groups[1].Value }

    $m = [regex]::Match($text, 'OpenRCT2-Access, v([0-9][0-9.]*)')
    if ($m.Success) { $result.Mod = $m.Groups[1].Value }

    return $result
}

# Candidate install locations: whatever the uninstall registry knows about, then the paths the
# official installer and the portable builds normally land in.
function Find-OpenRCT2Install {
    $candidates = New-Object System.Collections.Generic.List[string]

    $keys = @(
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*'
    )
    foreach ($key in $keys) {
        try {
            Get-ItemProperty $key -ErrorAction SilentlyContinue |
                Where-Object { $_.DisplayName -like '*OpenRCT2*' -and $_.InstallLocation } |
                ForEach-Object { $candidates.Add($_.InstallLocation) }
        } catch { }
    }

    foreach ($p in @(
        (Join-Path $env:ProgramFiles 'OpenRCT2'),
        (Join-Path ${env:ProgramFiles(x86)} 'OpenRCT2'),
        (Join-Path $env:LOCALAPPDATA 'Programs\OpenRCT2'),
        'C:\OpenRCT2'
    )) {
        if ($p) { $candidates.Add($p) }
    }

    $found = New-Object System.Collections.Generic.List[string]
    foreach ($c in $candidates) {
        if ([string]::IsNullOrWhiteSpace($c)) { continue }
        $exe = Join-Path $c $script:PayloadExe
        if (Test-Path -LiteralPath $exe) {
            $full = (Resolve-Path -LiteralPath $c).Path
            if (-not $found.Contains($full)) { $found.Add($full) }
        }
    }
    return $found
}

# True when we can actually write into the folder. Program Files needs elevation, and finding that
# out now gives a clear instruction instead of a half-finished install.
function Test-Writable {
    param([Parameter(Mandatory)] [string] $Folder)
    $probe = Join-Path $Folder ('.access-write-test-' + [guid]::NewGuid().ToString('N') + '.tmp')
    try {
        [System.IO.File]::WriteAllText($probe, 'x')
        [System.IO.File]::Delete($probe)
        return $true
    } catch {
        return $false
    }
}

function Resolve-Target {
    if ($TargetPath) {
        $exists = Test-Path -LiteralPath $TargetPath
        if ($exists -and (Test-Path -LiteralPath (Join-Path $TargetPath $script:PayloadExe))) {
            return (Resolve-Path -LiteralPath $TargetPath).Path
        }
        if ($Uninstall) {
            if (-not $exists) { throw "No such folder: $TargetPath" }
            throw "That folder does not contain $($script:PayloadExe): $TargetPath"
        }
        # A named folder with no OpenRCT2 in it means "set one up here", but only when it is new or
        # empty. A folder with other files in it is far more likely to be a mistyped path than a
        # deliberate choice, and quietly unpacking a game into it would be the wrong guess.
        if ($exists -and (Get-ChildItem -LiteralPath $TargetPath -Force | Select-Object -First 1)) {
            throw "That folder is not empty and does not contain $($script:PayloadExe): $TargetPath"
        }
        New-Item -ItemType Directory -Path $TargetPath -Force | Out-Null
        $script:IsFreshInstall = $true
        return (Resolve-Path -LiteralPath $TargetPath).Path
    }

    $found = Find-OpenRCT2Install
    if ($found.Count -eq 1) {
        Write-Info "Found OpenRCT2 at: $($found[0])"
        return $found[0]
    }
    if ($found.Count -gt 1) {
        Write-Info 'More than one OpenRCT2 installation was found:'
        for ($i = 0; $i -lt $found.Count; $i++) { Write-Info "  $($i + 1). $($found[$i])" }
        $pick = Read-Host 'Type the number of the one to use'
        $idx = 0
        if ([int]::TryParse($pick, [ref] $idx) -and $idx -ge 1 -and $idx -le $found.Count) {
            return $found[$idx - 1]
        }
        throw 'That was not one of the listed numbers.'
    }

    # Uninstalling something that is not there: nothing to resolve, and certainly nothing to set up.
    if ($Uninstall) {
        Write-Info 'OpenRCT2 was not found in any of the usual places.'
        $typed = Read-Host 'Type the full path of the folder containing openrct2.exe'
        if ([string]::IsNullOrWhiteSpace($typed)) { throw 'No folder given.' }
        $typed = $typed.Trim('"')
        if (-not (Test-Path -LiteralPath (Join-Path $typed $script:PayloadExe))) {
            throw "That folder does not contain $($script:PayloadExe): $typed"
        }
        return (Resolve-Path -LiteralPath $typed).Path
    }

    # Nothing found. The download carries a complete OpenRCT2 - the executable and the whole data\
    # tree - so there is no reason to send the player away to install one first. Setting it up here
    # also keeps them inside an accessible installer, rather than making them get through stock
    # OpenRCT2's own first run unaided.
    Write-Host ''
    Write-Host 'OpenRCT2 was not found on this computer.'
    Write-Info 'That is fine - this download includes it, so it can be set up for you now.'
    Write-Info "It will be installed to: $($script:FreshInstallPath)"
    Write-Info 'This folder does not need administrator rights, and RollerCoaster Tycoon 2 itself is'
    Write-Info 'found automatically if you own it on Steam.'
    Write-Host ''

    if (-not $Yes) {
        $answer = Read-Host 'Type yes to set up OpenRCT2 there, or type the path to an OpenRCT2 you already have'
        $answer = $answer.Trim().Trim('"')
        if ($answer.ToLower() -ne 'yes') {
            if ([string]::IsNullOrWhiteSpace($answer)) { throw 'Cancelled - nothing was changed.' }
            if (-not (Test-Path -LiteralPath (Join-Path $answer $script:PayloadExe))) {
                throw "That folder does not contain $($script:PayloadExe): $answer"
            }
            return (Resolve-Path -LiteralPath $answer).Path
        }
    }

    New-Item -ItemType Directory -Path $script:FreshInstallPath -Force | Out-Null
    $script:IsFreshInstall = $true
    return (Resolve-Path -LiteralPath $script:FreshInstallPath).Path
}

# -1 when $A is older than $B, 0 when equal, 1 when newer. The direction matters: an older game can
# be brought up to the mod, a newer one cannot be brought down to it.
function Compare-EngineVersion {
    param([Parameter(Mandatory)] [string] $A, [Parameter(Mandatory)] [string] $B)
    $va = [version] $A
    $vb = [version] $B
    if ($va -lt $vb) { return -1 }
    if ($va -gt $vb) { return 1 }
    return 0
}

function Assert-GameClosed {
    $running = Get-Process -Name 'openrct2' -ErrorAction SilentlyContinue
    if ($running) {
        throw 'OpenRCT2 is running. Close the game and run this installer again.'
    }
}

function Invoke-Install {
    param([Parameter(Mandatory)] [string] $Target, [Parameter(Mandatory)] [string] $Payload)

    $payloadVer = Get-ExeVersions (Join-Path $Payload $script:PayloadExe)
    $targetVer  = Get-ExeVersions (Join-Path $Target  $script:PayloadExe)

    Write-Step 'Versions'
    Write-Info "This mod build is for OpenRCT2 $($payloadVer.Engine) and is mod version $($payloadVer.Mod)."
    if ($script:IsFreshInstall) {
        Write-Info 'OpenRCT2 is not installed here yet, so it will be set up from this download.'
    } elseif ($targetVer.Mod) {
        Write-Info "The installed game is OpenRCT2 $($targetVer.Engine) with mod version $($targetVer.Mod) already installed."
    } else {
        Write-Info "The installed game is OpenRCT2 $($targetVer.Engine), unmodded."
    }

    if (-not $payloadVer.Engine) { throw 'Could not read a version from this mod build. The download may be damaged.' }
    if (-not $targetVer.Engine -and -not $script:IsFreshInstall) {
        throw "Could not read a version from $Target\$($script:PayloadExe). Is that really OpenRCT2?"
    }

    # The executable is version-locked to data\ - g2.dat is validated against a sprite count compiled
    # into the exe - so the two must always come from the same OpenRCT2 release. This package carries
    # both, so when the versions differ the fix is simply to install both, with nothing to download
    # and nothing to ask. A fresh install is the same thing with nothing to compare against: every
    # file comes from this download, so it is self-consistent by construction.
    $engineChanged = $script:IsFreshInstall
    if (-not $script:IsFreshInstall -and $payloadVer.Engine -ne $targetVer.Engine) {
        if ((Compare-EngineVersion $targetVer.Engine $payloadVer.Engine) -gt 0) {
            # Their OpenRCT2 is NEWER. Installing would technically work, since matching data ships
            # here - but it would roll their game back, and a park saved by a newer engine may not
            # open in an older one. Costing someone access to their saved parks to gain the mod is
            # not a trade to make on their behalf; that needs a mod build for their version.
            Write-Host ''
            Write-Host 'Cannot install: this mod build is older than your OpenRCT2.'
            Write-Info "You have OpenRCT2 $($targetVer.Engine); this mod build is for $($payloadVer.Engine)."
            Write-Info 'Installing it would move your game back a version, and parks you saved with the'
            Write-Info 'newer version might not open afterwards.'
            Write-Info ''
            Write-Info "Watch for a mod build for OpenRCT2 $($targetVer.Engine)."
            Write-Info 'Releases: https://github.com/RossMinor/OpenRCT2-Access/releases'
            throw 'Mod build is older than the installed game - nothing was changed.'
        }

        # Their OpenRCT2 is older, which this package can simply carry forward.
        $engineChanged = $true
        Write-Info ''
        Write-Info "Your OpenRCT2 will be updated from $($targetVer.Engine) to $($payloadVer.Engine)."
        Write-Info 'The matching game data is included in this download, so both are installed together.'
        Write-Info 'Your saved games, settings and RollerCoaster Tycoon 2 files are untouched - they live'
        Write-Info 'outside the game folder.'
    }

    if (-not (Test-Writable $Target)) {
        Write-Host ''
        Write-Host 'Cannot write to the installation folder.'
        Write-Info "Folder: $Target"
        Write-Info 'Close this window, then right-click Install-OpenRCT2Access.bat and choose'
        Write-Info '"Run as administrator".'
        throw 'Need administrator rights - nothing was changed.'
    }

    Write-Step 'About to change'
    if ($script:IsFreshInstall) {
        Write-Info "$Target\  (a new OpenRCT2 $($payloadVer.Engine) installation, with the mod)"
        Write-Info 'Nothing existing is touched, because there was nothing here.'
    } elseif ($engineChanged) {
        Write-Info "$Target\$($script:PayloadExe)  (replaced)"
    } else {
        Write-Info "$Target\$($script:PayloadExe)  (replaced; the original is kept as $($script:BackupName))"
    }
    if (-not $script:IsFreshInstall) {
        foreach ($d in $script:PayloadDlls) { Write-Info "$Target\$d  (added)" }
        foreach ($d in $script:PayloadDirs) { Write-Info "$Target\$d\  (added)" }
        if ($engineChanged) {
            Write-Info "$Target\data\  (updated to OpenRCT2 $($payloadVer.Engine))"
            Write-Info ''
            Write-Info 'Because the game data is being updated too, uninstalling later will not be able to put'
            Write-Info "your old OpenRCT2 $($targetVer.Engine) back - the saved executable would no longer match"
            Write-Info 'the new data. Reinstall OpenRCT2 if you ever want to return to the unmodded game.'
        }
    }

    if (-not $Yes) {
        Write-Host ''
        $answer = Read-Host 'Type yes to install, or press Enter to cancel'
        if ($answer.Trim().ToLower() -ne 'yes') { Write-Host 'Cancelled. Nothing was changed.'; return }
    }

    Write-Step 'Installing'

    # Back up the stock executable, but never let a re-install overwrite a good backup with an
    # already-modded exe - that would destroy the only copy of the original.
    #
    # When the game data is being updated, no backup is kept at all. Any saved executable belongs to
    # the OpenRCT2 version being replaced, so restoring it would put an old exe beside new data -
    # exactly the mismatch this installer exists to prevent, and a silent one. Better to have no
    # backup and say so than a backup that breaks the game when used.
    #
    # A fresh install has nothing to back up at all: every file in the folder came from this
    # download, so there is no "original" to return to. A marker records that, so the uninstaller
    # can say "delete this folder" rather than "reinstall OpenRCT2".
    $backupPath = Join-Path $Target $script:BackupName
    if ($script:IsFreshInstall) {
        [System.IO.File]::WriteAllText(
            (Join-Path $Target $script:FreshMarker),
            "This OpenRCT2 was installed by the OpenRCT2-Access installer." + [Environment]::NewLine +
            "There is no separate unmodded copy to restore - to remove it, delete this whole folder." + [Environment]::NewLine)
    } elseif ($engineChanged) {
        if (Test-Path -LiteralPath $backupPath) {
            Remove-Item -LiteralPath $backupPath -Force
            Write-Info 'Removed the old backup - it belonged to the previous OpenRCT2 version.'
        }
    } elseif (Test-Path -LiteralPath $backupPath) {
        Write-Info 'Original executable already backed up; keeping the existing backup.'
    } else {
        if ($targetVer.Mod) {
            Write-Info 'Warning: the installed executable is already modded and no backup exists.'
            Write-Info 'No backup will be made, so uninstall will not be able to restore the original.'
        } else {
            Copy-Item -LiteralPath (Join-Path $Target $script:PayloadExe) -Destination $backupPath -Force
            Write-Info "Backed up the original to $($script:BackupName)"
        }
    }

    Copy-Item -LiteralPath (Join-Path $Payload $script:PayloadExe) -Destination $Target -Force
    Write-Info "Installed $($script:PayloadExe)"

    foreach ($d in $script:PayloadDlls) {
        $src = Join-Path $Payload $d
        if (Test-Path -LiteralPath $src) {
            Copy-Item -LiteralPath $src -Destination $Target -Force
            Write-Info "Installed $d"
        } else {
            Write-Info "Warning: $d is missing from this download; speech may not work."
        }
    }

    # The matching game data, when the OpenRCT2 version is moving. Copied before the mod's own sound
    # folder so those cues are laid down last and stay authoritative. This merges rather than mirrors,
    # so anything the player added to data\ of their own survives.
    if ($engineChanged) {
        $dataSrc = Join-Path $Payload 'data'
        if (-not (Test-Path -LiteralPath $dataSrc)) {
            throw 'This download has no data folder, so it cannot update OpenRCT2. The download may be damaged.'
        }
        $dataDst = Join-Path $Target 'data'
        if (-not (Test-Path -LiteralPath $dataDst)) { New-Item -ItemType Directory -Path $dataDst -Force | Out-Null }
        # -Path, not -LiteralPath: the trailing wildcard has to be expanded.
        Copy-Item -Path (Join-Path $dataSrc '*') -Destination $dataDst -Recurse -Force
        Write-Info "Updated the game data to OpenRCT2 $($payloadVer.Engine)"
    }

    foreach ($d in $script:PayloadDirs) {
        $src = Join-Path $Payload $d
        if (Test-Path -LiteralPath $src) {
            $dst = Join-Path $Target $d
            if (-not (Test-Path -LiteralPath $dst)) { New-Item -ItemType Directory -Path $dst -Force | Out-Null }
            # -Path, not -LiteralPath: the trailing wildcard has to be expanded. With -LiteralPath
            # this silently copies nothing, because it looks for a file actually named "*".
            Copy-Item -Path (Join-Path $src '*') -Destination $dst -Recurse -Force
            $copied = (Get-ChildItem -LiteralPath $dst -Recurse -File).Count
            Write-Info "Installed $d ($copied files)"
        }
    }

    $now = Get-ExeVersions (Join-Path $Target $script:PayloadExe)
    Write-Step 'Done'
    Write-Info "OpenRCT2 $($now.Engine) with OpenRCT2-Access $($now.Mod) is installed."
    if ($script:IsFreshInstall) {
        Write-Info "It is at: $Target"
        Write-Info 'Start your screen reader, then run openrct2.exe from that folder. The first launch will'
        Write-Info 'look for your RollerCoaster Tycoon 2 files and should find them automatically if you own'
        Write-Info 'the game on Steam.'
    } else {
        Write-Info 'Start your screen reader, then launch OpenRCT2 as you normally would.'
    }
}

function Invoke-Uninstall {
    param([Parameter(Mandatory)] [string] $Target)

    $targetVer = Get-ExeVersions (Join-Path $Target $script:PayloadExe)
    $backupPath = Join-Path $Target $script:BackupName

    Write-Step 'Uninstalling from'
    Write-Info $Target

    if (-not $targetVer.Mod) {
        Write-Info 'The accessibility mod is not installed here.'
        if (-not (Test-Path -LiteralPath $backupPath)) { return }
    }

    # An installation this installer created has no unmodded copy to fall back to - every file in it
    # came from the mod download - so "restore the original" is meaningless. Say what actually
    # removes it instead of sending them to reinstall OpenRCT2 over the top.
    if (Test-Path -LiteralPath (Join-Path $Target $script:FreshMarker)) {
        Write-Host ''
        Write-Host 'This OpenRCT2 was installed by the accessibility mod itself.'
        Write-Info 'There is no separate unmodded copy to put back, because the whole installation came'
        Write-Info 'from the mod download.'
        Write-Info ''
        Write-Info 'To remove it, delete this folder:'
        Write-Info "  $Target"
        Write-Info 'Your saved parks and settings live elsewhere (Documents\OpenRCT2) and are not affected.'
        throw 'Nothing to restore - delete the folder to remove it.'
    }

    if (-not (Test-Path -LiteralPath $backupPath)) {
        Write-Host ''
        Write-Host 'Cannot uninstall: the original executable was not backed up.'
        Write-Info "Expected: $backupPath"
        Write-Info 'Reinstall OpenRCT2 to get a stock executable back.'
        throw 'No backup to restore - nothing was changed.'
    }

    if (-not (Test-Writable $Target)) {
        Write-Host ''
        Write-Host 'Cannot write to the installation folder.'
        Write-Info 'Right-click Install-OpenRCT2Access.bat and choose "Run as administrator".'
        throw 'Need administrator rights - nothing was changed.'
    }

    if (-not $Yes) {
        Write-Host ''
        $answer = Read-Host 'Type yes to remove the mod and restore the original game, or press Enter to cancel'
        if ($answer.Trim().ToLower() -ne 'yes') { Write-Host 'Cancelled. Nothing was changed.'; return }
    }

    Write-Step 'Restoring'
    Copy-Item -LiteralPath $backupPath -Destination (Join-Path $Target $script:PayloadExe) -Force
    Write-Info 'Restored the original openrct2.exe'
    Remove-Item -LiteralPath $backupPath -Force
    Write-Info 'Removed the backup copy'

    # Only files this installer added are removed. Anything the player put there is left alone.
    foreach ($d in $script:PayloadDlls) {
        $p = Join-Path $Target $d
        if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Force; Write-Info "Removed $d" }
    }
    foreach ($d in $script:PayloadDirs) {
        $p = Join-Path $Target $d
        if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Recurse -Force; Write-Info "Removed $d" }
    }

    $now = Get-ExeVersions (Join-Path $Target $script:PayloadExe)
    Write-Step 'Done'
    Write-Info "OpenRCT2 $($now.Engine) has been restored to its unmodded state."
}

# ---- main ----------------------------------------------------------------------------------

$payloadDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host '========================================'
Write-Host ' OpenRCT2-Access installer'
Write-Host '========================================'

try {
    Assert-GameClosed
    $target = Resolve-Target

    if ($Uninstall) {
        Invoke-Uninstall -Target $target
    } else {
        if (-not (Test-Path -LiteralPath (Join-Path $payloadDir $script:PayloadExe))) {
            throw "This installer must sit in the same folder as $($script:PayloadExe). Unzip the whole download and run it from there."
        }
        if ((Resolve-Path -LiteralPath $payloadDir).Path -eq $target) {
            throw 'The download folder and the game folder are the same. Unzip the download somewhere else and run it from there.'
        }
        Invoke-Install -Target $target -Payload $payloadDir
    }
    $exitCode = 0
} catch {
    Write-Host ''
    Write-Host "Stopped: $($_.Exception.Message)"
    $exitCode = 1
}

Write-Host ''
if (-not $NoPause) { Read-Host 'Press Enter to close' | Out-Null }
exit $exitCode
