[CmdletBinding()]
param(
  [string]$Prefix = "",
  [string]$MicromambaVersion = "2.8.1-0",
  [switch]$NoPersist
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ($env:OS -ne "Windows_NT") {
  throw "This bootstrap is for native Windows hosts."
}

$localAppData = [Environment]::GetFolderPath(
  [Environment+SpecialFolder]::LocalApplicationData)
if ([string]::IsNullOrWhiteSpace($Prefix)) {
  $Prefix = Join-Path $localAppData "Styio\toolchains\llvm18"
}
$Prefix = [IO.Path]::GetFullPath($Prefix)

$vswhere = Join-Path ${env:ProgramFiles(x86)} `
  "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
  throw "Visual Studio Installer discovery tool was not found."
}
$visualStudio = & $vswhere -latest -products * `
  -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
  -property installationPath
if ([string]::IsNullOrWhiteSpace($visualStudio)) {
  throw "Visual Studio 2022 C++ Build Tools with the MSVC x64 toolset are required."
}

$bootstrapRoot = Join-Path $localAppData "Styio\bootstrap"
$micromamba = Join-Path $bootstrapRoot "micromamba.exe"
$micromambaRoot = Join-Path $localAppData "Styio\micromamba"
New-Item -ItemType Directory -Force -Path $bootstrapRoot | Out-Null

$needMicromamba = $true
if (Test-Path -LiteralPath $micromamba) {
  $installedVersion = (& $micromamba --version).Trim()
  $needMicromamba = $installedVersion -ne ($MicromambaVersion -replace "-\d+$", "")
}
if ($needMicromamba) {
  $releaseBase =
    "https://github.com/mamba-org/micromamba-releases/releases/download/$MicromambaVersion"
  $candidate = Join-Path $bootstrapRoot "micromamba.download.exe"
  $checksumFile = Join-Path $bootstrapRoot "micromamba.download.sha256"
  Invoke-WebRequest -Uri "$releaseBase/micromamba-win-64.exe" -OutFile $candidate
  Invoke-WebRequest -Uri "$releaseBase/micromamba-win-64.sha256" -OutFile $checksumFile
  $expectedHash = ((Get-Content -LiteralPath $checksumFile -Raw).Trim() -split "\s+")[0]
  $actualHash = (Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash
  if ($actualHash -ne $expectedHash.ToUpperInvariant()) {
    throw "Micromamba checksum verification failed."
  }
  Move-Item -LiteralPath $candidate -Destination $micromamba -Force
}

$packages = @(
  "cmake=3.31.6",
  "ninja",
  "llvm>=18.1",
  "llvmdev>=18.1",
  "clang-tools>=18.1",
  "zlib=1.3.2",
  "zstd=1.5.7",
  "libxml2-devel=2.15.3"
)
$environmentHistory = Join-Path $Prefix "conda-meta\history"
if (Test-Path -LiteralPath $environmentHistory) {
  & $micromamba install -y -r $micromambaRoot -p $Prefix `
    -c conda-forge --strict-channel-priority @packages
}
else {
  & $micromamba create -y -r $micromambaRoot -p $Prefix `
    -c conda-forge --strict-channel-priority @packages
}
if ($LASTEXITCODE -ne 0) {
  throw "The native Windows LLVM environment transaction failed."
}

$llvmDir = Join-Path $Prefix "Library\lib\cmake\llvm"
$llvmConfig = Join-Path $llvmDir "LLVMConfig.cmake"
$llvmConfigExe = Join-Path $Prefix "Library\bin\llvm-config.exe"
$toolchainRoot = Join-Path $Prefix "Library"
if (-not (Test-Path -LiteralPath $llvmConfig)) {
  throw "LLVMConfig.cmake is missing from the installed LLVM development package."
}
if (-not (Test-Path -LiteralPath $llvmConfigExe)) {
  throw "llvm-config.exe is missing from the installed LLVM development package."
}
$llvmVersion = (& $llvmConfigExe --version).Trim()
if ($llvmVersion -notmatch '^(?<version>\d+(?:\.\d+){1,3})') {
  throw "Cannot parse the installed LLVM version: $llvmVersion."
}
$minimumLlvmVersion = [version]"18.1.0"
$installedLlvmVersion = [version]$Matches.version
if ($installedLlvmVersion -lt $minimumLlvmVersion) {
  throw "LLVM $minimumLlvmVersion or newer is required; found $llvmVersion."
}

$env:LLVM_DIR = $llvmDir
$env:STYIO_NATIVE_TOOLCHAIN_ROOT = $toolchainRoot
if (-not $NoPersist) {
  [Environment]::SetEnvironmentVariable("LLVM_DIR", $llvmDir, "User")
  [Environment]::SetEnvironmentVariable(
    "STYIO_NATIVE_TOOLCHAIN_ROOT", $toolchainRoot, "User")
}

Write-Host "Styio native Windows toolchain is ready."
Write-Host "Host compiler: MSVC x64"
Write-Host "LLVM development package: $llvmVersion"
Write-Host "CMake preset: windows-msvc-llvm"
