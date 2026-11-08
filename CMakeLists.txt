cmake_minimum_required(VERSION 3.21)
project(RoadRashTrilogyRecomp VERSION 0.60 LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(RECOMP_ROOT "${CMAKE_SOURCE_DIR}/.tools/engine" CACHE PATH "Pinned GenesisRecomp sources")
set(RUNNER_ROOT "${RECOMP_ROOT}/runner")
include("${RECOMP_ROOT}/cmake/GenesisRecompGenerated.cmake")
set(SDL_ROOT "${RUNNER_ROOT}/external/SDL2/SDL2-2.28.5")
file(GLOB RC_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/.tools/rcheevos/src/client/*.c"
    "${CMAKE_SOURCE_DIR}/.tools/rcheevos/src/api/*.c"
    "${CMAKE_SOURCE_DIR}/.tools/rcheevos/src/runtime/*.c"
    "${CMAKE_SOURCE_DIR}/.tools/rcheevos/src/util/*.c"
    "${CMAKE_SOURCE_DIR}/.tools/rcheevos/src/hash/*.c")
add_library(rcheevos STATIC ${RC_SOURCES})
target_include_directories(rcheevos PUBLIC "${CMAKE_SOURCE_DIR}/.tools/rcheevos/include")
target_compile_definitions(rcheevos PRIVATE _CRT_SECURE_NO_WARNINGS RC_DISABLE_LUA
    RC_HASH_NO_DISC RC_HASH_NO_ENCRYPTED RC_HASH_NO_ZIP RC_CLIENT_SUPPORTS_HASH)
add_library(rr_achievements STATIC src/achievements/ra_service.cpp src/achievements/local_achievements.cpp)
target_include_directories(rr_achievements PUBLIC src/achievements)
target_link_libraries(rr_achievements PUBLIC rcheevos winhttp crypt32)
target_compile_definitions(rr_achievements PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(rr_achievements PRIVATE /utf-8)
add_library(SDL2::SDL2 SHARED IMPORTED)
set_target_properties(SDL2::SDL2 PROPERTIES IMPORTED_IMPLIB "${SDL_ROOT}/lib/x64/SDL2.lib"
    IMPORTED_LOCATION "${SDL_ROOT}/lib/x64/SDL2.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${SDL_ROOT}/include;${SDL_ROOT}/include/SDL2")
genesisrecomp_runner_sources(RUNNER_SOURCES TRACE ON REVERSE_DEBUG OFF)
list(REMOVE_ITEM RUNNER_SOURCES "${RUNNER_ROOT}/main.c")
add_library(rr_runtime STATIC ${RUNNER_SOURCES} "${RUNNER_ROOT}/game_cycles.c" src/presentation/gpu_present.cpp src/presentation/motion_bridge.cpp src/presentation/sprite_geometry.cpp src/presentation/sprite_assets.cpp src/music/remix_player.cpp)
target_include_directories(rr_runtime PUBLIC
    "${CMAKE_SOURCE_DIR}/src/presentation" "${CMAKE_SOURCE_DIR}/src/music" "${CMAKE_SOURCE_DIR}/.tools/librashader/include"
    "${RUNNER_ROOT}" "${RUNNER_ROOT}/include" "${RUNNER_ROOT}/video"
    "${RUNNER_ROOT}/audio" "${RUNNER_ROOT}/external/clowncommon"
    "${RUNNER_ROOT}/external/superzazu" "${RUNNER_ROOT}/external/ymfm/src"
    "${RECOMP_ROOT}/recompiler/src" "${RECOMP_ROOT}/external/m68k-recomp-core/common"
    "${RECOMP_ROOT}/external/m68k-recomp-core/profiles/genesis")
target_compile_definitions(rr_runtime PUBLIC _CRT_SECURE_NO_WARNINGS SDL_MAIN_HANDLED
    GEN_ENABLE_TRACE=1 RECOMP_LAUNCHER=0 GENESIS_HAS_RECOMP_NET=0 SONIC_REVERSE_DEBUG=0
    GENESIS_GAME_VERSION="${PROJECT_VERSION}")
target_sources(rr_runtime PRIVATE src/presentation/road_surface.cpp)
target_sources(rr_runtime PRIVATE src/presentation/sprite_coverage.cpp)
target_sources(rr_runtime PRIVATE src/presentation/road_markings.cpp)
target_link_libraries(rr_runtime PUBLIC SDL2::SDL2 retcomm_rbengine rr_achievements ws2_32 winmm comdlg32 d3d11 dxgi mfplat mfreadwrite mfuuid ole32)
if(MSVC)
    target_compile_options(rr_runtime PRIVATE /W3 /bigobj /utf-8)
endif()

foreach(game rr1 rr2 rr3)
    set(input "${CMAKE_SOURCE_DIR}/.local/build-inputs/${game}")
    if(NOT EXISTS "${input}/game.bin")
        message(STATUS "${game}: no user ROM installed; skipping game target")
        continue()
    endif()
    genesisrecomp_add_generated_sources(GENERATED ${game} "${input}" "game.bin")
    add_executable(${game} "${RUNNER_ROOT}/main.c" src/game/game_spec.c src/game/scene_capture.c src/game/campaign_state.cpp
        ${GENERATED} "${GENERATED_CYCLES}")
    target_sources(${game} PRIVATE src/launcher/app.manifest)
    target_compile_definitions(${game} PRIVATE RR_GAME_${game}=1)
    target_link_libraries(${game} PRIVATE rr_runtime)
    genesisrecomp_runner_target(${game})
    target_include_directories(${game} PRIVATE "${GENERATED_DIR}")
    set_target_properties(${game} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${game}")
    if(MSVC)
        target_compile_options(${game} PRIVATE /W3 /bigobj /utf-8)
    endif()
    add_custom_command(TARGET ${game} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SDL2::SDL2>" "$<TARGET_FILE_DIR:${game}>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_SOURCE_DIR}/.tools/librashader/librashader.dll" "$<TARGET_FILE_DIR:${game}>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_SOURCE_DIR}/.tools/librashader/D3DX9_43.dll" "$<TARGET_FILE_DIR:${game}>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_SOURCE_DIR}/.tools/librashader/D3DCompiler_43.dll" "$<TARGET_FILE_DIR:${game}>"
        VERBATIM)
endforeach()

add_library(rr_controls_backend STATIC "${RUNNER_ROOT}/input_map.c" "${RUNNER_ROOT}/gamepad.c" "${RUNNER_ROOT}/app_config.c")
target_include_directories(rr_controls_backend PUBLIC "${RUNNER_ROOT}" "${RUNNER_ROOT}/include" "${RUNNER_ROOT}/external/clowncommon")
target_compile_definitions(rr_controls_backend PRIVATE _CRT_SECURE_NO_WARNINGS RECOMP_LAUNCHER=0)
target_link_libraries(rr_controls_backend PUBLIC SDL2::SDL2)
add_executable(TrilogyLauncher WIN32 src/launcher/main.cpp src/launcher/controls.cpp src/launcher/install.cpp src/launcher/updates.cpp src/launcher/app.manifest)
enable_language(RC)
configure_file(src/launcher/launcher.rc.in "${CMAKE_BINARY_DIR}/launcher.rc" @ONLY)
configure_file(assets/launcher/app.ico "${CMAKE_BINARY_DIR}/app.ico" COPYONLY)
target_sources(TrilogyLauncher PRIVATE "${CMAKE_BINARY_DIR}/launcher.rc")
target_link_libraries(TrilogyLauncher PRIVATE rr_controls_backend rr_achievements gdiplus comctl32 comdlg32 xinput9_1_0 shell32 ole32 uxtheme)
target_compile_definitions(TrilogyLauncher PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS RR_VERSION="${PROJECT_VERSION}")
target_compile_options(TrilogyLauncher PRIVATE /utf-8 /W3)
set_target_properties(TrilogyLauncher PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_custom_command(TARGET TrilogyLauncher POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SDL2::SDL2>" "$<TARGET_FILE_DIR:TrilogyLauncher>")

enable_testing()
add_executable(controls_test tests/test_controls.cpp)
target_compile_definitions(controls_test PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(controls_test PRIVATE rr_controls_backend)
add_custom_command(TARGET controls_test POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SDL2::SDL2>" "$<TARGET_FILE_DIR:controls_test>")
add_test(NAME controls COMMAND controls_test)
add_executable(presentation_test tests/test_presentation.cpp)
target_include_directories(presentation_test PRIVATE src)
add_test(NAME presentation COMMAND presentation_test)
add_executable(achievements_test tests/test_achievements.cpp)
target_link_libraries(achievements_test PRIVATE rr_achievements)
add_test(NAME achievements COMMAND achievements_test)
add_executable(motion_test tests/test_motion.cpp src/presentation/motion_bridge.cpp)
target_include_directories(motion_test PRIVATE src)
add_test(NAME motion COMMAND motion_test)
add_executable(vdp_test tests/test_vdp.c "${RUNNER_ROOT}/video/genesis_vdp.c")
target_include_directories(vdp_test PRIVATE "${RUNNER_ROOT}/video")
target_compile_definitions(vdp_test PRIVATE GENESIS_BATCHED_PLANES=0)
add_test(NAME vdp COMMAND vdp_test)
add_executable(sprite_geometry_test tests/test_sprite_geometry.cpp src/presentation/sprite_geometry.cpp src/presentation/sprite_assets.cpp)
target_include_directories(sprite_geometry_test PRIVATE src/presentation "${RUNNER_ROOT}/video")
add_test(NAME sprite_geometry COMMAND sprite_geometry_test)
target_sources(sprite_geometry_test PRIVATE src/presentation/road_surface.cpp)
target_sources(sprite_geometry_test PRIVATE src/presentation/road_markings.cpp)
add_executable(organic_scene_test tests/test_organic_scene.cpp src/presentation/sprite_geometry.cpp src/presentation/sprite_assets.cpp src/presentation/road_surface.cpp)
target_include_directories(organic_scene_test PRIVATE src/presentation "${RUNNER_ROOT}/video")
add_test(NAME organic_scene COMMAND organic_scene_test)
add_executable(sprite_coverage_test tests/test_sprite_coverage.cpp src/presentation/sprite_coverage.cpp)
target_include_directories(sprite_coverage_test PRIVATE src/presentation)
add_test(NAME sprite_coverage COMMAND sprite_coverage_test)
target_sources(sprite_geometry_test PRIVATE src/presentation/sprite_coverage.cpp)
target_sources(organic_scene_test PRIVATE src/presentation/sprite_coverage.cpp)
add_executable(split_screen_test tests/test_split_screen.cpp src/presentation/sprite_geometry.cpp
    src/presentation/sprite_assets.cpp src/presentation/road_surface.cpp src/presentation/road_markings.cpp)
target_include_directories(split_screen_test PRIVATE src/presentation "${RUNNER_ROOT}/video")
add_test(NAME split_screen COMMAND split_screen_test)
target_sources(organic_scene_test PRIVATE src/presentation/road_markings.cpp)
target_sources(split_screen_test PRIVATE src/presentation/sprite_coverage.cpp)
add_executable(road_markings_test tests/test_road_markings.cpp src/presentation/road_markings.cpp)
target_include_directories(road_markings_test PRIVATE src/presentation)
add_test(NAME road_markings COMMAND road_markings_test)
add_executable(music_test tests/test_music.cpp src/music/remix_player.cpp)
target_include_directories(music_test PRIVATE src)
target_link_libraries(music_test PRIVATE mfplat mfreadwrite mfuuid ole32)
add_test(NAME music COMMAND music_test)

# Exercise the actual SDK emitter without commercial ROM inputs.
add_executable(layout_codegen_test tests/test_layout_codegen.c
    "${RECOMP_ROOT}/recompiler/src/game_config.c" "${RECOMP_ROOT}/recompiler/src/toml.c")
target_include_directories(layout_codegen_test PRIVATE "${RECOMP_ROOT}/recompiler/src")
target_compile_definitions(layout_codegen_test PRIVATE _CRT_SECURE_NO_WARNINGS)
add_test(NAME layout_codegen COMMAND layout_codegen_test "${CMAKE_BINARY_DIR}/test-layout.c")
add_executable(campaign_state_test tests/test_campaign_state.cpp src/game/campaign_state.cpp)
target_include_directories(campaign_state_test PRIVATE src src/music "${RUNNER_ROOT}/video" "${RUNNER_ROOT}/audio" "${RUNNER_ROOT}/external/superzazu" "${RECOMP_ROOT}/recompiler/src")
target_compile_definitions(campaign_state_test PRIVATE SDL_MAIN_HANDLED _CRT_SECURE_NO_WARNINGS)
target_link_libraries(campaign_state_test PRIVATE rr_controls_backend)
add_test(NAME campaign_state COMMAND campaign_state_test)
