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
$vsIn    = Join-Path $root 'assets/shaders/vs_basic.sc'
$fsIn    = Join-Path $root 'assets/shaders/fs_basic.sc'
$vsOut   = Join-Path $outDir 'vs_basic.bin'
$fsOut   = Join-Path $outDir 'fs_basic.bin'

# Include de bgfx instalado por vcpkg (para <bgfx/bgfx_shader.sh>)
$inc1 = Join-Path $VcpkgRoot 'installed/x64-windows/include'

Write-Host "Compilando VS -> $vsOut"
& "$shaderc" -f "$vsIn" -o "$vsOut" --type v --platform windows --profile s_5_0 --varyingdef "$varying" -i "$inc1" -i "$root/assets/shaders"

Write-Host "Compilando FS -> $fsOut"
& "$shaderc" -f "$fsIn" -o "$fsOut" --type f --platform windows --profile s_5_0 --varyingdef "$varying" -i "$inc1" -i "$root/assets/shaders"

Write-Host "OK. Binarios en: $outDir"
