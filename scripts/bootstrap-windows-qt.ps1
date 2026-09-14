# Fetch the Qt 6.11.2 x64 SDK from the official component repository.
# Qt 6.11 uses nested per-architecture repositories.
[CmdletBinding()]
param([string]$Destination = 'C:\Qt\6.11.2\msvc2022_64')
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$base = 'https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_6112/qt6_6112_msvc2022_64/'
[xml]$metadata = (Invoke-WebRequest ($base + 'Updates.xml') -UseBasicParsing).Content
New-Item -ItemType Directory -Force $Destination | Out-Null
$downloads = Join-Path $env:TEMP 'Mindarchy-Qt-6.11.2'
New-Item -ItemType Directory -Force $downloads | Out-Null
foreach ($package in $metadata.Updates.PackageUpdate) {
    if ($package.Name -notin @('qt.qt6.6112.win64_msvc2022_64','qt.qt6.6112.addons.qtshadertools.win64_msvc2022_64')) { continue }
    foreach ($archive in ($package.DownloadableArchives -split ', ')) {
        if ($archive -notmatch '^(qtbase-|qtdeclarative-|qtsvg-|qttools-|qtshadertools-|d3dcompiler_|opengl32sw-)') { continue }
        $url = $base + $package.Name + '/' + $package.Version + $archive
        $file = Join-Path $downloads $archive
        $checksum = (Invoke-WebRequest ($url + '.sha1') -UseBasicParsing).Content
        if ($checksum -is [byte[]]) { $checksum = [Text.Encoding]::UTF8.GetString($checksum) }
        $expected = ($checksum.Trim() -split '\s+')[0]
        if (!(Test-Path $file)) { Invoke-WebRequest $url -OutFile $file -UseBasicParsing }
        if ((Get-FileHash $file -Algorithm SHA1).Hash -ne $expected) { throw "Checksum mismatch: $archive" }
        & tar.exe -xf $file -C $Destination
        if ($LASTEXITCODE -ne 0) { throw "Cannot extract $archive with Windows tar" }
    }
}
foreach ($required in @('bin\qmake.exe', 'bin\windeployqt.exe', 'lib\cmake\Qt6\Qt6Config.cmake')) {
    if (!(Test-Path (Join-Path $Destination $required))) { throw "Incomplete Qt SDK: $required is missing" }
}
'[Paths]' + "`r`n" + 'Prefix=..' | Set-Content (Join-Path $Destination 'bin\qt.conf')
$licenses = Join-Path $Destination 'LICENSES'
New-Item -ItemType Directory -Force $licenses | Out-Null
$licenseFiles = Invoke-RestMethod 'https://api.github.com/repos/qt/qtbase/contents/LICENSES?ref=v6.11.2'
foreach ($license in $licenseFiles) {
    if ($license.type -eq 'file') {
        $target = Join-Path $licenses $license.name
        for ($attempt = 1; $attempt -le 3; $attempt++) {
            try {
                Invoke-WebRequest $license.download_url -OutFile ($target + '.download') -UseBasicParsing
                Move-Item ($target + '.download') $target -Force
                break
            } catch {
                if ($attempt -eq 3) { throw }
                Start-Sleep -Seconds 2
            }
        }
    }
}
Write-Host "Qt SDK: $Destination"
