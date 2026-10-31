<#
.SYNOPSIS
    编译并烧录 T12 / HLW 焊台固件（pyOCD + CMSIS-DAP）。

.PARAMETER Button
    烧录三键版固件；不加此参数则烧录 EC11 编码器版。

.PARAMETER Auto
    烧录开机即用版固件（上电自动开机）；不能与 -Button 同时使用。

.PARAMETER Build
    烧录前强制重新编译（默认只在 bin 不存在时编译）。

.EXAMPLE
    .\flash.ps1
    .\flash.ps1 -Button
    .\flash.ps1 -Auto
    .\flash.ps1 -Button -Build
#>
param(
    [switch]$Button,
    [switch]$Auto,
    [switch]$Build
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot

if ($Button -and $Auto) { throw '-Button 与 -Auto 不能同时使用' }

$envName  = if ($Auto) { 'hc32f460jeta-auto' } elseif ($Button) { 'hc32f460jeta-btn' } else { 'hc32f460jeta' }
$buildDir = Join-Path $root ".pio\build\$envName"
$binPath  = Join-Path $buildDir 'firmware.bin'

Push-Location $root
try {
    if (-not (Get-Command pio -ErrorAction SilentlyContinue)) {
        throw '未找到 pio，请先安装 PlatformIO（pip install platformio 或在 VS Code 装 PlatformIO IDE）'
    }
    if (-not (Get-Command pyocd -ErrorAction SilentlyContinue)) {
        throw '未找到 pyocd，请先执行：pip install pyocd'
    }

    if ($Build -or -not (Test-Path $binPath)) {
        Write-Host "[1/2] 编译固件环境：$envName" -ForegroundColor Cyan
        pio run -e $envName
        if ($LASTEXITCODE -ne 0) { throw '编译失败，已终止烧录' }
    }
    else {
        Write-Host "[1/2] 使用已有固件：$binPath" -ForegroundColor DarkGray
    }

    $bin = Get-Item $binPath
    Write-Host ("[2/2] 烧录 {0}（{1} 字节）" -f $bin.Name, $bin.Length) -ForegroundColor Cyan
    pyocd load --target hc32f460xe -O connect_mode=under-reset $bin.FullName
    if ($LASTEXITCODE -ne 0) { throw '烧录失败：请检查 CMSIS-DAP 连接、SWD 接线与主板供电' }

    Write-Host '烧录完成，请断电重新上电确认屏幕点亮。' -ForegroundColor Green
}
catch {
    Write-Host "错误：$($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
finally {
    Pop-Location
}
