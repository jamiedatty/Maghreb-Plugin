cmake_minimum_required(VERSION 3.20)
project(CLion_Projects)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(DATA)
include_directories(include)
include_directories(include/nlohmann)
include_directories(lib)
include_directories(src)
include_directories(src/FMP)
include_directories(src/S-Mode)
include_directories(src/VersionControl)

add_library(CLion_Projects MODULE
        include/nlohmann/json.hpp
        include/curl.h
        include/EuroScopePlugIn.h
        include/IndraApcInterop.h
        include/S-Mode.h
        include/TopskyFunctions.h
        src/FMP/FMP.h
        src/FMP/FMPMain.cpp
        src/S-Mode/S-Mode.cpp
        src/S-Mode/S-Mode.h
        src/VersionControl/Version.cpp
        src/VersionControl/Version.h
        src/plugin.cpp
        src/S-Mode/S-Mode.cpp
        src/S-Mode/S-Mode.h)

set_target_properties(CLion_Projects PROPERTIES
        PREFIX ""
        SUFFIX ".dll"
        OUTPUT_NAME "IndraApc")

if (MSVC)
    target_compile_definitions(CLion_Projects PRIVATE
            _CRT_SECURE_NO_WARNINGS
            WIN32_LEAN_AND_MEAN)

    target_compile_options(CLion_Projects PRIVATE
            /EHsc)

    target_link_libraries(CLion_Projects PRIVATE
            "${CMAKE_SOURCE_DIR}/lib/EuroScopePlugInDll.lib"
            user32
            gdi32
            gdiplus
            winhttp
            ws2_32)

    target_link_options(CLion_Projects PRIVATE
            /SUBSYSTEM:WINDOWS
            /MACHINE:X86
            /EXPORT:EuroScopePlugInInit
            /EXPORT:EuroScopePlugInExit)
endif()