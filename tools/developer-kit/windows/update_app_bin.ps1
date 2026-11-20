#requires -Version 5.0
<#
.SYNOPSIS
  更新设备 app_bin（Windows 原生版，仅需系统自带 ssh，无需 Git Bash / WSL / Python）。
.EXAMPLE
  .\update_app_bin.ps1 192.168.123.100 /usr/bin/net_app
  .\update_app_bin.ps1 -User root -Port 22 192.168.123.100 /usr/bin/thermal_logger.py
#>
param(
    [Parameter(Mandatory=$true, Position=0)][string]$HostName,
    [Parameter(Mandatory=$true, Position=1)][string]$AppBinPath,
    [string]$User = "root",
    [int]$Port = 22
)
$ErrorActionPreference = "Stop"

if (-not (Get-Command ssh -ErrorAction SilentlyContinue)) {
    throw "ssh 不可用。Windows 10/11 请在「可选功能」中启用 OpenSSH 客户端。"
}

$login = "$User@$HostName"
Write-Host "[developer-kit] updating app_bin on ${login}:${Port} -> $AppBinPath"

# 远端逻辑唯一副本：device/set_app_bin.py（设备侧 python3 原子改写 config.json，三脚本共用）。
# 传输用 cmd /c 输入重定向，载荷按字节原样进入 ssh stdin——PS 5.1 向原生进程管道写字符串
# 会加 UTF-8 BOM（实测 EF BB BF），重定向不经任何编码转换，与 .sh 侧的 `< file` 完全对齐。
$payload = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\device\set_app_bin.py"))
cmd /c "ssh -p $Port -o StrictHostKeyChecking=accept-new $login `"python3 - '$AppBinPath'`" < `"$payload`""
if ($LASTEXITCODE -ne 0) { throw "ssh 更新 app_bin 失败 (exit $LASTEXITCODE)" }

Write-Host "[developer-kit] app_bin updated"
