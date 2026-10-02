param(
    [Parameter(Mandatory=$true)][string]$Python,
    [Parameter(Mandatory=$true)][string]$AcceptedLicenseDirectory,
    [string[]]$Packages = @()
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$lock = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'android-toolchain.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$sdkRoot = Join-Path $repoRoot '.qt/android-sdk'
$downloads = Join-Path $repoRoot '.qt/downloads'
# Reuse an already accepted SDK license. Never accept licenses on the user's behalf.
$license = Join-Path $AcceptedLicenseDirectory 'android-sdk-license'
if (!(Test-Path -LiteralPath $license)) { throw 'Provide a directory containing your already accepted Android SDK license.' }
New-Item -ItemType Directory -Force -Path (Join-Path $sdkRoot 'licenses'), $downloads | Out-Null
Copy-Item -LiteralPath $license -Destination (Join-Path $sdkRoot 'licenses/android-sdk-license')
foreach ($package in $lock.sdkPackages) {
    if ($Packages.Count -gt 0 -and $Packages -notcontains $package.path) { continue }
    $destination = Join-Path $sdkRoot $package.path
    if (Test-Path -LiteralPath (Join-Path $destination 'source.properties')) { continue }
    $archive = Join-Path $downloads $package.archive
    $valid = (Test-Path -LiteralPath $archive) -and ((Get-FileHash -LiteralPath $archive -Algorithm SHA1).Hash -eq $package.sha1)
    if (!$valid) {
        $curlArgs = @('-L','--fail','--show-error','--retry','3','--retry-all-errors','-o',$archive)
        if (Test-Path -LiteralPath $archive) { $curlArgs += @('-C','-') }
        & curl.exe @curlArgs "https://dl.google.com/android/repository/$($package.archive)"
        if ($LASTEXITCODE -ne 0) { throw "Download failed: $($package.archive). Rerun to resume." }
        if ((Get-FileHash -LiteralPath $archive -Algorithm SHA1).Hash -ne $package.sha1) { throw "Checksum mismatch: $archive. Archive retained for diagnosis." }
    }
    & $Python (Join-Path $PSScriptRoot 'extract-android-sdk.py') $archive $destination
    if ($LASTEXITCODE -ne 0) { throw "Extraction failed: $archive" }
}
Write-Output "Android SDK: $sdkRoot"
