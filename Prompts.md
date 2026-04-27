# Claude Prompts Log

---

## Branch: 04-26-2026-Logging

### Prompt 1
> Create a class called Logger.cpp with logging functions

#### Files Modified
- `include/Logger.h` — created singleton Logger with Debug/Info/Warning/Error levels, `init()`, and level methods
- `src/Logger.cpp` — implemented singleton, timestamped log entries, file + console output, mutex thread safety
- `CMakeLists.txt` — added `src/Logger.cpp` to VideoPlayer sources
- `tests/CMakeLists.txt` — added `../src/Logger.cpp` to RendererTests and VideoPlayerTests

### Prompt 2
> Add logging for errors

#### Files Modified
- `src/Decoder.cpp` — replaced 5 `std::cerr` calls with `Logger::instance().error()`; swapped `<iostream>` for `"Logger.h"`
- `src/Renderer.cpp` — replaced 5 `std::cerr` and 1 `std::cout` with Logger calls; swapped `<iostream>` for `"Logger.h"`
- `src/main.cpp` — replaced `std::cerr` with `Logger::instance().error()`; swapped `<iostream>` for `"Logger.h"`
- `tests/CMakeLists.txt` — added `../src/Logger.cpp` to DecoderTests (now required after Decoder.cpp uses Logger)

### Prompt 3
> Set the file path for the logger to be located in a folder called Log in the folder with the executable when running.

#### Files Modified
- `src/Logger.cpp` — added `std::filesystem::create_directories` in `init()` to create the log directory if missing
- `src/main.cpp` — added `Logger::instance().init()` call using `executableDir() + "Log\\video_player.log"`

### Prompt 4
> Include the day, month, year, hour, and minute in the name of the log file

#### Files Modified
- `src/main.cpp` — build timestamped filename `video_player_DD-MM-YYYY_HH-MM.log` using `<chrono>`/`<ctime>` before calling `init()`

### Prompt 5
> Have an option in the Logger.cpp to set how many log files to keep in the Log folder. Default of 10

#### Files Modified
- `include/Logger.h` — added `int maxFiles = 10` parameter to `init()`
- `src/Logger.cpp` — added pruning logic in `init()`: scans log directory, sorts by modification time, deletes oldest files beyond `maxFiles`; added `<algorithm>` and `<vector>` includes

### Prompt 6
> Add tests for the logger

#### Files Modified
- `src/Logger.cpp` — added `m_file.close()` at start of `init()` so tests can reinitialize the singleton cleanly
- `tests/LoggerTests.cpp` — created with 12 tests covering: singleton identity, file/directory creation, all log levels written to file, level tags, `minLevel` filtering, pruning count, pruning deletes oldest, `maxFiles=0` disables pruning
- `tests/CMakeLists.txt` — added LoggerTests target; moved `include(GoogleTest)` to after `FetchContent_MakeAvailable` so it applies to all targets

### Prompt 7
> Add trace logs, including tests

#### Files Modified
- `include/Logger.h` — added `Trace` to Level enum (below Debug), added `trace()` declaration
- `src/Logger.cpp` — implemented `trace()`, added `"TRACE"` case to `levelTag()`
- `tests/LoggerTests.cpp` — added `TraceMessageWrittenToFile`, `TraceTagPresentInOutput`, `DefaultMinLevelFiltersTrace`

### Prompt 8
> Check logger tests and see if trace needs to be added

#### Files Modified
- `tests/LoggerTests.cpp` — added `trace()` call to `MinLevelFiltersLowerMessages` test (trace was the only level missing from that filter check)

### Prompt 9
> Update Prompts file based on changes to current branch 04-26-2026-Logging. Include this prompt.

#### No edits
