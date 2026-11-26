@echo off
rem Windows 原生部署入口：仅需系统自带 OpenSSH（scp/ssh），无需 Git Bash / WSL / Python。
rem 用法: install_user_app.cmd 192.168.123.100 samples\thermal_logger\thermal_logger.py
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install_user_app.ps1" %*
