add_rules("mode.debug", "mode.release")
target("ms777")
    set_kind("binary")
    set_languages("c++17")
    set_warnings("all", "error")
    add_includedirs("include")
    add_files("src/*.cpp")
    if is_plat("windows", "mingw", "msys") then
        add_defines("_WIN32_WINNT=0x0601")
        add_defines("SPDLOG_FMT_EXTERNAL", "SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG")
        add_defines("OPENSSL_SUPPRESS_DEPRECATED")
        add_links("crypto", "gflags", "fmt")
        add_ldflags("-static")
        add_syslinks("ws2_32", "mswsock", "shlwapi")
    end
    if is_plat("linux") then
        add_syslinks("pthread")
    end
    if is_mode("debug") then
        add_defines("DEBUG")
        set_symbols("debug")
        set_optimize("none")
    end
    if is_mode("release") then
        add_defines("NDEBUG")
        set_symbols("hidden")
        set_strip("all")
        add_cxflags("-fomit-frame-pointer")
        add_mxflags("-fomit-frame-pointer")
        set_optimize("fastest")
    end
