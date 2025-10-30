param(
  [ValidateSet('Debug','RelWithDebInfo','Release','MinSizeRel')]
  [string]$Config = 'RelWithDebInfo',

  # Ajusta si tu vcpkg está en otra ruta:
  [string]$VcpkgRoot = 'D:/Repositorios Github/vcpkg'
)

$ErrorActionPreference = 'Stop'

$shaderc = Join-Path $VcpkgRoot 'installed/x64-windows/tools/bgfx/shaderc.exe'
if (!(Test-Path $shaderc)) { throw "No encuentro shaderc.exe en: $shaderc" }

$root = (Get-Location).Path
$outDir = Join-Path $root "build/bin/$Config/shaders/dx11"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$varying = Join-Path $root 'assets/shaders/varying.def.sc'

# Include de bgfx instalado por vcpkg (para <bgfx/bgfx_shader.sh>)
$inc1 = Join-Path $VcpkgRoot 'installed/x64-windows/include'

# ===== Shader básico =====
$vsBasicIn  = Join-Path $root 'assets/shaders/vs_basic.sc'
$fsBasicIn  = Join-Path $root 'assets/shaders/fs_basic.sc'
$vsBasicOut = Join-Path $outDir 'vs_basic.bin'
$fsBasicOut = Join-Path $outDir 'fs_basic.bin'

Write-Host "Compilando vs_basic -> $vsBasicOut" -ForegroundColor Cyan
& "$shaderc" -f "$vsBasicIn" -o "$vsBasicOut" --type v --platform windows --profile s_5_0 --varyingdef "$varying" -i "$inc1" -i "$root/assets/shaders"

Write-Host "Compilando fs_basic -> $fsBasicOut" -ForegroundColor Cyan
& "$shaderc" -f "$fsBasicIn" -o "$fsBasicOut" --type f --platform windows --profile s_5_0 --varyingdef "$varying" -i "$inc1" -i "$root/assets/shaders"

# ===== Shader de debug lines =====
$vsDebugIn  = Join-Path $root 'assets/shaders/vs_debugline.sc'
$fsDebugIn  = Join-Path $root 'assets/shaders/fs_debugline.sc'
$vsDebugOut = Join-Path $outDir 'vs_debugline.bin'
$fsDebugOut = Join-Path $outDir 'fs_debugline.bin'

Write-Host "Compilando vs_debugline -> $vsDebugOut" -ForegroundColor Green
& "$shaderc" -f "$vsDebugIn" -o "$vsDebugOut" --type v --platform windows --profile s_5_0 --varyingdef "$varying" -i "$inc1" -i "$root/assets/shaders"

Write-Host "Compilando fs_debugline -> $fsDebugOut" -ForegroundColor Green
& "$shaderc" -f "$fsDebugIn" -o "$fsDebugOut" --type f --platform windows --profile s_5_0 --varyingdef "$varying" -i "$inc1" -i "$root/assets/shaders"

Write-Host "`nOK. Binarios compilados en: $outDir" -ForegroundColor Yellow
Write-Host "Shaders generados:" -ForegroundColor Yellow
Write-Host "  - vs_basic.bin" -ForegroundColor Gray
Write-Host "  - fs_basic.bin" -ForegroundColor Gray
Write-Host "  - vs_debugline.bin" -ForegroundColor Gray
Write-Host "  - fs_debugline.bin" -ForegroundColor Gray