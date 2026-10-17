cmake_minimum_required(VERSION 3.20)
project(MickeyDonaldPort CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")   # static CRT: no redistributable needed

if(MSVC)
    add_compile_options(/W3 /utf-8 /permissive- /MP)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " /O2 /Oi /GL")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " /LTCG")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE " /LTCG")
endif()

# ---- emulation core -----------------------------------------------------------
add_library(mdcore STATIC
    src/core/m68k.cpp
    src/core/z80.cpp
    src/core/vdp.cpp
    src/core/ym2612.cpp
    src/core/psg.cpp
    src/core/md.cpp
    src/game/common.cpp
    src/game/coi.cpp
    src/game/qs.cpp
    src/game/woi.cpp
)
target_include_directories(mdcore PUBLIC src)

option(MDC_PS4 "Build OpenOrbis bring-up runner" OFF)
option(MDC_HEADLESS "Build portable ROM smoke test and CPU tests" OFF)
if(MDC_PS4 OR MDC_HEADLESS)
    add_library(mdc_session STATIC src/app/session.cpp src/app/palette.cpp
        src/app/audio_mixer.cpp src/app/settings.cpp platform/ps4/settings_io.cpp)
    target_link_libraries(mdc_session PUBLIC mdcore)
    if(MDC_PS4)
        add_subdirectory(platform/ps4)
    else()
        enable_testing()
        foreach(cpu m68k z80)
            add_executable(${cpu}_test tests/${cpu}_test.cpp)
            target_link_libraries(${cpu}_test PRIVATE mdcore)
            add_test(NAME ${cpu} COMMAND ${cpu}_test)
        endforeach()
        add_executable(ps4_state_store_test tests/ps4_state_store_test.cpp platform/ps4/state_store.cpp)
        add_test(NAME ps4_state_store COMMAND ps4_state_store_test)
        add_executable(mdc_smoke platform/ps4/smoke.cpp platform/ps4/state_store.cpp)
        target_link_libraries(mdc_smoke PRIVATE mdc_session)
        find_package(Threads REQUIRED)
        add_executable(ps4_audio_test tests/ps4_audio_test.cpp platform/ps4/audio.cpp)
        target_include_directories(ps4_audio_test PRIVATE tests/ps4_mock)
        target_link_libraries(ps4_audio_test PRIVATE Threads::Threads)
        add_test(NAME ps4_audio COMMAND ps4_audio_test)
        add_executable(ps4_presentation_test tests/ps4_presentation_test.cpp platform/ps4/presentation.cpp)
        target_include_directories(ps4_presentation_test PRIVATE tests/ps4_mock)
        add_test(NAME ps4_presentation COMMAND ps4_presentation_test)
    endif()
    return()
endif()

# ---- the game -----------------------------------------------------------------
add_executable(DisneyClassics WIN32
    src/app/main.cpp
    src/app/app.cpp
    src/app/app_win.cpp
    src/app/app_menus.cpp
    src/app/launcher.cpp
    src/app/jukebox.cpp
    src/app/session.cpp
    src/app/settings.cpp
    src/app/palette.cpp
    src/app/audio_mixer.cpp
    src/app/romfind.cpp
    src/app/sys_win.cpp
    src/app/unzip.cpp
    src/app/monitors.cpp
    src/app/png.cpp
    src/app/rewind.cpp
    src/app/ui.cpp
    src/platform/video.cpp
    src/platform/audio.cpp
    src/platform/input.cpp
    res/app.rc
)
target_link_libraries(DisneyClassics PRIVATE mdcore
    user32 gdi32 ole32 d3d11 dxgi d3dcompiler xaudio2 xinput dinput8 dxguid dwrite winmm dwmapi shell32)
set_target_properties(DisneyClassics PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin-debug)

# ---- analysis DLL for tools/mdt.py ---------------------------------------------
add_library(mdtool SHARED src/tools/mdtool.cpp)
target_link_libraries(mdtool PRIVATE mdcore)
set_target_properties(mdtool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/tools
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/tools)
