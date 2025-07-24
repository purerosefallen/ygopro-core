newoption { trigger = "lua-dir", description = "", value = "PATH", default = "./lua" }
newoption { trigger = "wasm", description = "" }
newoption { trigger = "sqlite3-dir", description = "", value = "PATH" }

function GetParam(param)
    return _OPTIONS[param] or os.getenv(string.upper(string.gsub(param,"-","_")))
end

LUA_DIR=GetParam("lua-dir")
if not os.isdir(LUA_DIR) then
    LUA_DIR="../lua"
end

WASM = GetParam("wasm")

SQLITE3_DIR=GetParam("sqlite3-dir")

workspace "ocgcoredll"
    location "build"
    language "C++"
    cppdialect "C++14"
    configurations { "Release", "Debug" }
    if WASM then
        toolset "emcc"
        platforms { "wasm" }
    else
        platforms { "x64", "x32", "arm64" }
    end
    
    filter "platforms:x32"
        architecture "x32"

    filter "platforms:x64"
        architecture "x64"

    filter "platforms:arm64"
        architecture "ARM64"

    filter "configurations:Release"
        optimize "Speed"

    filter "configurations:Debug"
        symbols "On"
        defines "_DEBUG"

    filter "system:windows"
        systemversion "latest"
        startproject "ocgcore"

    filter { "configurations:Release", "action:vs*" }
        linktimeoptimization "On"
        staticruntime "On"
        disablewarnings { "4334" }

    filter "action:vs*"
        cdialect "C11"
        conformancemode "On"
        buildoptions { "/utf-8" }
        defines { "_CRT_SECURE_NO_WARNINGS" }

    filter "system:bsd"
        defines { "LUA_USE_POSIX" }

    filter "system:macosx"
        defines { "LUA_USE_MACOSX" }

    filter "system:linux"
        defines { "LUA_USE_LINUX" }
        pic "On"
        linkoptions { "-static-libstdc++", "-static-libgcc" }

    filter "system:emscripten"
        defines { "LUA_USE_C89" }
        pic "On"

filter {}

include(LUA_DIR)

project "ocgcore"

    kind "SharedLib"

    files { "*.cpp", "*.h" }
    links { "lua" }
    
    includedirs { LUA_DIR .. "/src" }

    filter "system:emscripten"
        targetextension ".wasm"
        linkoptions { "-s MODULARIZE=1", "-s EXPORT_NAME=\"createOcgcore\"", "--no-entry", "-s ENVIRONMENT=web,node", "-s EXPORTED_RUNTIME_METHODS=[\"ccall\",\"cwrap\",\"addFunction\",\"removeFunction\"]", "-s ALLOW_TABLE_GROWTH=1", "-s ALLOW_MEMORY_GROWTH=1", "-o ../wasm/libocgcore.js" }

if not WASM and SQLITE3_DIR and os.isdir(SQLITE3_DIR) then
project "sqlite3"
    kind "SharedLib"
    language "C"

    files {
        SQLITE3_DIR .. "/sqlite3.c",
        SQLITE3_DIR .. "/sqlite3.h"
    }

    filter "system:windows"
        systemversion "latest"
        defines { "SQLITE_API=__declspec(dllexport)" }

    filter "system:linux or system:macosx"
        pic "On"

    filter "system:linux"
        linkoptions { "-static-libstdc++", "-static-libgcc" }

    filter "configurations:Debug"
        symbols "On"
        defines { "DEBUG" }

    filter "configurations:Release"
        optimize "On"
        defines { "NDEBUG" }
end
