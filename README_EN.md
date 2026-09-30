<p align="center">
  <img src="resources/icons/app.png" alt="MiaCode avatar" width="128">
</p>

# MiaCode

[中文](README.md) | [English](README_EN.md)

A new version rebuilt with QML is in development. Follow the [`feature/qml-ui`](https://github.com/Team-MiaCode/MiaCode/tree/feature/qml-ui) branch for progress.

<table align="center" width="100%">
  <tr>
    <td align="center" width="50%"><img src="resources/readme/dark-theme.jpg" alt="MiaCode dark theme" width="400"></td>
    <td align="center" width="50%"><img src="resources/readme/light-theme.jpg" alt="MiaCode light theme" width="400"></td>
  </tr>
  <tr>
    <td align="center">Dark theme</td>
    <td align="center">Light theme</td>
  </tr>
</table>

MiaCode is an all-in-one maimai chart authoring tool built with Qt 6 / C++, offering a range of features across multiple platforms.

## Quick Start

Release packages are available on [GitHub Releases](https://github.com/Team-MiaCode/MiaCode/releases/latest) for Windows (x64) and macOS (Apple Silicon). Download and extract the archive for your system.

- **Windows**: Double-click `MiaCode.exe` in the extracted folder.
- **macOS**: Double-click `MiaCode.app`, or drag it into Applications. If macOS displays a security prompt, open Terminal in the extracted folder and run `xattr -dr com.apple.quarantine "MiaCode.app"`, then launch the app.

Linux users can build from source using the instructions below.

## Features

### Highlights

- Supports Windows, macOS on Apple Silicon, and Linux.

- Adjustable panel widths, with interchangeable editor and preview positions.

- Dark and light themes, with Chinese, English, and Japanese interfaces.

- Preview updates as you type. Scrub through the chart at any time, including while paused.

### Syntax and Muri Checks

<table align="center" width="100%">
  <tr>
    <td align="center" width="50%"><img src="resources/readme/syntax-check.jpg" alt="Syntax check results" width="360"></td>
    <td align="center" width="50%"><img src="resources/readme/muri-check.jpg" alt="Muri detection results" width="360"></td>
  </tr>
  <tr>
    <td align="center">Syntax check</td>
    <td align="center">Muri detection</td>
  </tr>
</table>

Check chart syntax and detect Muri (physically unplayable patterns), with navigation to the relevant lines.

### Video and Cover Export

<table align="center" width="100%">
  <tr>
    <td align="center" width="50%"><img src="resources/readme/video-export.jpg" alt="Video export interface" height="200"></td>
    <td align="center" width="50%"><img src="resources/readme/cover-export.jpg" alt="Cover editor interface" height="200"></td>
  </tr>
  <tr>
    <td align="center">Export</td>
    <td align="center">Cover tools</td>
  </tr>
</table>

#### Export

Export chart preview videos with an intro and full-background PV.

Set a custom export range, or select a section in the editor and use it as the export range.

#### Covers

Create covers for publishing chart videos.

Customize fonts, backgrounds, and other visual elements, and overlay chart frame screenshots to showcase patterns.

### Net Batch Download

<table align="center" width="100%">
  <tr>
    <td align="center"><img src="resources/readme/net-batch-download.jpg" alt="Net batch download interface" width="640"></td>
  </tr>
  <tr>
    <td align="center">Net batch download</td>
  </tr>
</table>

Filter Majdata Net charts by user ID, tag, song title, and upload date. Select charts to download in batches to a chosen folder, with optional ZIP archives.

### More Tools

<table align="center" width="100%">
  <tr>
    <td align="center" width="50%"><img src="resources/readme/time-value-autocomplete.jpg" alt="Note duration completion" height="150"></td>
    <td align="center" width="50%"><img src="resources/readme/chart-formatting.jpg" alt="One-click chart formatting" height="150"></td>
  </tr>
  <tr>
    <td align="center">Note duration completion</td>
    <td align="center">Chart formatting</td>
  </tr>
  <tr>
    <td align="center"><img src="resources/readme/audio-video-tools.jpg" alt="Audio and video tools" height="150"></td>
    <td align="center"><img src="resources/readme/bpm-offset-detection.jpg" alt="BPM and latency detection" height="150"></td>
  </tr>
  <tr>
    <td align="center">Audio and video tools</td>
    <td align="center">BPM and latency detection</td>
  </tr>
</table>

- Custom backgrounds

- Disable IME input or convert full-width characters
- Bookmark navigation
- Quick Touch note authoring
- Drag in audio to create a chart
- Display the beat count of a selection
- Reset tap note positions to lane 1
- Note duration completion
- One-click chart formatting

- Audio and video tools
- BPM and latency detection

## Build

### Requirements

- CMake 3.21+
- C++20 compiler
- Qt 6.8+

See [scripts/README_EN.md](scripts/README_EN.md) for packaging details.

### Windows

Install the following build tools:

- Visual Studio 2022 or Build Tools 2022 with the Desktop development with C++ workload
- CMake 3.21+
- Python 3 with pip

Run the script to install Qt, prepare dependencies, build, and package the application:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build\build-win.ps1
```

### macOS

Use an Apple Silicon Mac running macOS 13 or later. Install Xcode Command Line Tools, then install CMake and Python 3 with [Homebrew](https://brew.sh/):

```bash
xcode-select --install
brew install cmake python
```

After installing the prerequisites, run the script to install Qt, prepare dependencies, build, and package the application:

```bash
bash scripts/build/build-macos.sh
```

### Linux (Advanced Users)

Linux builds require manual setup. Install CMake 3.21+, a C++20 compiler, pkg-config, and Qt 6.8+ (including Qt Quick, Quick Controls 2, Multimedia, the Multimedia private development headers, Shader Tools, and SVG), along with development libraries for FFmpeg, VA-API, DRM, and OpenGL / EGL. The FFmpeg development libraries must include `libavfilter`, `libavcodec`, `libavformat`, `libavutil`, `libswresample`, and `libswscale`.

Run the following commands from the repository root, replacing `[/path/to/Qt/6.x/gcc_64]` with your Qt installation directory:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="[/path/to/Qt/6.x/gcc_64]"
cmake --build build --target MiaCode --parallel
./build/MiaCode
```

Audio/video processing and export use a separate `ffmpeg` executable. Install FFmpeg with H.264 / AAC encoding support.

## Repository Layout

- [src](src): application source
- [assets](assets): runtime assets and generated data
- [resources](resources): Qt resource collections
- [scripts](scripts): build and maintenance scripts
- [third_party](third_party): third-party dependencies
- [docs](docs): documentation
- [samples](samples): sample resources for specification tests

## License

Project-owned code is MIT-licensed. Release binaries distributed with the repository are intended for non-commercial use; see [LICENSE_SCOPE.md](LICENSE_SCOPE.md) for the scope.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the licenses and distribution restrictions of third-party libraries, fonts, sound effects, images, FFmpeg, BASS, Qt, and reference implementations.

## Acknowledgements

Thanks to [Minepig/MaiMuriDX](https://github.com/Minepig/MaiMuriDX) for the Muri detection reference.

Thanks to [gfdfdxc/maimai-transition](https://github.com/gfdfdxc/maimai-transition) for the intro animation reference.

Thanks to [Majdata Net](https://majdata.net/) for the community chart platform.

Thanks to [MaiViewer](https://www.maiviewer.net/) for the official chart transcription download site.

Special thanks to hitomi for drawing the MiaCode logo free of charge.

Thanks to everyone who offered suggestions, reproduced issues, and helped with debugging during internal testing; see [ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md).

## Community

Join the official MiaCode QQ group: 1095435375

<p align="center">
  <img src="resources/community/qq-group.png" alt="MiaCode QQ group QR code" width="360">
</p>
