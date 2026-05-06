LUA_DIR = "./lua"

newoption { trigger = "lua-dir", description = "", value = "PATH" }
newoption { trigger = "sqlite3-dir", description = "", value = "PATH" }
newoption { trigger = "ndk-dir", category = "YGOPro - android", description = "", value = "PATH" }
newoption { trigger = "android-api-level", category = "YGOPro - android", description = "", value = "LEVEL" }
newoption { trigger = "no-longjmp", description = "Disable use of longjmp for error handling in Lua" }

boolOptions = {
    "no-lua-safe",
}

for _, boolOption in ipairs(boolOptions) do
    newoption { trigger = boolOption, category = "YGOPro - options", description = "" }
end

function GetParam(param)
    return _OPTIONS[param] or os.getenv(string.upper(string.gsub(param,"-","_")))
end

LUA_DIR = GetParam("lua-dir") or LUA_DIR
if not os.isdir(LUA_DIR) then
    LUA_DIR = "../lua"
end

SQLITE3_DIR=GetParam("sqlite3-dir")
USE_LONGJMP=not GetParam("no-longjmp")
ANDROID_NDK_DIR=GetParam("ndk-dir")

function QuoteIfNeeded(value)
    if string.find(value, " ", 1, true) then
        return "\"" .. value .. "\""
    end
    return value
end

function FindAndroidToolchainBin(ndkDir)
    local prebuiltDir = path.join(ndkDir, "toolchains/llvm/prebuilt")
    local prebuilts = os.matchdirs(path.join(prebuiltDir, "*"))
    table.sort(prebuilts)
    if #prebuilts == 0 then
        error("Android NDK toolchain not found under " .. prebuiltDir)
    end
    return path.join(prebuilts[1], "bin")
end

ANDROID_ENABLED=false
ANDROID_API_LEVEL_TEXT=GetParam("android-api-level") or "26"
ANDROID_API_LEVEL=tonumber(ANDROID_API_LEVEL_TEXT)
if not ANDROID_API_LEVEL then
    error("Invalid android api level: " .. ANDROID_API_LEVEL_TEXT)
end
if ANDROID_NDK_DIR then
    ANDROID_NDK_DIR=path.getabsolute(ANDROID_NDK_DIR)
    if not os.isdir(ANDROID_NDK_DIR) then
        error("Android NDK directory not found: " .. ANDROID_NDK_DIR)
    end
    ANDROID_ENABLED=true
    ANDROID_TOOLCHAIN_BIN=FindAndroidToolchainBin(ANDROID_NDK_DIR)
    ANDROID_TARGET="aarch64-linux-android" .. ANDROID_API_LEVEL
    premake.override(premake.tools.clang, "gettoolname", function(base, cfg, tool)
        if cfg.system == premake.ANDROID then
            if tool == "cc" then
                return QuoteIfNeeded(path.join(ANDROID_TOOLCHAIN_BIN, "clang")) .. " --target=" .. ANDROID_TARGET
            elseif tool == "cxx" then
                return QuoteIfNeeded(path.join(ANDROID_TOOLCHAIN_BIN, "clang++")) .. " --target=" .. ANDROID_TARGET
            elseif tool == "ar" then
                return QuoteIfNeeded(path.join(ANDROID_TOOLCHAIN_BIN, "llvm-ar"))
            end
        end
        return base(cfg, tool)
    end)
end

function ApplyBoolean(param)
    if GetParam(param) then
        defines { "YGOPRO_" .. string.upper(string.gsub(param,"-","_")) }
    end
end

local workspacePlatforms = { "x64", "x32", "arm64", "wasm_cjs", "wasm_esm" }
if ANDROID_ENABLED then
    table.insert(workspacePlatforms, "android_arm64")
end

workspace "ocgcoredll"
    location "build"
    language "C++"
    cppdialect "C++14"
    configurations { "Release", "Debug" }
    platforms(workspacePlatforms)

    if USE_LONGJMP then
        defines { "LUA_USE_LONGJMP" }
    end

    for _, boolOption in ipairs(boolOptions) do
        ApplyBoolean(boolOption)
    end
    
    filter "platforms:x32"
        architecture "x32"

    filter "platforms:x64"
        architecture "x64"

    filter "platforms:arm64"
        architecture "ARM64"

    filter "platforms:android_arm64"
        architecture "ARM64"
        system "android"
        toolset "clang"
        pic "On"

    filter "configurations:Release"
        optimize "Speed"

    filter "configurations:Debug"
        symbols "On"
        defines "_DEBUG"

    filter "system:windows"
        systemversion "latest"
        startproject "ocgcore"

    filter { "system:windows", "action:vs2026" }
        toolset "v143"

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
        if USE_LONGJMP then
            linkoptions { "-static-libstdc++", "-static-libgcc" }
        end

    filter { "system:android", "language:C++" }
        linkoptions { "-static-libstdc++" }

    filter "platforms:wasm_cjs or platforms:wasm_esm"
        toolset "emcc"
        -- defines { "LUA_USE_C89" }
        pic "On"

filter {}

include(LUA_DIR)

project "ocgcore"

    kind "SharedLib"

    files { "*.cpp", "*.h" }
    links { "lua" }
    
    includedirs { LUA_DIR .. "/src" }

    filter "platforms:wasm_cjs or platforms:wasm_esm"
        -- Avoid -shared so emcc emits JS glue + .wasm.
        kind "ConsoleApp"
        targetprefix "lib"
        local wasmLinkOptions = { 
            "-s MODULARIZE=1", 
            "-s EXPORT_NAME=\"createOcgcore\"", 
            "--no-entry",
            "-s EXIT_RUNTIME=1",
            "-s ENVIRONMENT=web,worker,node", 
            "-s EXPORTED_RUNTIME_METHODS=[\"ccall\",\"cwrap\",\"addFunction\",\"removeFunction\"]", 
            "-s EXPORTED_FUNCTIONS=[\"_malloc\",\"_free\"]", 
            "-s ALLOW_TABLE_GROWTH=1", 
            "-s ALLOW_MEMORY_GROWTH=1",
            "-s ASSERTIONS=0",
            "-s SAFE_HEAP=0",
            "-s DEMANGLE_SUPPORT=0",
        }
        linkoptions(wasmLinkOptions)

    filter "platforms:wasm_esm"
        -- Build as ES module
        targetextension ".mjs"
        linkoptions { "-s EXPORT_ES6=1" }

    filter "platforms:wasm_cjs"
        targetextension ".cjs"

    filter {}

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
