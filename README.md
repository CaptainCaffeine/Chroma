# Chroma

Chroma is a Game Boy, Game Boy Color, and Game Boy Advance emulator that strives for accuracy and readability. It is currently capable of playing most commercial games quite well, but is lacking some features common in other emulators like savestates and cheat code support.

Chroma has only been tested on Linux; however, it should work on both macOS and FreeBSD, I just don't have systems set up on which to test it. If you try Chroma on other operating systems, please let me know if it works or if there are any issues! There isn't anything in particular that would prevent it from working on Windows, I just haven't tried compiling it with MSVC.

Chroma is licensed under the GPLv3.

### Dependencies
At the moment, Chroma's only dependencies are fmt and SDL 2.0.5. fmt is bundled as a submodule, so you need to clone the repo with `git clone --recursive` to clone it as well. If you did not clone the repo recursively, you can clone the fmt submodule by running `git submodule update --init`.

* Linux: Install SDL2 from your distribution's repositories.
* macOS: Install SDL2 through homebrew: `brew install sdl2`.
* Windows: Download SDL2 from their [website](https://www.libsdl.org/download-2.0.php).

### Building
A C++17 compiler is required to build Chroma (GCC 8 or Clang 6 and above). I only regularly test whatever versions are currently in the Arch Linux repos, so older compilers will break from time to time.

`mkdir build`

`cd build`

`cmake -DCMAKE_BUILD_TYPE=Release ..`

`make`


### Controls

| Button     | Key        |
| ---------- | ---------- |
| Up         | W          |
| Left       | A          |
| Down       | S          |
| Right      | D          |
| A          | K          |
| B          | J          |
| L          | H          |
| R          | L          |
| Start      | Enter/I    |
| Select     | U          |

| Command    | Key        |
| ---------- | ---------- |
| Quit       | Ctrl+Q     |
| Pause      | P          |
| Fullscreen | V          |
| Screenshot | T          |
