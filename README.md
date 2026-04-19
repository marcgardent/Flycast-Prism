<img src="docs/prism/logo-prism.svg" alt="flycast prism logo" />

**Flycast-Prism** is an experimental fork of the renowned Flycast emulator, dedicated to pushing the visual boundaries of Sega Dreamcast, Naomi, and Atomiswave emulation through modern rendering techniques.

First and foremost, we want to express our deepest gratitude to the original Flycast team, particularly flyinghead and all the contributors. Flycast-Prism is built upon their colossal and brilliant foundational work. We are merely standing on the shoulders of giants, and this project would simply not exist without their dedication to preserving gaming history.

## Current Features (Custom Render Engine)

Our primary focus is overhauling the rendering pipeline to support modern post-processing effects. Currently, the engine features:
 
 * **GBuffer Implementation**: We have successfully integrated a GBuffer that extracts essential scene data in real-time, including:
   * Albedo: The base color map.
   * HUD: Separating the user interface from the 3D scene to prevent UI distortion during post-processing.
   * Normal: Surface direction data for advanced lighting calculations.
   * Depth: Accurate Z-buffer information.
   
 * **Post-Processing Filters**: Leveraging the GBuffer, we have enabled the following effects:
   * SSAO (Screen Space Ambient Occlusion): Adding realistic shading and depth to geometric intersections.
   * Optical Blur: Simulating realistic camera lens blurring effects.

## Short-Term Objectives (HUD Customization)

Our immediate goal is to overhaul the HUD composition specifically for widescreen rendering. This will introduce a new level of flexibility, giving users the ability to fully customize the HUD, move specific UI elements across the screen, or hide them entirely according to their preferences.

## Long-Term Objective

Our ultimate goal for the rendering engine is to successfully implement a Velocity Map (motion vectors). Acquiring accurate velocity data is the key to unlocking the most advanced, industry-standard post-processing filters, such as Temporal Anti-Aliasing (TAA) and high-quality Motion Blur.

details of implementation : [GBuffer Architecture.md](docs/prism/GBuffer%20Architecture.md)
________________________
# Flycast

[![Android CI](https://github.com/flyinghead/flycast/actions/workflows/android.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/android.yml)
[![C/C++ CI](https://github.com/flyinghead/flycast/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/c-cpp.yml)
[![Nintendo Switch CI](https://github.com/flyinghead/flycast/actions/workflows/switch.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/switch.yml)
[![Windows UWP CI](https://github.com/flyinghead/flycast/actions/workflows/uwp.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/uwp.yml)
[![BSD CI](https://github.com/flyinghead/flycast/actions/workflows/bsd.yml/badge.svg)](https://github.com/flyinghead/flycast/actions/workflows/bsd.yml)

<img src="shell/linux/flycast.png" alt="flycast logo" width="150"/>

**Flycast** is a multi-platform Sega Dreamcast, Naomi, Naomi 2, and Atomiswave emulator derived from [**reicast**](https://github.com/skmp/reicast-emulator).

Information about configuration and supported features can be found on [**TheArcadeStriker's flycast wiki**](https://github.com/TheArcadeStriker/flycast-wiki/wiki).

Join us on our [**Discord server**](https://discord.gg/X8YWP8w) for a chat. 

## Install

### Android ![android](https://flyinghead.github.io/flycast-builds/android.jpg)
Install Flycast from [**Google Play**](https://play.google.com/store/apps/details?id=com.flycast.emulator).
### Flatpak (Linux ![ubuntu logo](https://flyinghead.github.io/flycast-builds/ubuntu.png))

1. [Set up Flatpak](https://www.flatpak.org/setup/).

2. Install Flycast from [Flathub](https://flathub.org/apps/details/org.flycast.Flycast):

`flatpak install -y org.flycast.Flycast`

3. Run Flycast:

`flatpak run org.flycast.Flycast`

### Homebrew (MacOS ![apple logo](https://flyinghead.github.io/flycast-builds/apple.png))

1. [Set up Homebrew](https://brew.sh).

2. Install Flycast via Homebrew:

`brew install --cask flycast`

### iOS

Due to persistent harassment from an iOS user, support for this platform has been dropped. 

### Xbox One/Series ![xbox logo](https://flyinghead.github.io/flycast-builds/xbox.png)

Grab the latest build from [**the builds page**](https://flyinghead.github.io/flycast-builds/), or the [**GitHub Actions**](https://github.com/flyinghead/flycast/actions/workflows/uwp.yml). Then install it using the **Xbox Device Portal**.

### Binaries ![android](https://flyinghead.github.io/flycast-builds/android.jpg) ![windows](https://flyinghead.github.io/flycast-builds/windows.png) ![linux](https://flyinghead.github.io/flycast-builds/ubuntu.png) ![apple](https://flyinghead.github.io/flycast-builds/apple.png) ![switch](https://flyinghead.github.io/flycast-builds/switch.png) ![xbox](https://flyinghead.github.io/flycast-builds/xbox.png)

Get fresh builds for your system [**on the builds page**](https://flyinghead.github.io/flycast-builds/).

**New:** Now automated test results are available as well. 

### Build requirements (Linux):

- **C/C++ compiler toolchain** (e.g. `gcc`/`g++`)
- **CMake**
- **make**
- **libcurl** (development headers)
- **libudev** (development headers)
- **SDL2** (development headers)
- **Graphics API**: Vulcan, OpenGL

### Build instructions:
```
$ git clone --recursive https://github.com/flyinghead/flycast.git
$ cd flycast
$ mkdir build && cd build
$ cmake ..
$ make
```
