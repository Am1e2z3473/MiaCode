#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CACHE_DIR="$ROOT_DIR/build-ios/dependencies"
BASS_DIR="$ROOT_DIR/third_party/bass/lib/ios"
SDK_DIR="$ROOT_DIR/third_party/ffmpeg/ios/dev"
mkdir -p "$CACHE_DIR" "$BASS_DIR"
for package in bass24 bassmix24 bass_fx24 bassopus24 bassflac24; do
    archive="$CACHE_DIR/$package-ios.zip"
    if [[ ! -f "$archive" ]]; then
        path="$package-ios.zip"
        [[ "$package" != bass_fx24 ]] || path="z/0/$path"
        curl -fL "https://www.un4seen.com/files/$path" -o "$archive"
    fi
    unzip -oq "$archive" '*.xcframework/*' -d "$BASS_DIR"
done
if [[ -f "$SDK_DIR/lib/libavfilter.a" ]]; then
    exit 0
fi
VERSION=8.1.2
ARCHIVE="$CACHE_DIR/ffmpeg-$VERSION.tar.xz"
if [[ ! -f "$ARCHIVE" ]]; then
    curl -fL "https://ffmpeg.org/releases/ffmpeg-$VERSION.tar.xz" -o "$ARCHIVE"
fi
printf '%s  %s\n' 464beb5e7bf0c311e68b45ae2f04e9cc2af88851abb4082231742a74d97b524c "$ARCHIVE" | shasum -a 256 -c -
if [[ ! -d "$CACHE_DIR/ffmpeg-$VERSION" ]]; then
    tar -xf "$ARCHIVE" -C "$CACHE_DIR"
fi
SDK_PATH="$(xcrun --sdk iphoneos --show-sdk-path)"
FLAGS="-arch arm64 -isysroot $SDK_PATH -miphoneos-version-min=17.0"
cd "$CACHE_DIR/ffmpeg-$VERSION"
./configure --prefix="$SDK_DIR" --target-os=darwin --arch=arm64 --enable-cross-compile \
    --cc="$(xcrun --sdk iphoneos --find clang)" \
    --cxx="$(xcrun --sdk iphoneos --find clang++)" \
    --sysroot="$SDK_PATH" --enable-static --disable-shared --enable-pic \
    --disable-programs --disable-doc --disable-debug --disable-autodetect \
    --disable-gpl --disable-nonfree --disable-avdevice --enable-videotoolbox \
    --extra-cflags="$FLAGS" --extra-ldflags="$FLAGS"
make -j4
make install
