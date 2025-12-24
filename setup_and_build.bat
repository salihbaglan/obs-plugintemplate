@echo off
setlocal EnableDelayedExpansion

echo [INFO] Checking for CMake...
where cmake >nul 2>nul
if !ERRORLEVEL! NEQ 0 (
    echo [WARN] CMake not found in PATH. Attempting to install via winget...
    winget install -e --id Kitware.CMake --accept-source-agreements --accept-package-agreements
    
    REM Check if installed successfully regardless of winget exit code (which can be weird)
    REM Try to find it in common locations
    set "CMAKE_DEFAULT_PATH=C:\Program Files\CMake\bin"
    if exist "!CMAKE_DEFAULT_PATH!\cmake.exe" (
        echo [INFO] CMake found at !CMAKE_DEFAULT_PATH!. Adding to PATH...
        set "PATH=!CMAKE_DEFAULT_PATH!;!PATH!"
    ) else (
        echo [WARN] Could not find CMake in standard locations after install attempting.
        echo [INFO] If the installation was successful, you may need to restart this setup script or your terminal.
        pause
        exit /b 1
    )
) else (
    echo [INFO] CMake found in PATH.
)

echo [INFO] Checking for Visual Studio 2022 C++ Tools...
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_INSTALLED=0"
if exist "!VSWHERE!" (
    for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VS_INSTALLED=1"
        echo [INFO] Found Visual Studio at: %%i
    )
)

if "!VS_INSTALLED!" NEQ "1" (
    echo [WARN] Visual Studio 2022 with C++ Tools not found.
    echo [INFO] Attempting to install Visual Studio 2022 Build Tools...
    echo [NOTE] This is a large installation and may take some time. You may see a UAC prompt.
    winget install -e --id Microsoft.VisualStudio.2022.BuildTools --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    
    if !ERRORLEVEL! NEQ 0 (
        echo [ERROR] Failed to install Visual Studio Build Tools.
        pause
        exit /b 1
    )
    echo [INFO] Visual Studio Build Tools installed.
)

echo [INFO] Checking for OBS Studio...
set "OBS_PATH=C:\Program Files\obs-studio"
if exist "%OBS_PATH%" (
    echo [INFO] Found OBS Studio at %OBS_PATH%
    set "CMAKE_PREFIX_PATH=%OBS_PATH%;%CMAKE_PREFIX_PATH%"
) else (
    echo [WARN] OBS Studio not found at default location: %OBS_PATH%
    echo [INFO] CMake will try to find it in standard system paths.
)

echo [INFO] Configuring project (Preset: windows-x64)...
cmake --preset windows-x64
if !ERRORLEVEL! NEQ 0 (
    echo [ERROR] CMake configuration failed.
    pause
    exit /b 1
)

echo [INFO] Building project (Preset: windows-x64)...
cmake --build --preset windows-x64
if !ERRORLEVEL! NEQ 0 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

echo [SUCCESS] Build completed successfully.
pause

