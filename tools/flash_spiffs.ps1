# Build SPIFFS image with assets/first.pcm and flash to the storage partition.
# Requires ESP-IDF environment (idf.py / esptool on PATH, IDF_PATH set).
param(
  [string]$Port = "COM7",
  [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if (-not $Root) {
  $Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

$spiffsDir = Join-Path $Root "build\spiffs_image"
$img = Join-Path $Root "build\spiffs.bin"
$pcm = Join-Path $Root "assets\first.pcm"
$partSize = 0xDF0000
$partOffset = 0x210000

if (-not (Test-Path $pcm)) {
  throw "Missing $pcm - convert First.mp3 first (u8 mono 22050)."
}

if (-not $env:IDF_PATH) {
  throw "IDF_PATH not set; activate ESP-IDF first."
}

New-Item -ItemType Directory -Force -Path $spiffsDir | Out-Null
Copy-Item -Force $pcm (Join-Path $spiffsDir "first.pcm")

$spiffsgen = Join-Path $env:IDF_PATH "components\spiffs\spiffsgen.py"
if (-not (Test-Path $spiffsgen)) {
  throw "spiffsgen.py not found under IDF_PATH."
}

Write-Host "Building SPIFFS image..."
python $spiffsgen $partSize $spiffsDir $img
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$offHex = ("0x{0:X}" -f $partOffset)
Write-Host "Flashing SPIFFS $img to $offHex on $Port"
esptool.py --chip esp32 -p $Port -b 460800 write_flash --flash_mode dio --flash_freq 40m --flash_size 16MB $partOffset $img
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "SPIFFS flash OK"
