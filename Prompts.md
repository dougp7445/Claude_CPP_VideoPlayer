# Claude Prompts Log

---

## Branch: 04-22-2025-Readme (Note bad branch name should be 2026)

### Prompt 1
> Update the README.md to tell a new developer how to build and run this project on Windows

#### Minor editing for file path and some other nits I ran into.

### Prompt 2
> Put the Building and Running sections under a For Windows section in the README.md

#### No edits

### Prompt 3
> Update Prompts.md with Claude prompts used for the 04-22-2025-Readme branch

#### Minor editing as it included a previous prompt and did not include the prompt to actually update the Prompts.md file.

## Branch: 04-22-25-LibraryUpdates (Note bad branch name should be 2026)

### Prompt 1
> Move the FFMpeg libraries and SDL Libraries into a directory under libraries labeled Windows and update cmake file for library links

#### No edits

### Prompt 2
> Check and update the README and Prompts files based on changes.  Include this prompt.

#### Minor edits to Readme to reflect OS agnostic edits.  Minor edits to Prompts file to include branch changes.

## Branch: 04-22-2026-UseSVGIcons

### Prompt 1
> Update the Mute / Speaker icon with the volume svgs located in the resources/icons directory

#### No edits

### Prompt 2
> Update the build to use nanosvg.h and nanosvgrast.h in the libraries directory

#### No edits

### Prompt 3
> (Build error) C2440: cannot convert from 'const char *' to 'char *'

#### Pasted error from previous prompt that led to this prompt.  Resolved the issue.

### Prompt 4
> Check and update the README and Prompts files based on changes to current branch.  Include this prompt.

#### Minor edits to Readme to make it easier to find github for svg. Minor edits to Prompts file to include branch changes.

## Branch: 4-11-2026-DecoderComments (Pay attention to branch names)

### Prompt 1
> Add comments to Decoder.cpp

#### Minor formatting edits.

### Prompt 2
> Update Prompts file based on changes to current branch 4-11-2026-DecoderComments.  Include this prompt.

#### No edits

## Branch: 4-22-2026-AddTests

### Prompt 1
> Add new Cmake project in a directory called tests to run GoogleTests for Decoder.cpp

#### No edits

### Prompt 2
> Add script called runTests.sh that will run unit tests

#### No edits

### Prompt 3
> Update README.md with information on how to run tests

#### No edits

### Prompt 4
> Add tests for Renderer.cpp

#### Added comments in CMakeLists.txt to show where Decoder and Renderer tests begin.

### Prompt 5
> (Build error) C2361: initialization of 'target' is skipped by 'default' label

#### Changed target to be initialized outside of switch

### Prompt 6
> The following tests FAILED: 22 - RendererSDLTest.AudioClockAfterInitIsNonNegative (Failed)

#### getAudioClock() changed back to original prompt

### Prompt 7
> The following tests FAILED: 22 - RendererSDLTest.AudioClockAfterInitIsNonNegative (Failed) is still failing

#### Root cause was m_audioStream being null when no audio device available. Fixed by adding SkipIfNoAudio() guard to audio-dependent tests.

### Prompt 8
> Update the RendererSDLTest to use the test.mp4 file

#### No edits

### Prompt 9
> Add tests for renderer without opening TEST_VIDEO

#### No edits

### Prompt 10
> Update Prompts file based on changes to current branch 4-22-2026-AddTests.  Include this prompt.

#### Multiple edits to describe user edits.  Left Prompt 7 alone because description was accurate to problem.

## Branch: 4-22-2026-Constants

### Prompt 1
> Add a constants file to main project and another constants file for the test project

#### No edits

### Prompt 2
> Add constants for video dimensions

#### No edits

### Prompt 3
> Add other standard video resolutions to constants

#### No edits

### Prompt 4
> Update Prompts file based on changes to current branch 4-22-2026-Constants.  Include this prompt.

#### No edits

### Prompt 5
> Add contants for common video bitrates

#### Removed ' deliminator

### Prompt 6
> Update Prompts.md

#### No edits

### Prompt 7
> Add common audio sample rates and bytes per sample to constants

#### No edits

### Prompt 8

> Consolidate contants to remove duplicates

#### Rejected change as Claude wanted to keep both duplicate values instead of replacing one with the other

### Prompt 9
> Add common audio channels to constants

#### User rejected keeping AUDIO_CHANNELS and redirected to replace it with CHANNELS_STEREO everywhere instead.

### Prompt 10
> Update Prompts.md

#### Multiple prompts missed (7 and 8) and added in.

## Branch: 4-26-2026-OpenFiles

### Prompt 1
> Allow a user to select a video file to play in the player

#### No edits

### Prompt 2
> Add a UI menu element to open files

#### No edits

### Prompt 3
> Getting this error when opening a file with a video already playing: SDL_CreateRenderer failed: Renderer already associated with window

#### Pasted build error. Fixed by guarding SDL_CreateRenderer, SDL_CreateTexture, and audio stream creation with null checks in initRenderer().

### Prompt 4
> Make a menu ui bar that appears at the top of the window

#### No edits

### Prompt 5
> When open file is canceled, do not close the player and keep the previous file loaded

#### No edits

### Prompt 6
> Remove the open file from the player ui but not from the menu bar

#### No edits

### Prompt 7
> Move the menu ui to its own file called MenuUI.cpp

#### No edits

### Prompt 8
> (Build error) LNK2019: unresolved external symbol MenuUI::render / handleMouseClick / handleMouseMotion

#### Pasted linker error. Fixed by adding MenuUI.cpp to RendererTests sources in tests/CMakeLists.txt.

### Prompt 9
> Have test project inherit from main target

#### Undone by next prompt.

### Prompt 10
> Undo last prompt

#### This turned into a massive change that was not wanted here.  Should of realized it beforehand.

### Prompt 11
> save the last 10 most recent files opened into a file for reference. do not allow duplicates

#### No edits

### Prompt 12
> change the output of the most recent files to a json format

#### No edits

### Prompt 13
> Add a ui element to the menu that allows the user to see the most recent files and open one

#### No edits

### Prompt 14
> Add an option to the Menu ui called Recent Files

#### No edits

### Prompt 15
> Move file opening functions to its own file called FileOperations.cpp

#### No edits

### Prompt 16
> Add tests for file operations

#### No edits

### Prompt 17
> Do not include the file picker in tests

#### This would of included user input, defeating the purpose of the unit tests.

### Prompt 18
> always use braces with if statements

#### This wasn't perfect and required manual fixing later.

### Prompt 19
> Clean up main.cpp to use FileOperations.cpp openFileDialog

#### No edits

### Prompt 20
> Update Prompts file based on changes to current branch 4-26-2026-OpenFiles. Include this prompt.

#### Minor notes editing.