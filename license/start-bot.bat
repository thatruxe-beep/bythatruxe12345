@echo off
rem 18:32 cheat - Telegram bot launcher (run from Explorer by double click)
cd /d "%~dp0.."
where python >nul 2>nul
if errorlevel 1 (
    echo Python was not found. Install Python 3.10+ from python.org and check "Add to PATH".
    pause
    exit /b 1
)
python -m license.bot.run
pause
