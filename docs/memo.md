# メモ

## .vscode/setting.json

このプロジェクトは`"C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",` ではダメ。

```json
{
    // IntelliSense: not CMake Tools. The build is driven from ../megasource
    // with this repository reachable only through the megasource/libs/love
    // junction, so CMake's codemodel names every source by that path and
    // never matches a file opened here. The include list below is what
    // build/love/liblove.vcxproj carries, spelled relative to this folder.
    "C_Cpp.default.configurationProvider": "",
    "C_Cpp.default.compilerPath": "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/cl.exe",
    "C_Cpp.default.intelliSenseMode": "windows-msvc-x64",
    "C_Cpp.default.cStandard": "c11",
    "C_Cpp.default.cppStandard": "c++17",
    "C_Cpp.default.includePath": [
        "${workspaceFolder}/src",
        "${workspaceFolder}/src/libraries",
        "${workspaceFolder}/src/libraries/box2D",
        "${workspaceFolder}/src/modules",
        "${workspaceFolder}/../lhat",
        "${workspaceFolder}/../lhat/include",
        "${workspaceFolder}/build/love/lhat/include",
        "${workspaceFolder}/../megasource/libs/SDL3/include",
        "${workspaceFolder}/build/SDL3/include-revision",
        "${workspaceFolder}/../megasource/libs/openal-soft/include",
        "${workspaceFolder}/../megasource/libs/openal-soft/include/AL",
        "${workspaceFolder}/../megasource/libs/zlib-1.3.1",
        "${workspaceFolder}/build/zlib",
        "${workspaceFolder}/../megasource/libs/freetype/include",
        "${workspaceFolder}/build/freetype/include",
        "${workspaceFolder}/../megasource/libs/harfbuzz/src",
        "${workspaceFolder}/../megasource/libs/libmodplug-0.8.8.4/src",
        "${workspaceFolder}/../megasource/libs/libmodplug-0.8.8.4/src/libmodplug",
        "${workspaceFolder}/build/libmodplug",
        "${workspaceFolder}/../megasource/libs/libvorbis-1.3.5/include",
        "${workspaceFolder}/../megasource/libs/libvorbis-1.3.5/lib",
        "${workspaceFolder}/../megasource/libs/libogg-1.3.2/include",
        "${workspaceFolder}/../megasource/libs/libtheora-1.1.1/include",
        "${workspaceFolder}/../megasource/libs/libtheora-1.1.1/lib"
    ],
    "C_Cpp.default.defines": [
        "WIN32",
        "_WINDOWS",
        "liblove_EXPORTS"
    ],
    "files.associations": {
        "*.lh": "lhat"
    }
}
```
