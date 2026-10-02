cmake_minimum_required(VERSION 3.21)
project(MiaCodeMobile VERSION 2.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()
enable_testing()
set(MIACODE_VERSION_STRING "2.0.0-alpha")
option(MIACODE_ANDROID_HOST_PROBE "Build mobile validation on the host" OFF)
if(NOT ANDROID AND NOT MIACODE_ANDROID_HOST_PROBE)
    message(FATAL_ERROR "This is the Android product. For host validation set MIACODE_ANDROID_HOST_PROBE=ON.")
endif()
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)
include("${PROJECT_SOURCE_DIR}/src/android/CMakeLists.txt")

set(repo "${PROJECT_SOURCE_DIR}")
target_include_directories(MiaCodeAndroid PRIVATE "${repo}/src/app/ui/editor" "${repo}/src/app/ui/timeline" "${repo}/src/android")
include("${PROJECT_SOURCE_DIR}/src/android/V2Ui.cmake")
set_source_files_properties(src/android/AndroidMain.qml PROPERTIES QT_RESOURCE_ALIAS "AndroidMain.qml")
qt_add_qml_module(MiaCodeAndroid URI MiaCode.UI VERSION 1.0
    QML_FILES src/android/AndroidMain.qml ${MOBILE_V2_QML_FILES}
    SOURCES
        "${repo}/src/app/ui/timeline/TimelineThemeBridge.cpp"
        "${repo}/src/app/ui/timeline/TimelineThemeBridge.h"
        "${repo}/src/app/ui/editor/SimaiSyntaxHighlighter.cpp"
        "${repo}/src/app/ui/editor/SimaiSyntaxHighlighter.h"
        "${repo}/src/app/ui/editor/EditorTextStyle.cpp"
        "${repo}/src/app/ui/editor/EditorTextStyle.h"
        "${repo}/src/app/ui/editor/EditorInputBridge.cpp"
        "${repo}/src/app/ui/editor/EditorInputBridge.h"
    RESOURCES ${MOBILE_V2_ICONS}
)
target_sources(MiaCodeAndroid PRIVATE "${repo}/src/app/services/EditorSyncController.cpp" "${repo}/src/app/services/EditorSyncController.h")
target_sources(MiaCodeAndroid PRIVATE "${repo}/resources/app_icons.qrc")
qt_add_resources(MiaCodeAndroid mobileTranslations PREFIX "/i18n" BASE "${repo}/resources/i18n" FILES "${repo}/resources/i18n/zh_CN.qm")
qt_add_resources(MiaCodeAndroid mobileSharedIcons PREFIX "/" FILES ${MOBILE_V2_ICONS})
include("${repo}/src/android/MobileScene.cmake")
if(WIN32 AND MIACODE_ANDROID_HOST_PROBE)
    target_sources(MiaCodeAndroid PRIVATE "${repo}/src/android/HostCrashTrace.cpp")
    target_link_libraries(MiaCodeAndroid PRIVATE dbghelp)
    target_link_options(MiaCodeAndroid PRIVATE "/MAP:${CMAKE_BINARY_DIR}/MiaCodeAndroid.map" "/DEBUG")
    # MSVC 19.36 crashes in the nested slide-area walk with /O2 (Qt 6.11).
    # Keep this host validation TU unoptimized while investigating. Android
    # uses Clang and retains the normal Release optimization settings.
    set_source_files_properties("${repo}/src/core/scene/PreviewTrackLayerState.cpp" PROPERTIES COMPILE_OPTIONS "/Zi;/Od")
endif()
