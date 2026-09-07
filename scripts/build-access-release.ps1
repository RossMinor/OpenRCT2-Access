# Packages an OpenRCT2-Access release.
#
# The package is the game, not an installer for it: the executable, the speech DLLs and the matching
# game data\ tree - g2.dat, objects, scenarios, language files, the lot - all at the zip root. Unzip
# it anywhere and run openrct2.exe. There is no install step because there is nothing left to do.
#
# Shipping data\ alongside the executable is what makes that possible, and it makes version
# mismatches impossible at the same time: the executable is version-locked to data\ (g2.dat is
# validated against a sprite count compiled into the exe), so travelling together means a player can
# never end up with an executable from one OpenRCT2 release sitting beside another release's data.
#
# The alternative - shipping only the executable, at about 8 MB - was tried, and it pushed that
# version-matching problem onto the player: their data\ came from whichever OpenRCT2 they happened
# to have installed, and any mismatch had to be detected, explained and repaired. 80 MB is a cheap
# price for deleting that entire class of failure.
#
# The zip is still named for BOTH versions, because a build is built against exactly one OpenRCT2
# release and the filename should say which.
#
# Run from anywhere. Windows PowerShell 5.1: no ternary, no null-coalescing, no && chaining.

[CmdletBinding()]
param(
    # Where to write the zip. Defaults to the repository's artifacts folder.
    [string] $OutputDir
)

$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$bin  = Join-Path $repo 'bin'
if (-not $OutputDir) { $OutputDir = Join-Path $repo 'artifacts' }

function Fail { param([string] $Message) Write-Host "ERROR: $Message"; exit 1 }

$exe = Join-Path $bin 'openrct2.exe'
if (-not (Test-Path -LiteralPath $exe)) { Fail "No built executable at $exe. Build Release x64 first." }

# Both versions come out of the binary, so the package can never disagree with what it contains.
$text = [System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes($exe))
$engine = [regex]::Match($text, 'OpenRCT2, v([0-9]+\.[0-9]+\.[0-9]+)').Groups[1].Value
$mod    = [regex]::Match($text, 'OpenRCT2-Access, v([0-9][0-9.]*)').Groups[1].Value
if (-not $engine) { Fail 'Could not read the OpenRCT2 version from the executable.' }
if (-not $mod)    { Fail 'Could not read the mod version from the executable. Is this a mod build?' }

Write-Host "Packaging OpenRCT2-Access v$mod for OpenRCT2 v$engine"

# Everything that goes in the zip, as source -> name at the zip root.
$files = [ordered]@{}
$files[$exe] = 'openrct2.exe'
foreach ($d in 'prism.dll', 'tolk.dll', 'nvdaControllerClient64.dll') {
    $p = Join-Path $bin $d
    if (-not (Test-Path -LiteralPath $p)) { Fail "Missing $d in bin. Build once to run CopySpeechLibraries." }
    $files[$p] = $d
}
# The installer script, and ONLY the script. There is nothing to install any more - unzipping the
# package produces a complete, runnable game, so the player runs openrct2.exe and that is the whole
# procedure. The two .bat launchers are deliberately not shipped: an install step that copies files
# into a folder the player already unzipped earns nothing and is one more thing to get wrong.
#
# The script itself still travels because builds released BEFORE this change update themselves by
# running it out of the unpacked staging folder (FinishInstall in AccessUpdate.cpp). Dropping it
# would break the in-game update for everyone still on those builds. It can go once nobody is
# updating across this change; nothing in a current build calls it, and the current FinishInstall
# deletes it from the staging copy so it never reaches a player's game folder.
$files[(Join-Path $repo 'distribution\installer\OpenRCT2Access-Installer.ps1')] = 'OpenRCT2Access-Installer.ps1'
$files[(Join-Path $repo 'distribution\installer\README.md')] = 'README.md'
$files[(Join-Path $repo 'distribution\changelog.txt')]       = 'changelog.txt'
$files[(Join-Path $repo 'contributors.md')]                  = 'contributors.md'
$files[(Join-Path $repo 'licence.txt')]                      = 'licence.txt'
$files[(Join-Path $repo 'PRIVACY.md')]                       = 'PRIVACY.md'
# Prism is MPL-2.0 and we redistribute prism.dll, so its licence has to travel with it.
$files[(Join-Path $repo 'src\openrct2-ui\accessibility\prism\LICENSE-MPL-2.0.txt')] = 'LICENSE-Prism-MPL-2.0.txt'

# The mod's own sound cues - normally the only game data the package carries. The folder's README is
# developer documentation and would otherwise be installed into the player's data directory.
foreach ($w in Get-ChildItem (Join-Path $repo 'data\sounds\access') -Filter *.wav) {
    $files[$w.FullName] = "data/sounds/access/$($w.Name)"
}

# The built data tree. This travels with the executable so the two can never disagree - see the
# header. portable-data is excluded: it holds the player's own config and saved parks, and
# overwriting those would be the worst kind of "upgrade".
$dataRoot = Join-Path $bin 'data'
if (-not (Test-Path -LiteralPath $dataRoot)) { Fail "No built data tree at $dataRoot. Build the solution first." }
$prefix = (Resolve-Path -LiteralPath $dataRoot).Path.Length + 1

# Dedupe on the ZIP ENTRY NAME, not the source path. The mod's cues are staged above from the
# repository while bin\data holds build-output copies of the same files: different paths, same
# destination. Keyed by path they would both be added and the zip would carry two entries called
# data/sounds/access/DirtStep1.wav. Keeping the repository copy also drops anything stale that
# is sitting in bin\data but no longer in the repository.
$used = New-Object 'System.Collections.Generic.HashSet[string]'
foreach ($n in $files.Values) { [void] $used.Add($n) }

foreach ($f in Get-ChildItem $dataRoot -Recurse -File) {
    $rel = 'data/' + $f.FullName.Substring($prefix).Replace([char]92, [char]47)
    # Skip the mod's sound folder wholesale rather than relying on the name check. bin\data is
    # build output that accumulates: it still holds a misspelled Vommit.wav no longer in the
    # repository, and the folder's developer README, neither of which belongs in a player's
    # install. The repository copies staged above are the authoritative set.
    if ($rel.StartsWith('data/sounds/access/')) { continue }
    if ($used.Add($rel)) { $files[$f.FullName] = $rel }
}

foreach ($src in $files.Keys) {
    if (-not (Test-Path -LiteralPath $src)) { Fail "Missing file: $src" }
}

if (-not (Test-Path -LiteralPath $OutputDir)) { New-Item -ItemType Directory -Force $OutputDir | Out-Null }
$zipPath = Join-Path $OutputDir "OpenRCT2-Access-v$mod-for-OpenRCT2-v$engine-windows-x64.zip"
if (Test-Path -LiteralPath $zipPath) { Fail "$zipPath already exists. Move it aside rather than overwriting a published package." }

# .NET ZipArchive rather than Compress-Archive: PowerShell 5.1 writes backslash entry names, which
# some extractors render as one long filename instead of a folder.
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$fs = [System.IO.File]::Open($zipPath, [System.IO.FileMode]::CreateNew)
$archive = New-Object System.IO.Compression.ZipArchive($fs, [System.IO.Compression.ZipArchiveMode]::Create)
foreach ($src in $files.Keys) {
    $entry = $archive.CreateEntry($files[$src], [System.IO.Compression.CompressionLevel]::Optimal)
    $out = $entry.Open()
    $in = [System.IO.File]::OpenRead($src)
    $in.CopyTo($out)
    $in.Dispose(); $out.Dispose()
}
$archive.Dispose(); $fs.Dispose()

# Read the finished package back rather than trusting that it contains what we intended.
$check = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
$names = @($check.Entries | ForEach-Object { $_.FullName })
$check.Dispose()

$problems = @()
if ($names.Count -ne $files.Count) { $problems += "entry count $($names.Count) != expected $($files.Count)" }
if ($names | Where-Object { $_.Contains([char]92) }) { $problems += 'entries contain backslashes' }
foreach ($required in 'openrct2.exe', 'prism.dll', 'OpenRCT2Access-Installer.ps1') {
    if ($names -notcontains $required) { $problems += "missing $required" }
}
# Nothing for a player to run but the game. A .bat back in the package would put an install step in
# front of a folder that is already a complete installation.
if ($names | Where-Object { $_ -like '*.bat' }) { $problems += 'a .bat launcher leaked into the package' }
if (($names | Where-Object { $_ -like 'data/sounds/access/*' }).Count -lt 1) { $problems += 'no sound cues' }
if ($names | Where-Object { $_ -like '*.pdb' -or $_ -like '*.lib' -or $_ -like '*portable-data*' }) { $problems += 'development files leaked in' }
if (($names | Group-Object | Where-Object { $_.Count -gt 1 }).Count -gt 0) { $problems += 'duplicate entry names' }
# The data tree is the whole point of the package shape, so check the files that pair with the
# executable rather than trusting a file count. A package missing these would install an executable
# with nothing to match it, which is exactly the failure this design exists to prevent.
foreach ($required in 'data/g2.dat', 'data/fonts.dat', 'data/tracks.dat') {
    if ($names -notcontains $required) { $problems += "missing $required" }
}
if (($names | Where-Object { $_ -like 'data/language/*' }).Count -lt 1) { $problems += 'no language files' }
if (($names | Where-Object { $_ -like 'data/object/*' }).Count -lt 1) { $problems += 'no object data' }

if ($problems.Count -gt 0) {
    Write-Host 'Package FAILED verification:'
    foreach ($p in $problems) { Write-Host "  - $p" }
    exit 1
}

Write-Host ''
Write-Host "Wrote $zipPath"
Write-Host ("  {0} entries, {1:N1} MB" -f $names.Count, ((Get-Item $zipPath).Length / 1MB))
Write-Host "  a complete OpenRCT2 v$engine with OpenRCT2-Access v$mod - unzip and run openrct2.exe"
