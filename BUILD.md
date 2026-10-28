# How to build Cro-Mag Rally

## The easy way: build.py (automated build script)

`build.py` can produce a game executable from a fresh clone of the repo in a single command. It will work on macOS, Windows and Linux, provided that your system has Python 3, CMake, and an adequate C++ compiler.

```
git clone --recurse-submodules https://github.com/jorio/CroMagRally
cd CroMagRally
python3 build.py
```

If you want to build the game **manually** instead, the following sections describe how to do just that on each of the big 3 desktop operating systems.

Building SDL from source with `BUILD_SDL_FROM_SOURCE=ON` requires CMake 3.24+.

## How to build the game manually on macOS

1. Install the prerequisites:
    - Xcode (preferably the latest version)
    - [CMake](https://formulae.brew.sh/formula/cmake) 3.21+ (installing via Homebrew is recommended)
1. Clone the repo **recursively**:
    ```
    git clone --recurse-submodules https://github.com/jorio/CroMagRally
    cd CroMagRally
    ```
1. Download [SDL3-3.2.8.dmg](https://libsdl.org/release/SDL3-3.2.8.dmg), open it, then browse to SDL3.xcframework/macos-arm64_x86_64. In that folder, copy **SDL3.framework** to the game's **extern** folder.
1. Prep the Xcode project:
    ```
    cmake -G Xcode -S . -B build
    ```
1. Now you can open `build/CroMagRally.xcodeproj` in Xcode, or you can just go ahead and build the game:
    ```
    cmake --build build --config RelWithDebInfo
    ```
1. The game gets built in `build/RelWithDebInfo/CroMagRally.app`. Enjoy!

## How to build the game manually on Windows

1. Install the prerequisites:
    - Visual Studio 2022 with the C++ toolchain
    - [CMake](https://cmake.org/download/) 3.21+
1. Clone the repo **recursively**:
    ```
    git clone --recurse-submodules https://github.com/jorio/CroMagRally
    cd CroMagRally
    ```
1. Download [SDL3-devel-3.2.8-VC.zip](https://libsdl.org/release/SDL3-devel-3.2.8-VC.zip), extract it, and copy **SDL3-3.2.8** to the **extern** folder. Rename **SDL3-3.2.8** to just **SDL3**.
1. Prep the Visual Studio solution:
    ```
    cmake -G "Visual Studio 17 2022" -A x64 -S . -B build
    ```
1. Now you can open `build/CroMagRally.sln` in Visual Studio, or you can just go ahead and build the game:
    ```
    cmake --build build --config Release
    ```
1. The game gets built in `build/Release/CroMagRally.exe`. Enjoy!

## How to build the game manually on Linux et al.

1. Install the prerequisites from your package manager:
    - Any C++20 compiler
    - CMake 3.21+
    - SDL3 development library (e.g. "libsdl3-dev" on Ubuntu, "sdl3" on Arch, "SDL3-devel" on Fedora)
    - OpenGL development libraries (e.g. "libgl1-mesa-dev" on Ubuntu)
1. Clone the repo **recursively**:
    ```
    git clone --recurse-submodules https://github.com/jorio/CroMagRally
    cd CroMagRally
    ```
1. Build the game:
    ```
    cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build
    ```
    If you'd like to enable runtime sanitizers, append `-DSANITIZE=1` to the **first** `cmake` call above.
1. The game gets built in `build/CroMagRally`. Enjoy!

## Browser build and static hosting

The web build complements the native desktop builds described above.

Requires CMake 3.24+, Git, Python 3, and an activated [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) (tested with 6.0.13). Run from the repository root:

```sh
git submodule sync
git submodule update --init --recursive
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --parallel
python3 -m http.server 8000 --directory build-web
```

Open <http://localhost:8000/CroMagRally.html>. Opening the HTML directly with `file://` does not work.

The game starts after loading. CMake fetches SDL 3.2.28; the pinned
[Pomme fork](https://github.com/Rage997/Pomme) reuses that SDL target.
To use a local SDL checkout, add `-DFETCHCONTENT_SOURCE_DIR_SDL3=/path/to/SDL`.

The launcher is `packaging/web.html`; artwork is in `packaging/web-assets/`.
Blender is not needed to build.

Publish these files together on an HTTPS static host:

- `CroMagRally.html` (the entry point)
- `CroMagRally.js`, `CroMagRally.wasm`, `CroMagRally.data`
- `CroMagRally.png` and the complete `web-assets/` directory
- `LICENSE.md`, `Pomme-LICENSE.md`, `SDL-LICENSE.txt`

Serve `.wasm` as `application/wasm` and keep the files together. You may rename
`CroMagRally.html` to `index.html`. No backend or cross-origin isolation is needed.
Keep the credits and noncommercial/share-alike licenses.

The asset bundle is about **155 MiB** uncompressed. Check the host's file-size
limit and enable compression.

### Browser runtime

- Requires WebAssembly, WebGL 2, and a keyboard or gamepad. No touch controls are supported.
- **Arrows** and **Enter** navigate; **Escape** goes back or pauses. Controls are configurable.
- Preferences, progress, and scores stay in this browser. Clearing site data deletes them; unavailable storage allows play without saving.
- Use the page's fullscreen/sound control when prompted. Hiding the tab pauses the game; graphics context loss requires a reload.

