Simple C++ SDL2 Platformer

Endless mini-platformer built with C++ + SDL2.
Stick to moving platforms, dodge the spike floor, and survive as long as you can.

https://github.com/jamesg00/SimpleC-SDL2Game

 Features

Sticky platforms — land from above → you ride the platform (no bounce).

Clean jumping — jump while standing on platforms or ground.

Endless scroll — randomized platforms continuously float downward.

Spike row hazard — touch the red spikes → instant reset.

MM:SS timer — survival time shows top-left & in the window title.

Tiny footprint — runs with just core SDL2 (no fonts needed).

 Controls

Enter → start game (from title screen)

← / → or A / D → move

Space / ↑ → jump (works on platforms)

R → reset to title

 Build (Windows / MinGW + SDL2)

Prereqs:

g++ (MinGW)

SDL2 development package (MinGW)

Project layout (expected):

.
├─ main.cpp
├─ art/              # optional images (not required for core build)
├─ audio/            # optional sounds (not required for core build)
└─ src/
   ├─ include/SDL2/  # SDL2 headers (SDL.h, etc.)
   └─ lib/           # libSDL2.dll.a, libSDL2main.a, etc.


Compile (core SDL2 only):

g++ -std=c++17 main.cpp -I"src\include" -L"src\lib" -lmingw32 -lSDL2main -lSDL2 -o game.exe
game.exe
<img width="311" height="334" alt="Screenshot 2025-11-09 203924" src="https://github.com/user-attachments/assets/26e71cd3-2135-4cdd-be92-bf682d848447" />


<img width="326" height="352" alt="image" src="https://github.com/user-attachments/assets/3e5e86a1-0773-4e13-a661-52929889c91a" />


