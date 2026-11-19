@echo off
rem Load a GLB in the UE 5.7 editor (Interchange) and render capture images
rem usage: check.bat <input.glb> <output folder>
setlocal
if "%~2"=="" (
    echo usage: check.bat ^<input.glb^> ^<output folder^>
    exit /b 2
)
if "%UE_ROOT%"=="" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.7"

set "CHECK_GLB=%~f1"
set "CHECK_OUT=%~f2"

rem Synchronous texture/mesh compilation and no streaming, otherwise captures come out grey
"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0ConvertProj\ConvertProj.uproject" ^
    -run=pythonscript -script="%~dp0check_glb_ue.py" ^
    -unattended -nosplash -nop4 -AllowCommandletRendering ^
    "-dpcvars=Editor.AsyncTextureCompilation=0,Editor.AsyncStaticMeshCompilation=0,r.TextureStreaming=0" ^
    -stdout -FullStdOutLogOutput
exit /b %ERRORLEVEL%
