@echo off
rem Asset server: creates .venv on first run, then starts uvicorn on port 8000 (override: run_server.bat 9000)
setlocal
cd /d "%~dp0"
set PORT=%1
if "%PORT%"=="" set PORT=8000

if not exist ".venv\Scripts\python.exe" (
    echo [setup] creating .venv
    python -m venv .venv || exit /b 1
    rem Closed network: put wheels from "pip download -r requirements.txt -d wheels" in .\wheels first
    if exist "wheels" (
        .venv\Scripts\python.exe -m pip install --no-index --find-links wheels -r requirements.txt || exit /b 1
    ) else (
        .venv\Scripts\python.exe -m pip install -r requirements.txt || exit /b 1
    )
)

echo Swagger UI: http://localhost:%PORT%/docs
.venv\Scripts\python.exe -m uvicorn main:app --host 0.0.0.0 --port %PORT%
