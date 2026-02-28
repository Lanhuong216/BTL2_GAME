@echo off
echo Building game...
cmake --build build
if %errorlevel% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo Copying executable...
copy /Y build\app.exe build\Debug\app.exe

echo Running game...
cd build\Debug
start app.exe
cd ..\..
echo Game started!

