project "ocgcore"
    kind "StaticLib"

    files { "*.cpp", "*.h" }
    links { "lua" }
    
    if BUILD_LUA then
        includedirs { "../lua" }
    end

    filter "action:vs*"
        if not BUILD_LUA then
            includedirs { "../lua" }
        end

    filter "not action:vs*"
        buildoptions { "-std=c++14" }

    filter "system:bsd"
        defines { "LUA_USE_POSIX" }

    filter "system:macosx"
        defines { "LUA_USE_MACOSX" }
        if not BUILD_LUA then
            includedirs { "../lua" }
        end

    filter "system:linux"
        defines { "LUA_USE_LINUX" }
        if not BUILD_LUA then
            includedirs { "/usr/include/lua5.3" }
        end
