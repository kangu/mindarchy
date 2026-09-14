[CmdletBinding()]
param(
    [string]$App = "$env:LOCALAPPDATA\Programs\Mindarchy\mindarchy.exe",
    [string]$OutputDir = "$env:TEMP\Mindarchy-installed-test"
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force $OutputDir | Out-Null
$App = (Resolve-Path $App).Path
$originalPath = $env:PATH
$env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
$qtEnvironment = @{}
foreach ($name in @('QT_PLUGIN_PATH','QML_IMPORT_PATH','QML2_IMPORT_PATH','QT_QPA_PLATFORM','QT_QPA_PLATFORM_PLUGIN_PATH','QT_QPA_FONTDIR')) {
    $qtEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
    [Environment]::SetEnvironmentVariable($name, $null, 'Process')
}
try {
    $shot = Join-Path $OutputDir 'installed-main-window.png'
    $log = Join-Path $OutputDir 'installed-stderr.log'
    Remove-Item $shot -ErrorAction SilentlyContinue
    $args = @('--nodes','15','--no-window-state','--screenshot',('"' + $shot + '"'),'--quit-after','4000')
    $process = Start-Process $App -ArgumentList $args -PassThru -RedirectStandardError $log
    $null = $process.Handle # Cache the process handle so PowerShell 5 retains ExitCode after exit.
    $loadedModules = @()
    $deadline = (Get-Date).AddSeconds(3)
    do {
        $process.Refresh()
        if ($process.HasExited) { break }
        try { $loadedModules = @($process.Modules | Select-Object -ExpandProperty FileName) } catch {}
        if ($loadedModules.Count -gt 0 -and $process.MainWindowHandle -ne 0) { break }
        Start-Sleep -Milliseconds 100
    } while ((Get-Date) -lt $deadline)
    if ($loadedModules.Count -eq 0) { throw 'Could not inspect installed runtime modules.' }
    foreach ($runtime in @('Qt6Core.dll', 'Qt6Gui.dll', 'vcruntime140.dll', 'msvcp140.dll')) {
        if (!(Join-Path (Split-Path $App -Parent) $runtime | Where-Object { $loadedModules -contains $_ })) {
            throw "Required runtime not loaded from installation: $runtime"
        }
    }
    $loadedModules | ConvertTo-Json | Set-Content (Join-Path $OutputDir 'loaded-modules.json')
    if ($loadedModules | Where-Object { $_ -match '\\Qt\\|\\Visual Studio\\' -and !$_.StartsWith((Split-Path $App -Parent), [StringComparison]::OrdinalIgnoreCase) }) {
        throw 'The installed app loaded a module from a developer installation.'
    }
    if (!$process.WaitForExit(30000)) { $process.Kill(); throw 'Installed application did not exit within 30 seconds.' }
    if ($process.ExitCode -ne 0) { throw "Installed application exited with $($process.ExitCode). See $log" }
    if (!(Test-Path $shot)) { throw "Application did not produce a main-window screenshot. See $log" }
    $errors = Get-Content $log -Raw
    if ($errors -match 'module .* is not installed|failed to load|could not load|QQmlApplicationEngine failed') { throw $errors }
    # Exercise the explicitly selected offscreen platform from the installed tree.
    $documentPath = Join-Path $OutputDir 'installed-test.omm'
    @{format='mindarchy';version=1;themeId='beach-day';layout='Horizontal';spacing='Standard';branchStyle='Rounded';manual=$false;connections=@();nodes=@(@{id=1;parent=-1;children=@();text='Installed runtime check';notes='';folded=$false;task=$false;checked=$false;x=0;y=0})} | ConvertTo-Json -Depth 10 | Set-Content $documentPath -Encoding UTF8
    $previewPath = Join-Path $OutputDir 'installed-preview.png'
    Remove-Item $previewPath -ErrorAction SilentlyContinue
    $previewArgs = @('--render-preview', ('"' + $documentPath + '"'), '--preview-output', ('"' + $previewPath + '"'))
    $previewProcess = Start-Process $App -ArgumentList $previewArgs -PassThru -RedirectStandardError (Join-Path $OutputDir 'installed-preview-stderr.log')
    $null = $previewProcess.Handle
    if (!$previewProcess.WaitForExit(30000)) { $previewProcess.Kill(); throw 'Installed offscreen preview timed out.' }
    if ($previewProcess.ExitCode -ne 0 -or !(Test-Path $previewPath)) { throw 'Installed offscreen preview failed.' }
    @{
        OS = (Get-CimInstance Win32_OperatingSystem).Caption
        OSBuild = (Get-CimInstance Win32_OperatingSystem).BuildNumber
        Architecture = $env:PROCESSOR_ARCHITECTURE
        Executable = $App
        DeveloperPathRemoved = $true
        OffscreenPreview = $previewPath
        ExitCode = $process.ExitCode
        Screenshot = $shot
        TestedAt = (Get-Date).ToString('o')
    } | ConvertTo-Json | Set-Content (Join-Path $OutputDir 'verification.json')
    Write-Host "Installed runtime smoke test passed: $shot"
} finally {
    $env:PATH = $originalPath
    foreach ($name in $qtEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $qtEnvironment[$name], 'Process')
    }
}
