@echo off

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

SET includes=/Isrc /I%VULKAN_SDK%\Include
SET links=/link /LIBPATH:%VULKAN_SDK%\Lib vulkan-1.lib user32.lib
SET defines=/D DEBUG /D PONGGINE

echo "Building main..."

for /r %%f in (*.cpp) do (
    echo Compiling %%f...
    cl /EHsc /c %includes% %defines% /std:c++20 "%%f" /Fo"%%~nf.obj" 
)

link %links% /OUT:main.exe *.obj