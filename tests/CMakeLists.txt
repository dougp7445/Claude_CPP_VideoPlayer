include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# Prevent GoogleTest from overriding the parent project's compiler/linker settings on Windows.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(DecoderTests DecoderTests.cpp ../src/Decoder.cpp)

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

include(GoogleTest)
gtest_discover_tests(DecoderTests)
