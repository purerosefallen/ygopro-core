newoption { trigger = "lua-dir", description = "", value = "PATH", default = "./lua" }
newoption { trigger = "wasm", description = "" }
newoption { trigger = "mac-arm", description = "" }

function GetParam(param)
    return _OPTIONS[param] or os.getenv(string.upper(string.gsub(param,"-","_")))
end

LUA_DIR=GetParam("lua-dir")
if not os.isdir(LUA_DIR) then
    LUA_DIR="../lua"
end

WASM = GetParam("wasm")

workspace "ocgcoredll"
    location "build"
    language "C++"
    cppdialect "C++14"
    configurations { "Release", "Debug" }
    if WASM then
        toolset "emcc"
        platforms { "wasm" }
    elseif os.istarget("macosx") then
        if GetParam("mac-arm") then
            platforms { "arm64" }
        else
            platforms { "x64" }
        end
    else 
        platforms { "x32", "x64" }
    end
    
    filter "platforms:x32"
        architecture "x32"

    filter "platforms:x64"
        architecture "x64"

    filter "configurations:Release"
        optimize "Speed"

    filter "configurations:Debug"
        symbols "On"
        defines "_DEBUG"

    filter "system:windows"
        defines { "WIN32", "_WIN32" }
        systemversion "latest"
        startproject "ocgcore"

    filter { "configurations:Release", "action:vs*" }
        if linktimeoptimization then
            linktimeoptimization "On"
        else
            flags { "LinkTimeOptimization" }
        end
        staticruntime "On"
        disablewarnings { "4334" }

    filter "action:vs*"
        buildoptions { "/utf-8" }
        defines { "_CRT_SECURE_NO_WARNINGS" }

    filter "not action:vs*"
        buildoptions { }

    filter "system:bsd"
        defines { "LUA_USE_POSIX" }

    filter "system:macosx"
        defines { "LUA_USE_MACOSX" }

    filter "system:linux"
        defines { "LUA_USE_LINUX" }
        buildoptions { "-fPIC" }

    filter "system:emscripten"
        defines { "LUA_USE_LONGJMP", "LUA_USE_C89" }
        buildoptions { "-fPIC" }

filter {}

include(LUA_DIR)

project "ocgcore"

    kind "SharedLib"
    cppdialect "C++14"

    files { "*.cpp", "*.h" }
    links { "lua" }
    
    includedirs { LUA_DIR .. "/src" }

    filter "system:emscripten"
        targetextension ".wasm"
        linkoptions { "-s MODULARIZE=1", "-s EXPORT_NAME=\"createOcgcore\"", "--no-entry", "-s EXPORTED_FUNCTIONS=[\"_set_script_reader\",\"_set_card_reader\",\"_set_message_handler\",\"_create_duel\",\"_start_duel\",\"_end_duel\",\"_set_player_info\",\"_get_log_message\",\"_get_message\",\"_process\",\"_new_card\",\"_new_tag_card\",\"_query_card\",\"_query_field_count\",\"_query_field_card\",\"_query_field_info\",\"_set_responsei\",\"_set_responseb\",\"_preload_script\"]", "-s ENVIRONMENT=web,node", "-s EXPORTED_RUNTIME_METHODS=[\"ccall\",\"cwrap\",\"addFunction\",\"removeFunction\"]", "-s ALLOW_TABLE_GROWTH=1", "-s ALLOW_MEMORY_GROWTH=1", "-o ../wasm/libocgcore.js" }
