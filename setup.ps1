<#  Tremolo VST3 - setup and build  #>
$ErrorActionPreference = "Stop"
$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltinRole]::Administrator)
if (-not $isAdmin) { Write-Host "  ERROR: Run as Administrator." -ForegroundColor Red; Read-Host; exit 1 }

function Write-Step($m){ Write-Host "`n>>> $m" -ForegroundColor Cyan }
function Write-OK($m)  { Write-Host "    [OK] $m"    -ForegroundColor Green }
function Refresh-Path  { $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine")+";"+[System.Environment]::GetEnvironmentVariable("Path","User") }
function Find-Exe($name,[string[]]$fb){ Refresh-Path; $f=Get-Command $name -EA SilentlyContinue; if($f){return $f.Source}; foreach($p in $fb){if(Test-Path $p){$env:Path+=";$(Split-Path $p -Parent)";return $p}}; return $null }
function Install-Pkg($id,$dn,[string]$ov=""){ Write-Host "    Installing $dn..." -ForegroundColor Yellow; $a=@("install","--id",$id,"--silent","--accept-package-agreements","--accept-source-agreements"); if($ov){$a+=@("--override",$ov)}; $p=Start-Process -FilePath "winget" -ArgumentList $a -Wait -PassThru -NoNewWindow; if($p.ExitCode -notin @(0,-1978335189)){throw "$dn install failed."}; Write-OK "$dn installed" }
function Get-Gen($ins){ foreach($i in ($ins|Sort-Object{[version]$_.installationVersion} -Desc)){ switch(([version]$i.installationVersion).Major){ 17{"Visual Studio 17 2022";return} 16{"Visual Studio 16 2019";return} 15{"Visual Studio 15 2017";return} } }; return $null }

try {
    Clear-Host
    Write-Host "  ============================================================" -ForegroundColor Cyan
    Write-Host "   TREMOLO VST3 - Automated Setup" -ForegroundColor White
    Write-Host "  ============================================================" -ForegroundColor Cyan
    Read-Host "  Press Enter to begin (Ctrl+C to cancel)"

    Write-Step "1/5 - winget"; if(-not(Get-Command winget -EA SilentlyContinue)){throw "winget missing - install App Installer from Microsoft Store."}; Write-OK "winget ok"
    Write-Step "2/5 - Git"; $git=Find-Exe "git" @("C:\Program Files\Git\cmd\git.exe"); if(-not $git){Install-Pkg "Git.Git" "Git"; $git=Find-Exe "git" @("C:\Program Files\Git\cmd\git.exe"); if(-not $git){throw "Restart Setup.bat."}}; Write-OK $git
    Write-Step "3/5 - CMake"; $cmake=Find-Exe "cmake" @("C:\Program Files\CMake\bin\cmake.exe"); if(-not $cmake){Install-Pkg "Kitware.CMake" "CMake"; $cmake=Find-Exe "cmake" @("C:\Program Files\CMake\bin\cmake.exe"); if(-not $cmake){throw "Restart Setup.bat."}}; Write-OK $cmake
    Write-Step "4/5 - VS C++ Build Tools"
    $vsw="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $gen=$null; $ok=$false
    if(Test-Path $vsw){$raw=& $vsw -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json 2>$null; $ins=$raw|ConvertFrom-Json; if($ins -and $ins.Count){$ok=$true;$gen=Get-Gen $ins; Write-OK "tools found: $gen"}}
    if(-not $ok){Install-Pkg "Microsoft.VisualStudio.2022.BuildTools" "VS 2022 Build Tools" "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"; $raw=& $vsw -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json 2>$null; $ins=$raw|ConvertFrom-Json; if($ins -and $ins.Count){$ok=$true;$gen=Get-Gen $ins}; if(-not $ok){throw "Rerun Setup.bat."}}
    Write-Step "5/5 - Build"
    Set-Location $scriptDir
    if(Test-Path "build\CMakeCache.txt"){Remove-Item "build" -Recurse -Force}
    & cmake -B build -G $gen -A x64; if($LASTEXITCODE -ne 0){throw "CMake configure failed."}
    & cmake --build build --config Release --parallel
    $art="build\Tremolo_artefacts\Release\VST3\Tremolo.vst3"
    if($LASTEXITCODE -ne 0 -and -not(Test-Path $art)){throw "Build failed."}
    if(Test-Path $art){Copy-Item -Recurse -Force $art "$env:CommonProgramFiles\VST3\"}
    Write-Host "`n  SUCCESS! Tremolo installed to %CommonProgramFiles%\VST3\" -ForegroundColor Green
    Read-Host "  Press Enter to close"
} catch { Write-Host "`n  FAILED: $_" -ForegroundColor Red; Read-Host; exit 1 }
