#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
QT_VERSION="${QT_VERSION:-6.11.2}"
QT_ROOT="${QT_ROOT:-$HOME/Qt/$QT_VERSION/ios}"
QT_HOST_ROOT="${QT_HOST_ROOT:-$HOME/Qt/$QT_VERSION/macos}"
BUILD_DIR="${MIACODE_IOS_BUILD_DIR:-$ROOT_DIR/build-ios}"
: "${MIACODE_IOS_DEVELOPMENT_TEAM:?Set MIACODE_IOS_DEVELOPMENT_TEAM to your Apple team identifier}"
if [[ ! -f "$QT_ROOT/lib/cmake/Qt6/qt.toolchain.cmake" ]]; then
    echo "Qt iOS $QT_VERSION is required at $QT_ROOT; install the iOS, Multimedia, Quick 3D and Shader Tools components with Qt MaintenanceTool." >&2
    exit 2
fi
bash "$ROOT_DIR/scripts/build/provision-ios-dependencies.sh"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Xcode \
    -DCMAKE_TOOLCHAIN_FILE="$QT_ROOT/lib/cmake/Qt6/qt.toolchain.cmake" \
    -DQT_HOST_PATH="$QT_HOST_ROOT" -DCMAKE_OSX_SYSROOT=iphoneos \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
    -DMIACODE_IOS_DEVELOPMENT_TEAM="$MIACODE_IOS_DEVELOPMENT_TEAM" \
    -DMIACODE_BUILD_DEV_TOOLS=OFF
DESTINATION="generic/platform=iOS"
if [[ -n "${MIACODE_IOS_DEVICE:-}" ]]; then
    xcrun devicectl device info details --device "$MIACODE_IOS_DEVICE" \
        --json-output "$BUILD_DIR/device.json" > "$BUILD_DIR/device-info.log"
    DEVICE_UDID="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["result"]["hardwareProperties"]["udid"])' "$BUILD_DIR/device.json")"
    DESTINATION="platform=iOS,id=$DEVICE_UDID"
fi
xcodebuild -quiet -project "$BUILD_DIR/MiaCode.xcodeproj" -scheme MiaCode \
    -configuration Release -destination "$DESTINATION" -jobs 4 \
    -allowProvisioningUpdates -allowProvisioningDeviceRegistration build
if [[ -n "${MIACODE_IOS_DEVICE:-}" ]]; then
    xcrun devicectl device install app --device "$MIACODE_IOS_DEVICE" "$BUILD_DIR/Release-iphoneos/MiaCode.app"
    xcrun devicectl device process launch --device "$MIACODE_IOS_DEVICE" com.fanfaredash.MiaCode
fi
