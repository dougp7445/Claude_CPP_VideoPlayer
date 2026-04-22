cmake_minimum_required(VERSION 3.20)
project(VideoPlayer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FFMPEG_DIR "${CMAKE_SOURCE_DIR}/libraries/Windows/FFmpeg")
set(SDL_DIR    "${CMAKE_SOURCE_DIR}/libraries/Windows/SDL")

add_executable(VideoPlayer
    src/main.cpp
    src/Decoder.cpp
    src/PlayerUI.cpp
    src/Renderer.cpp
    src/VideoPlayer.cpp
)

target_include_directories(VideoPlayer PRIVATE
    include
    "${FFMPEG_DIR}/include"
    "${SDL_DIR}/include"
)

target_link_libraries(VideoPlayer PRIVATE
    "${FFMPEG_DIR}/lib/avcodec.lib"
    "${FFMPEG_DIR}/lib/avformat.lib"
    "${FFMPEG_DIR}/lib/avutil.lib"
    "${FFMPEG_DIR}/lib/swscale.lib"
    "${FFMPEG_DIR}/lib/swresample.lib"
    "${SDL_DIR}/lib/x64/SDL3.lib"
)

# Copy DLLs to the output directory after build
set(FFMPEG_DLLS
    "${FFMPEG_DIR}/lib/avcodec-61.dll"
    "${FFMPEG_DIR}/lib/avformat-61.dll"
    "${FFMPEG_DIR}/lib/avutil-59.dll"
    "${FFMPEG_DIR}/lib/swscale-8.dll"
    "${FFMPEG_DIR}/lib/swresample-5.dll"
)

add_custom_command(TARGET VideoPlayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FFMPEG_DLLS}
        "${SDL_DIR}/lib/x64/SDL3.dll"
        $<TARGET_FILE_DIR:VideoPlayer>
)
