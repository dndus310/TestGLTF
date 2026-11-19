@echo off
rem FBX -> GLB with the UE 5.7 editor (Interchange import + built-in GLTFExporter)
rem usage: convert.bat <input.fbx> <output.glb> [basecolor.png] [normal.png]
rem Needs a GPU: material baking renders, so -nullrhi cannot be used.
setlocal
if "%~2"=="" (
    echo usage: convert.bat ^<input.fbx^> ^<output.glb^> [basecolor.png] [normal.png]
    exit /b 2
)
if "%UE_ROOT%"=="" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.7"

set "FBX2GLB_FBX=%~f1"
set "FBX2GLB_OUT=%~f2"
set "FBX2GLB_BASE="
set "FBX2GLB_NORMAL="
if not "%~3"=="" set "FBX2GLB_BASE=%~f3"
if not "%~4"=="" set "FBX2GLB_NORMAL=%~f4"

"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0ConvertProj\ConvertProj.uproject" ^
    -run=pythonscript -script="%~dp0fbx2glb_ue.py" ^
    -unattended -nosplash -nop4 -AllowCommandletRendering -stdout -FullStdOutLogOutput
exit /b %ERRORLEVEL%
