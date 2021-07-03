@echo off

REM You will need Build Tools for Visual Studio 2019 (or a full installation of Visual Studio 2019)
REM Link: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2019
REM Make sure you're in a Developer Command Prompt for VS 2019, or that you have found the well-hidden vcvarsall.bat
REM and executed it from a regular command prompt (not PowerShell) before running this script. Make sure you're running
REM the 32-bit toolchain as well.

echo - Build started...

REM Setup the compiler and linker flags

set CompilerFlags=/nologo /std:c++latest /permissive- /Zc:inline /Zc:threadSafeInit- /Zc:forScope /Zc:__cplusplus /O1 /Oi /GR- /GS- /Gs9999999 /EHa- /MD /W4 /WX /Zl /arch:SSE2 /I"include/" /D"WIN32" /D"_HAS_EXCEPTIONS=0"
set LinkerFlags=/nologo /nodefaultlib /entry:_main /machine:x86 /stack:0x100000,0x100000 /largeaddressaware /incremental:no /opt:ref /opt:icf /manifest:no /dynamicbase:no /fixed /safeseh:no

REM Setup temporary directory and output directory if needed

if not exist "tmp\" mkdir "tmp\"
if not exist "tmp\obj\" mkdir "tmp\obj\"
if not exist "tmp\exe\" mkdir "tmp\exe\"
if not exist "out\" mkdir "out\"

REM Clear temporary directory and output directory before starting

del "tmp\*.*" /s /f /q >nul
del "out\*.*" /f /q >nul

REM Compile the game

echo - Compiling the game (Release)

cl.exe %CompilerFlags% /D"NDEBUG" /Fo"tmp/obj/game.obj" /c src/test.cpp

if %errorlevel% neq 0 exit /b %errorlevel%

echo - Compiling the game (Debug)

cl.exe %CompilerFlags% /D"_DEBUG" /Zi /Fo"tmp/obj/game_d.obj" /Fd"tmp/obj/game_d.pdb" /c src/test.cpp

if %errorlevel% neq 0 exit /b %errorlevel%

REM Link the game

echo - Linking the game (Release)

link.exe %LinkerFlags% /subsystem:windows /out:tmp/exe/game.exe tmp/obj/game.obj kernel32.lib user32.lib gdi32.lib opengl32.lib 1>nul

if %errorlevel% neq 0 exit /b %errorlevel%

echo - Linking the game (Debug)

link.exe /debug:full %LinkerFlags% /subsystem:console /out:tmp/exe/game_d.exe /pdb:tmp/exe/game_d.pdb tmp/obj/game_d.obj kernel32.lib user32.lib gdi32.lib opengl32.lib 1>nul

if %errorlevel% neq 0 exit /b %errorlevel%

REM Move executable to the out directory

move /Y "tmp\exe\game.exe" "out\game.exe" >nul
move /Y "tmp\exe\game_d.exe" "out\game_d.exe" >nul
move /Y "tmp\exe\game_d.pdb" "out\game_d.pdb" >nul

echo - Done -^> %~dp0out\game.exe
