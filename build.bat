@echo off
rem SPDX-License-Identifier: GPL-3.0-or-later
rem Builds build\bi_lumen.lv2 and the test programs using Visual Studio's x64 C compiler.
rem Test programs go in build-test, so build\ holds only the bundle for the LV2 parser to scan.
setlocal
set "ROOT=%~dp0"
rem Finds any Visual Studio edition and version with the C++ tools (2017 or later) through
rem vswhere. Set VCVARS yourself to point at a particular vcvars64.bat instead.
if not defined VCVARS (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    for /f "usebackq delims=" %%i in (`call "%%VSWHERE%%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
)
if not exist "%VCVARS%" (
    echo Could not find Visual Studio's C++ tools. Install them, or set VCVARS to vcvars64.bat.
    exit /b 1
)
call "%VCVARS%" >nul || exit /b 1

set "OUT=%ROOT%build"
set "BUNDLE=%OUT%\bi_lumen.lv2"
set "TESTS=%ROOT%build-test"
if not exist "%BUNDLE%" mkdir "%BUNDLE%"
if not exist "%TESTS%" mkdir "%TESTS%"
pushd "%TESTS%" || exit /b 1

rem /MT links the C runtime statically, so hosts don't need a redistributable installed.
cl /nologo /std:c11 /O2 /fp:precise /MT /W4 /LD "%ROOT%src\plugin.c" "%ROOT%src\engine.c" /Fo"%TESTS%\\" /Fe"%BUNDLE%\bi_lumen.dll" /link /NOLOGO /NOIMPLIB /NOEXP
if errorlevel 1 (popd & exit /b 1)
del /q "%BUNDLE%\bi_lumen.lib" "%BUNDLE%\bi_lumen.exp" 2>nul

rem The GUI: a Win32 window drawn by its own software renderer.
cl /nologo /std:c11 /O2 /MT /W4 /LD "%ROOT%src\ui\ui_lv2.c" "%ROOT%src\ui\view.c" /Fo"%TESTS%\\" /Fe"%BUNDLE%\bi_lumen_ui.dll" /link /NOLOGO /NOIMPLIB /NOEXP user32.lib gdi32.lib
if errorlevel 1 (popd & exit /b 1)
del /q "%BUNDLE%\bi_lumen_ui.lib" "%BUNDLE%\bi_lumen_ui.exp" 2>nul

copy /y "%ROOT%bundle\*.ttl" "%BUNDLE%\" >nul || (popd & exit /b 1)
rem Licence, notices and the AI usage statement travel with the bundle.
for %%F in (LICENSE NOTICE.md AI_USAGE.md) do copy /y "%ROOT%%%F" "%BUNDLE%\" >nul || (popd & exit /b 1)

cl /nologo /std:c11 /O2 /fp:precise /MT /W3 "%ROOT%test\test_dsp.c" /Fo"%TESTS%\\" /Fe"%TESTS%\test_dsp.exe" /link /NOLOGO
if errorlevel 1 (popd & exit /b 1)

cl /nologo /std:c11 /O2 /MT /W3 "%ROOT%test\ui_render.c" /Fo"%TESTS%\\" /Fe"%TESTS%\ui_render.exe" /link /NOLOGO
if errorlevel 1 (popd & exit /b 1)

cl /nologo /std:c11 /O2 /MT /W3 "%ROOT%test\ui_check.c" /Fo"%TESTS%\\" /Fe"%TESTS%\ui_check.exe" /link /NOLOGO user32.lib gdi32.lib
if errorlevel 1 (popd & exit /b 1)

cl /nologo /std:c11 /Od /MT "%ROOT%test\lilv_check.c" /Fo"%TESTS%\\" /Fe"%TESTS%\lilv_check.exe" /link /NOLOGO
if errorlevel 1 (popd & exit /b 1)

popd
echo Built %BUNDLE%
echo Davidian Bi-Lumen is free software under GPL-3.0-or-later. It was made with generative AI ^(Claude Code^); see AI_USAGE.md.
