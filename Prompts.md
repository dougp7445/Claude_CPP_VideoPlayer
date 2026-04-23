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

#### No edits

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