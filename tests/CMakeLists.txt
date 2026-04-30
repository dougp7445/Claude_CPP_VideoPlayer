include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# Prevent GoogleTest from overriding the parent project's compiler/linker settings on Windows.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# Logger Tests
add_executable(LoggerTests LoggerTests.cpp ../src/Logger.cpp)

target_include_directories(LoggerTests PRIVATE ../include)

target_link_libraries(LoggerTests PRIVATE gtest_main)

gtest_discover_tests(LoggerTests)


# Encoder Tests
add_executable(EncoderTests
    EncoderTests.cpp
    ../src/Encoder.cpp
    ../src/Logger.cpp
)

target_include_directories(EncoderTests PRIVATE
    ../include
    "${FFMPEG_DIR}/include"
)

target_link_libraries(EncoderTests PRIVATE
    gtest_main
    "${FFMPEG_DIR}/lib/avcodec.lib"
    "${FFMPEG_DIR}/lib/avformat.lib"
    "${FFMPEG_DIR}/lib/avutil.lib"
    "${FFMPEG_DIR}/lib/swresample.lib"
)

add_custom_command(TARGET EncoderTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FFMPEG_DLLS}
        $<TARGET_FILE_DIR:EncoderTests>
)

gtest_discover_tests(EncoderTests)


# Decoder Tests
add_executable(DecoderTests DecoderTests.cpp ../src/Decoder.cpp ../src/Demuxer.cpp ../src/Logger.cpp)

target_include_directories(DecoderTests PRIVATE
    ../include
    "${FFMPEG_DIR}/include"
)

target_link_libraries(DecoderTests PRIVATE
    gtest_main
    "${FFMPEG_DIR}/lib/avcodec.lib"
    "${FFMPEG_DIR}/lib/avformat.lib"
    "${FFMPEG_DIR}/lib/avutil.lib"
    "${FFMPEG_DIR}/lib/swscale.lib"
    "${FFMPEG_DIR}/lib/swresample.lib"
)

# Pass the resources path so tests can locate test.mp4 without a relative path.
target_compile_definitions(DecoderTests PRIVATE
    TEST_RESOURCES_DIR="${CMAKE_SOURCE_DIR}/resources"
)

# FFmpeg DLLs must be next to the test binary for the runtime to find them.
add_custom_command(TARGET DecoderTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FFMPEG_DLLS}
        $<TARGET_FILE_DIR:DecoderTests>
)

gtest_discover_tests(DecoderTests)


# FileOperations Tests
add_executable(FileOperationsTests FileOperationsTests.cpp ../src/FileOperations.cpp)

target_include_directories(FileOperationsTests PRIVATE ../include)

target_link_libraries(FileOperationsTests PRIVATE gtest_main)

gtest_discover_tests(FileOperationsTests)


# Renderer Tests
add_executable(RendererTests
    RendererTests.cpp
    ../src/Decoder.cpp
    ../src/Demuxer.cpp
    ../src/Logger.cpp
    ../src/MenuUI.cpp
    ../src/PlayerUI.cpp
    ../src/Renderer.cpp
)

target_include_directories(RendererTests PRIVATE
    ../include
    "${FFMPEG_DIR}/include"
    "${SDL_DIR}/include"
    "${NANOSVG_DIR}"
)

target_link_libraries(RendererTests PRIVATE
    gtest_main
    "${FFMPEG_DIR}/lib/avcodec.lib"
    "${FFMPEG_DIR}/lib/avformat.lib"
    "${FFMPEG_DIR}/lib/avutil.lib"
    "${FFMPEG_DIR}/lib/swscale.lib"
    "${FFMPEG_DIR}/lib/swresample.lib"
    "${SDL_DIR}/lib/x64/SDL3.lib"
)

target_compile_definitions(RendererTests PRIVATE
    TEST_RESOURCES_DIR="${CMAKE_SOURCE_DIR}/resources"
)

# FFmpeg and SDL DLLs must be next to the test binary for the runtime to find them.
add_custom_command(TARGET RendererTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FFMPEG_DLLS}
        "${SDL_DIR}/lib/x64/SDL3.dll"
        $<TARGET_FILE_DIR:RendererTests>
)

gtest_discover_tests(RendererTests)


# VideoPlayer Tests
add_executable(VideoPlayerTests
    VideoPlayerTests.cpp
    ../src/Decoder.cpp
    ../src/Demuxer.cpp
    ../src/DialogWindow.cpp
    ../src/Encoder.cpp
    ../src/EncoderSettingsPanel.cpp
    ../src/ExportSettingsDialog.cpp
    ../src/FileOperations.cpp
    ../src/Logger.cpp
    ../src/MenuUI.cpp
    ../src/PlayerUI.cpp
    ../src/RecentFiles.cpp
    ../src/Renderer.cpp
    ../src/VideoPlayer.cpp
)

target_include_directories(VideoPlayerTests PRIVATE
    ../include
    "${FFMPEG_DIR}/include"
    "${SDL_DIR}/include"
    "${NANOSVG_DIR}"
)

target_link_libraries(VideoPlayerTests PRIVATE
    gtest_main
    "${FFMPEG_DIR}/lib/avcodec.lib"
    "${FFMPEG_DIR}/lib/avformat.lib"
    "${FFMPEG_DIR}/lib/avutil.lib"
    "${FFMPEG_DIR}/lib/swscale.lib"
    "${FFMPEG_DIR}/lib/swresample.lib"
    "${SDL_DIR}/lib/x64/SDL3.lib"
)

target_compile_definitions(VideoPlayerTests PRIVATE
    TEST_RESOURCES_DIR="${CMAKE_SOURCE_DIR}/resources"
)

add_custom_command(TARGET VideoPlayerTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FFMPEG_DLLS}
        "${SDL_DIR}/lib/x64/SDL3.dll"
        $<TARGET_FILE_DIR:VideoPlayerTests>
)

add_custom_command(TARGET VideoPlayerTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources/icons"
        "$<TARGET_FILE_DIR:VideoPlayerTests>/icons"
)

gtest_discover_tests(VideoPlayerTests)
