<p align="center">
  <img src="resources/icons/app.png" alt="MiaCode avatar" width="128">
</p>

<h1 align="center">MiaCode</h1>

<p align="center">
  <a href="https://github.com/Team-MiaCode/MiaCode/releases/latest"><img src="https://img.shields.io/github/v/release/Team-MiaCode/MiaCode" alt="Latest release"></a>
  <a href="https://github.com/Team-MiaCode/MiaCode/stargazers"><img src="https://img.shields.io/github/stars/Team-MiaCode/MiaCode" alt="GitHub stars"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/source_code_license-MIT-blue" alt="Source code license: MIT"></a>
</p>

<p align="center">
  <a href="README.md">中文</a> | <a href="README_EN.md">English</a>
</p>

MiaCode is a maimai chart authoring tool built with Qt 6 / C++ / QML, with editing, live preview, syntax and Muri checks, video export, and cover creation.

<p align="center">
  <img src="resources/screenshots/main-dark.png" alt="MiaCode dark theme with the 狐雨 chart, waveform timeline, and live preview" width="1280">
</p>

<p align="center">
  <img src="resources/screenshots/main-light.png" alt="The same chart in MiaCode's light theme" width="1280">
</p>

## Quick Start

Release packages are available on [GitHub Releases](https://github.com/Team-MiaCode/MiaCode/releases).

Nightly packages are available on [GitHub Actions](https://github.com/Team-MiaCode/MiaCode/actions/workflows/package.yml). Select a `dev` build.

Windows (x64 / ARM64) and macOS (Apple Silicon) are supported. Download and extract the archive for your system and architecture.

- **Windows**: Double-click `MiaCode.exe` in the extracted folder.
- **macOS**: Double-click `MiaCode.app`, or drag it into Applications. If macOS displays a security prompt, open Terminal in the extracted folder and run `xattr -dr com.apple.quarantine "MiaCode.app"`, then launch the app.

Linux users can build from source using the instructions below.

### Open Your First Chart

1. Open a simai `maidata.txt`, or drag an audio file into the workspace to create a chart.
2. Edit the song information and select a difficulty in the sidebar. Type in the editor, then scrub the waveform timeline or preview progress bar to inspect a passage.
3. Use **Save** for the active editing page, or **Save entire document** to save all difficulties and song information. Open video export or the cover editor from the sidebar when publishing.

## Features

### Editing and Preview

- Edit simai charts with syntax highlighting, duration completion, and separate tabs for song information and difficulties.
- Navigate with section bookmarks and a waveform timeline that displays note positions, holds, and slide paths.
- Preview updates as you type. Scrub through the chart while paused, adjust playback speed, or detach the preview into its own window.
- Resize panels and swap the editor and preview positions. Choose dark or light themes, Chinese, English, or Japanese, with system matching available for theme and language.
- Customize skins, judge lines, preview fonts, and the workspace background.

### Syntax and Muri Checks

<table align="center" width="100%">
  <tr>
    <td align="center" width="50%"><img src="resources/screenshots/syntax.png" alt="Syntax diagnostics and line navigation" width="640"></td>
    <td align="center" width="50%"><img src="resources/screenshots/muri.png" alt="Muri diagnostics and source navigation" width="640"></td>
  </tr>
  <tr>
    <td align="center">Syntax check</td>
    <td align="center">Muri detection</td>
  </tr>
</table>

Syntax diagnostics list errors and warnings with line positions. Muri checks identify patterns such as overlapping notes and tap-on-slide conflicts. Select a result to navigate to its source and preview position.

### Video and Cover Export

#### Export

<p align="center">
  <img src="resources/screenshots/video-export.png" alt="Video export settings and range selection" width="1280">
</p>

Export chart preview videos with an intro, background video, and optional chart information and note statistics. Configure the resolution, frame rate, audio bitrate, and encoder in the export workspace.

Set a custom export range, use the current playback position for either boundary, or select a passage in the editor and send it to export. Preview the selected range before exporting.

#### Covers

<p align="center">
  <img src="resources/screenshots/cover-editor.png" alt="Chart frame and difficulty card in the cover editor" width="1280">
</p>

Create covers for publishing chart videos.

Combine difficulty cards, chart frames, images, and text as editable layers. Adjust their position, size, stacking order, and opacity; customize fonts and backgrounds, then save the composition as a reusable preset.

### More Tools

<table align="center" width="100%">
  <tr>
    <td align="center" width="50%"><img src="resources/screenshots/completion.png" alt="Note duration completion candidates" width="640"></td>
    <td align="center" width="50%"><img src="resources/screenshots/normalize.png" alt="Chart formatting options" width="640"></td>
  </tr>
  <tr>
    <td align="center">Note duration completion</td>
    <td align="center">Chart formatting</td>
  </tr>
  <tr>
    <td align="center"><img src="resources/screenshots/media-tools.png" alt="Audio and video processing tools" width="640"></td>
    <td align="center"><img src="resources/screenshots/latency.png" alt="BPM and latency calibration" width="640"></td>
  </tr>
  <tr>
    <td align="center">Audio and video tools</td>
    <td align="center">BPM and latency detection</td>
  </tr>
</table>

- Disable IME input, convert full-width characters, and inspect the beat count of a selection.
- Author Touch notes with the preview touchpad, transform note positions, and format an entire chart or a selection.
- Convert audio to 44.1 kHz, prepend silence or blank video, align background video to audio, and compress videos individually or in batches.
- Detect BPM and audio offset, then use the calibration page to check timing against the track.

## Build

### Requirements

- CMake 3.21+
- C++20 compiler
- Qt 6.10+ with Qt Quick, Quick Controls 2, Quick 3D, Multimedia, Shader Tools, Linguist Tools, and Multimedia private development headers

See [scripts/README_EN.md](scripts/README_EN.md) for packaging details.

### Get the Source

Clone the development branch with its ScintillaQuick editor submodule:

```bash
git clone --branch dev --recurse-submodules https://github.com/Team-MiaCode/MiaCode.git
cd MiaCode
```

For an existing checkout, run `git submodule update --init --recursive` before configuring.

### Windows

Install the following build tools:

- Visual Studio 2022 or Build Tools 2022 with the Desktop development with C++ workload
- CMake 3.21+
- Python 3 with pip

Run the script to install Qt, prepare dependencies, build, and package the application:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build\build-win.ps1 -Toolchain msvc -BuildJobs 4
```

On an ARM64 host, use `-Toolchain msvc-arm64` with a separate build directory; see [scripts/README_EN.md](scripts/README_EN.md).

### macOS

Use an Apple Silicon Mac running macOS 13 or later. Install Xcode Command Line Tools, then install CMake and Python 3 with [Homebrew](https://brew.sh/):

```bash
xcode-select --install
brew install cmake python
```

Install Qt 6.11.1 with Multimedia, Shader Tools, and Quick 3D, prepare the preview SDK and export FFmpeg, then build and package:

```bash
python3 -m pip install aqtinstall
python3 -m aqt install-qt mac desktop 6.11.1 clang_64 --outputdir .qt \
  --modules qtmultimedia qtshadertools qtquick3d
bash scripts/ffmpeg/ensure-macos-ffmpeg-dev.sh
bash scripts/ffmpeg/ensure-macos-ffmpeg.sh
bash scripts/build/build-macos-local.sh
```

Set `QT_ROOT` to reuse an existing Qt installation. The local script reuses dependencies and writes packages to `dist/`.

### Linux (Advanced Users)

Linux builds require manual setup. Install CMake 3.21+, a C++20 compiler, pkg-config, and Qt 6.10+ (including Qt Quick, Quick Controls 2, Quick 3D, Multimedia, the Multimedia private development headers, Shader Tools, and Linguist Tools), along with development libraries for FFmpeg, VA-API, DRM, and OpenGL / EGL. The FFmpeg development libraries must include `libavfilter`, `libavcodec`, `libavformat`, `libavutil`, `libswresample`, and `libswscale`.

Run the following commands from the repository root, replacing `[/path/to/Qt/6.x/gcc_64]` with your Qt installation directory:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="[/path/to/Qt/6.x/gcc_64]"
cmake --build build --target MiaCode --parallel 4
./build/bin/MiaCode
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
