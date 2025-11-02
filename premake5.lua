project "ocgcore"
    if OCGCORE_DYNAMIC then
        kind "SharedLib"
        links { LUA_LIB_NAME }
        libdirs { LUA_LIB_DIR }
    else
        kind "StaticLib"
    end

    files { "*.cpp", "*.h" }
    
    if BUILD_LUA then
        includedirs { "../lua/src" }
    else
        includedirs { LUA_INCLUDE_DIR }
    end

    filter "not action:vs*"
        cppdialect "C++14"

    filter "system:bsd"
        defines { "LUA_USE_POSIX" }

    filter "system:macosx"
        defines { "LUA_USE_MACOSX" }

    filter "system:linux"
        defines { "LUA_USE_LINUX" }
        if OCGCORE_DYNAMIC then
            pic "On"
            linkoptions { "-static-libstdc++", "-static-libgcc" }
        end
