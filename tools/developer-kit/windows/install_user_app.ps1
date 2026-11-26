#requires -Version 5.0
<#
.SYNOPSIS
  部署用户程序到设备并切换 app_bin（Windows 原生版，仅需系统自带 scp/ssh）。
  文件可以是交叉编译的 ELF，也可以是带 #!/usr/bin/python3 shebang 的 Python 脚本。
.EXAMPLE
  .\install_user_app.ps1 192.168.123.100 .\thermal_logger.py
  .\install_user_app.ps1 192.168.123.100 .\build\user_app my_app
  .\install_user_app.ps1 -NoReboot 192.168.123.100 .\thermal_logger.py
  .\install_user_app.ps1 192.168.123.100 .\build\user_app net_app   # 兼容旧流程
#>
param(
    [Parameter(Mandatory=$true, Position=0)][string]$HostName,
    [Parameter(Mandatory=$true, Position=1)][string]$LocalFile,
    [Parameter(Position=2)][string]$RemoteName,
    [string]$User = "root",
    [int]$Port = 22,
    [switch]$NoReboot
)
$ErrorActionPreference = "Stop"

foreach ($cmd in "scp", "ssh") {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
        throw "$cmd 不可用。Windows 10/11 请在「可选功能」中启用 OpenSSH 客户端。"
    }
}

$local = Resolve-Path $LocalFile -ErrorAction Stop
if (-not (Test-Path $local -PathType Leaf)) { throw "文件不存在: $LocalFile" }

if (-not $RemoteName) { $RemoteName = [IO.Path]::GetFileName($local.Path) }
$remotePath = if ($RemoteName.StartsWith("/")) { $RemoteName } else { "/usr/bin/$RemoteName" }
$login = "$User@$HostName"
$sshOpts = @("-o", "StrictHostKeyChecking=accept-new")

Write-Host "[developer-kit] uploading $($local.Path) -> ${login}:$remotePath"
& scp -P $Port @sshOpts $local.Path "${login}:$remotePath"
if ($LASTEXITCODE -ne 0) { throw "scp 上传失败 (exit $LASTEXITCODE)" }

# 一次 ssh 完成：chmod + 更新 app_bin（+ 重启），减少密码输入次数
# 远端逻辑唯一副本：device/set_app_bin.py（三脚本共用）。传输用 cmd /c 输入重定向——
# 载荷按字节原样进入 ssh stdin，避开 PS 5.1 管道给原生 stdin 加 UTF-8 BOM 的实测行为。
$payload = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\device\set_app_bin.py"))

$remoteCmd = "chmod 0755 '$remotePath' && python3 - '$remotePath'"
if (-not $NoReboot) { $remoteCmd += " && reboot" }

Write-Host "[developer-kit] chmod + app_bin -> $remotePath$(if (-not $NoReboot) { '，随后重启' })"
cmd /c "ssh -p $Port -o StrictHostKeyChecking=accept-new $login `"$remoteCmd`" < `"$payload`""
# reboot 会主动断开连接导致非零退出码；区分「断开」与「真失败」不可靠，
# 这里仅告警，由用户通过重启后进程/日志验证。
if ($LASTEXITCODE -ne 0) {
    Write-Warning "ssh 退出码 $LASTEXITCODE（重启导致的连接断开也会报非零）。请验证设备是否已重启且程序已拉起。"
}

Write-Host "[developer-kit] install completed: $remotePath"
