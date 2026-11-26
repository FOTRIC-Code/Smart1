@echo off
rem 仅更新设备 app_bin（Windows 原生版）。用法: update_app_bin.cmd 192.168.123.100 /usr/bin/net_app
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0update_app_bin.ps1" %*
