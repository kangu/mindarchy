[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$QtDir,
    [string]$Version = '0.1.0',
    [string]$InnoCompiler,
    [string]$BuildDir,
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$source = Split-Path $PSScriptRoot -Parent
if (!$BuildDir) { $BuildDir = Join-Path $source 'build-windows' }
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$stage = Join-Path $BuildDir 'stage'
$output = Join-Path $source 'artifacts\windows'
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw 'Version must be numeric major.minor.patch' }
if (!(Test-Path "$QtDir\bin\windeployqt.exe")) { throw 'QtDir must be the MSVC x64 Qt installation (with windeployqt.exe).' }
if (!$InnoCompiler) {
    $InnoCompiler = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (!$InnoCompiler -or !(Test-Path $InnoCompiler)) { throw 'Pass -InnoCompiler with the existing ISCC.exe path.' }
function Run([string]$Program, [string[]]$Arguments) {
    Get-Command $Program -ErrorAction Stop | Out-Null
    $previousPreference = $ErrorActionPreference
    try {
        # Windows PowerShell 5 treats native stderr as error records. Preserve
        # warnings and decide success using the actual native exit code.
        $ErrorActionPreference = 'Continue'
        & $Program @Arguments
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    if ($code -ne 0) { throw "$Program failed with exit code $code" }
}
# Resolve Visual Studio even in a plain PowerShell window. Always select x64
# when possible, including when the host is Windows ARM64.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = $null
if (Test-Path $vswhere) {
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if ($vs) {
    $devcmd = Join-Path $vs 'Common7\Tools\VsDevCmd.bat'
    cmd /c "`"$devcmd`" -arch=x64 -host_arch=x64 >nul && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
    }
    $cmakeTools = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake'
    $env:PATH = "$cmakeTools\CMake\bin;$cmakeTools\Ninja;$env:PATH"
}
if (!(Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw 'Install Visual Studio Build Tools with Desktop development with C++.'
}
$env:PATH = "$QtDir\bin;$env:PATH"
Run cmake @('-S', $source, '-B', $BuildDir, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=ON', "-DCMAKE_PREFIX_PATH=$QtDir", "-DMINDMAP_VERSION=$Version")
Run cmake @('--build', $BuildDir, '--parallel', '4')
if (!$SkipTests) {
    Run cmake @('--build', $BuildDir, '--target', 'mindarchy-tests', '--parallel', '4')
    Run ctest @('--test-dir', $BuildDir, '--output-on-failure', '--no-tests=error', '-LE', '^native$')
    Run ctest @('--test-dir', $BuildDir, '--output-on-failure', '--no-tests=error', '-L', '^native$') }
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage,$output | Out-Null
Run cmake @('--install', $BuildDir, '--prefix', $stage)
Run "$QtDir\bin\windeployqt.exe" @('--release', '--qmldir', (Join-Path $source 'qml'), '--compiler-runtime', '--no-translations', (Join-Path $stage 'mindarchy.exe'))
# The CLI preview renderer selects the offscreen platform explicitly. windeployqt
# only discovers the normal Windows platform from GUI usage.
Copy-Item (Join-Path $QtDir 'plugins\platforms\qoffscreen.dll') (Join-Path $stage 'platforms') -Force
# App-local MSVC runtime allows standard-user installation without a global redist prerequisite.
if ($env:VCToolsRedistDir) {
    $crt = Get-ChildItem (Join-Path $env:VCToolsRedistDir 'x64\Microsoft.VC*.CRT') -Directory | Select-Object -First 1
    if ($crt) { Copy-Item (Join-Path $crt.FullName '*.dll') $stage -Force }
}
foreach ($dependency in @('Qt6Core.dll','Qt6Gui.dll','Qt6Qml.dll','Qt6Quick.dll','platforms\qwindows.dll','platforms\qoffscreen.dll','msvcp140.dll','vcruntime140.dll')) {
    if (!(Test-Path (Join-Path $stage $dependency))) { throw "Missing deployed dependency: $dependency" }
}
$licenses = Join-Path $stage 'licenses\Qt'
New-Item -ItemType Directory -Force $licenses | Out-Null
if (Test-Path "$QtDir\LICENSES") { Copy-Item "$QtDir\LICENSES\*" $licenses -Recurse }
elseif (Test-Path "$QtDir\..\..\Licenses") { Copy-Item "$QtDir\..\..\Licenses\*" $licenses -Recurse }
else { throw 'Qt license files not found; place the redistributed Qt license texts in QtDir\LICENSES.' }
Copy-Item (Join-Path $source 'packaging\windows\THIRD-PARTY.txt') $stage
Run $InnoCompiler @("/DAppVersion=$Version", "/DStageDir=$stage", "/DOutputPath=$output", (Join-Path $source 'packaging\windows\mindarchy.iss'))
Get-FileHash (Join-Path $output "Mindarchy-$Version-windows-x64-setup.exe") -Algorithm SHA256 | Format-List | Out-String | Set-Content (Join-Path $output 'SHA256.txt')
Write-Host "Installer: $output\Mindarchy-$Version-windows-x64-setup.exe"
