# OpenRCT2-Access installer.
#
# Installs the accessibility mod into an existing OpenRCT2 installation, and uninstalls it again.
#
# The mod is compiled into openrct2.exe, so "installing" means swapping that one file and adding
# three DLLs plus a sounds folder. Nothing belonging to the stock game is edited - every other file
# we place is new - which is what makes the uninstall a genuine restore rather than a best effort.
#
# The exe and the game's data/ folder are version-locked: g2.dat is validated against a sprite count
# compiled into the executable, so a mod build of one OpenRCT2 version dropped onto another produces
# missing or wrong graphics. The installer therefore reads the engine version out of both
# executables and refuses to proceed unless they match exactly.
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
        if (-not (Test-Path -LiteralPath $TargetPath)) {
            throw "No such folder: $TargetPath"
        }
        $full = (Resolve-Path -LiteralPath $TargetPath).Path
        if (-not (Test-Path -LiteralPath (Join-Path $full $script:PayloadExe))) {
            throw "That folder does not contain $($script:PayloadExe): $full"
        }
        return $full
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

    Write-Info 'OpenRCT2 was not found in any of the usual places.'
    $typed = Read-Host 'Type the full path of the folder containing openrct2.exe'
    if ([string]::IsNullOrWhiteSpace($typed)) { throw 'No folder given.' }
    $typed = $typed.Trim('"')
    if (-not (Test-Path -LiteralPath (Join-Path $typed $script:PayloadExe))) {
        throw "That folder does not contain $($script:PayloadExe): $typed"
    }
    return (Resolve-Path -LiteralPath $typed).Path
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
    if ($targetVer.Mod) {
        Write-Info "The installed game is OpenRCT2 $($targetVer.Engine) with mod version $($targetVer.Mod) already installed."
    } else {
        Write-Info "The installed game is OpenRCT2 $($targetVer.Engine), unmodded."
    }

    if (-not $payloadVer.Engine) { throw 'Could not read a version from this mod build. The download may be damaged.' }
    if (-not $targetVer.Engine)  { throw "Could not read a version from $Target\$($script:PayloadExe). Is that really OpenRCT2?" }

    # The hard gate. Mixing an exe with another version's data/ gives broken graphics, not a clean error.
    if ($payloadVer.Engine -ne $targetVer.Engine) {
        Write-Host ''
        Write-Host 'Cannot install: version mismatch.'
        Write-Info "This mod build is for OpenRCT2 $($payloadVer.Engine), but the installed game is $($targetVer.Engine)."
        Write-Info 'The executable and the game data folder must come from the same OpenRCT2 version,'
        Write-Info 'so installing this would leave the game with missing or wrong graphics.'
        Write-Info ''
        Write-Info "Download the mod build for OpenRCT2 $($targetVer.Engine), or update OpenRCT2 to $($payloadVer.Engine)."
        Write-Info 'Releases: https://github.com/RossMinor/OpenRCT2-Access/releases'
        throw 'Version mismatch - nothing was changed.'
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
    Write-Info "$Target\$($script:PayloadExe)  (replaced; the original is kept as $($script:BackupName))"
    foreach ($d in $script:PayloadDlls) { Write-Info "$Target\$d  (added)" }
    foreach ($d in $script:PayloadDirs) { Write-Info "$Target\$d\  (added)" }

    if (-not $Yes) {
        Write-Host ''
        $answer = Read-Host 'Type yes to install, or press Enter to cancel'
        if ($answer.Trim().ToLower() -ne 'yes') { Write-Host 'Cancelled. Nothing was changed.'; return }
    }

    Write-Step 'Installing'

    # Back up the stock executable, but never let a re-install overwrite a good backup with an
    # already-modded exe - that would destroy the only copy of the original.
    $backupPath = Join-Path $Target $script:BackupName
    if (Test-Path -LiteralPath $backupPath) {
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
    Write-Info 'Start your screen reader, then launch OpenRCT2 as you normally would.'
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
