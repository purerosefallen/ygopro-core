project "ocgcore"
    kind "StaticLib"

    files { "**.cc", "**.cpp", "**.c", "**.h" }
    configuration "windows"
        includedirs { "../lua" }
    configuration "not vs*"
        buildoptions { "-std=c++14" }
    configuration "linux"
        if BUILD_LUA then
            includedirs { "../lua" }
        else
            includedirs { "/usr/include/lua5.3" }
        end
    configuration "macosx" 
        includedirs { "../lua" }
