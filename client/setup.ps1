#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$Msys2Dir = "C:\msys64"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Info { param([string]$Msg) Write-Host "[setup] $Msg" -ForegroundColor Green }
function Step { param([string]$Msg) Write-Host "  -> $Msg" -ForegroundColor Cyan }
function Warn { param([string]$Msg) Write-Host "[warn] $Msg" -ForegroundColor Yellow }
function Die  { param([string]$Msg) Write-Host "[error] $Msg" -ForegroundColor Red; exit 1 }

$ScriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$IncludeDir = Join-Path $ScriptDir "include"
$LibDir     = Join-Path $ScriptDir "lib"

# ------------------------------------------------------------
# Run command inside MSYS2 bash
# ------------------------------------------------------------
function Invoke-Msys2Bash {
    param([string]$Command, [switch]$IgnoreExit)

    $bash = Join-Path $Msys2Dir "usr\bin\bash.exe"
    if (-not (Test-Path $bash)) { Die "MSYS2 bash not found" }

    # escape double quotes for bash
    $escaped = $Command -replace '"','\"'

    $arg = "--login -c `"$escaped`""

    $proc = Start-Process -FilePath $bash `
        -ArgumentList $arg `
        -Wait -PassThru -NoNewWindow

    if (-not $IgnoreExit -and $proc.ExitCode -ne 0) {
        Die "MSYS2 command failed: $Command"
    }
}

# ------------------------------------------------------------
# Install MSYS2 (only if missing)
# ------------------------------------------------------------
function Install-Msys2 {
    if (Test-Path (Join-Path $Msys2Dir "usr\bin\bash.exe")) {
        Info "MSYS2 already installed at $Msys2Dir"
        return
    }

    Info "Downloading MSYS2 installer..."

    $ApiUrl  = "https://api.github.com/repos/msys2/msys2-installer/releases/latest"
    $Headers = @{ "User-Agent" = "setup.ps1" }

    $Release = Invoke-RestMethod -Uri $ApiUrl -Headers $Headers
    $Asset = $Release.assets | Where-Object { $_.name -like "*x86_64*.exe" } | Select-Object -First 1

    if (-not $Asset) { Die "MSYS2 installer not found" }

    $Installer = Join-Path $env:TEMP $Asset.name

    Step "Downloading MSYS2..."
    Invoke-WebRequest -Uri $Asset.browser_download_url -OutFile $Installer

    Step "Installing MSYS2..."
    Start-Process -FilePath $Installer -ArgumentList "/S /D=$Msys2Dir" -Wait

    Info "MSYS2 installed"
}

# ------------------------------------------------------------
# Toolchain install
# ------------------------------------------------------------
function Install-Toolchain {
    Info "Installing MSYS2 dependencies..."

    $packages = @(
        "mingw-w64-x86_64-toolchain"
        "mingw-w64-x86_64-make"

        "mingw-w64-x86_64-SDL2"
        "mingw-w64-x86_64-SDL2_ttf"
        "mingw-w64-x86_64-SDL2_image"
        "mingw-w64-x86_64-SDL2_mixer"
        "mingw-w64-x86_64-mpg123"
        "mingw-w64-x86_64-libvorbis"
        "mingw-w64-x86_64-libogg"

        "mingw-w64-x86_64-openssl"
    )

    $pkgString = $packages -join " "

    Invoke-Msys2Bash "pacman -S --noconfirm --needed $pkgString"

    Info "Toolchain installed"
}

function Setup-BuildEnv {
    Info "Configuring MSYS2 environment (no repo modification)..."

    $mingwBin = Join-Path $Msys2Dir "mingw64\bin"

    if (-not (Test-Path $mingwBin)) {
        Die "MSYS2 mingw64 not found"
    }

    # Ensure compiler/tools are used
    $env:PATH = "$mingwBin;$env:PATH"

    Info "Using MSYS2 toolchain directly"
}

# ------------------------------------------------------------
# PATH setup
# ------------------------------------------------------------
function Link-Msys2ToRepo {
    Info "Linking MSYS2 into repo (no copying)..."

    $mingw = Join-Path $Msys2Dir "mingw64"

    $include = Join-Path $ScriptDir "include"
    $lib     = Join-Path $ScriptDir "lib"

    if (Test-Path $include) { Remove-Item -Recurse -Force $include }
    if (Test-Path $lib)     { Remove-Item -Recurse -Force $lib }

    New-Item -ItemType SymbolicLink -Path $include -Target "$mingw\include" | Out-Null
    New-Item -ItemType SymbolicLink -Path $lib     -Target "$mingw\lib"     | Out-Null

    Info "Symlinks created (repo contains no actual SDL/OpenSSL files)"
}
# ------------------------------------------------------------
# MAIN
# ------------------------------------------------------------
Info "Archcast Windows setup"
Info "MSYS2 dir: $Msys2Dir"
Info ""

Install-Msys2
Install-Toolchain
Setup-BuildEnv

Info ""
Info "DONE. Run:"
Info "  mingw32-make"