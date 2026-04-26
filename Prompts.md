# Claude Prompts Log

---

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

### Prompt 21
> After every prompt, build and run tests

#### No edits

### Prompt 22
> Do not build and run tests after changes that only affect README.md and Prompts.md

#### No edits

### Prompt 23
> Add header protections to header files

#### No edits

### Prompt 24
> Only include the latest prompt in Prompts.md.  Move older prompts to a PromptsArchive folder with a single ArchivedPrompts.md file.

#### No edits
