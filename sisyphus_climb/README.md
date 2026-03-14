# Project Overview
## Title: Sisyphus Climb
A typing-based visual novel/action hybrid exploring the myth of Sisyphus. Players must type correctly to push the stone up the mountain while escaping a chasing Demon.

## Current Functionality (v0.5 Progress)
- The core "Engine" of the game is now stable. The following systems are implemented and functional:
- Game State Machine: Seamless transitions between Title Screen, Gameplay, and Ending screens.
- Typing System: Real-time input detection, accuracy calculation, and word-count tracking.
- Movement Physics: Sisyphus moves relative to typing progress; the Demon chases the player with adaptive speed.
- Asset Management: Dynamic loading of textures and music with cross-platform path handling (Windows/macOS).
- Visuals: Background parallax scrolling and UI overlays for stats.
Note on Assets: Most visual and audio assets are currently placeholders. They represent the intended mood but will be replaced with custom/final versions in future iterations.

## Compilation Instructions
To ensure this project runs for reviewers, use the following specifications:
#### Windows (Target Platform)
- Compiler: GCC (recommended via w64devkit i686-w64-mingw32).
- Architecture: 32-bit (win32).
- Required Files: main.c, raylib.h, raymath.h, libraylib.a.
- Command used to compile exe file: `gcc main.c -o SisyphusGame.exe -L. -lraylib -lopengl32 -lgdi32 -lwinmm -static`
To ensure the game loads all assets (textures, music, and story data) correctly, please follow these steps:
1. Navigate to the sisyphus_climb folder.
2. Ensure the assets folder is in the same directory as SisyphusGame.exe.
3. Double-click SisyphusGame.exe.
Note: If Windows Defender shows a blue "Protected your PC" popup, click "More Info" and then "Run Anyway." This is normal for self-compiled unsigned programs.
#### macOS
- Compiler: Clang (cc).
- Requirements: Raylib installed via Homebrew (brew install raylib).
- Command used to compile exe file: `cc main.c -o SisyphusClimb -lraylib -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL`


