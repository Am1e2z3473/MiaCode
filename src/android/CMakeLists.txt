
set(repo "${PROJECT_SOURCE_DIR}")
add_library(MiaCodeAndroidFoundation STATIC
    "${repo}/src/android/AndroidDocumentSession.cpp" "${repo}/src/android/AndroidDocumentSession.h" "${repo}/src/android/MobileV2Document.cpp"
    "${repo}/src/android/MobileMetadata.cpp" "${repo}/src/common/Id3TagReader.cpp"
    "${repo}/src/app/ui/editor/EditorController.cpp"
    "${repo}/src/app/ui/editor/EditorController.h"
    "${repo}/src/editor/SimaiTextEditPolicy.cpp"
    "${repo}/src/editor/SimaiCompletionCatalog.cpp"
    "${repo}/src/editor/BookmarkCommentSyntax.cpp"
    "${repo}/src/editor/TouchPadAuthoringEdit.cpp"
    "${repo}/src/core/chart/selection/ChartSelectionBeatSummary.cpp"
    "${repo}/src/core/chart/parser/SimaiCommentScan.cpp"
    "${repo}/src/app/services/ChartWorkspace.cpp"
    "${repo}/src/app/services/ChartWorkspace.h"
    "${repo}/src/core/chart/document/SimaiDocument.cpp"
    "${repo}/src/core/chart/document/SimaiTimingMetadata.cpp"
    "${repo}/src/core/chart/parser/SimaiNativeParser.cpp"
    "${repo}/src/common/OperationLog.cpp"
    "${repo}/src/common/DebugLog.cpp"
)
file(GLOB chartTransforms "${repo}/src/core/chart/transform/*.cpp")
target_sources(MiaCodeAndroidFoundation PRIVATE ${chartTransforms})
target_include_directories(MiaCodeAndroidFoundation PUBLIC "${repo}/src" "${repo}/src/app/ui")
target_link_libraries(MiaCodeAndroidFoundation PUBLIC Qt6::Core Qt6::Gui)

if (ANDROID)
    if (NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
        message(FATAL_ERROR "MiaCode Android currently supports arm64-v8a only")
    endif()
    qt_add_executable(MiaCodeAndroid "${repo}/src/android/main.cpp" "${repo}/src/android/AndroidPlatformBridge.cpp" "${repo}/src/android/AndroidPlatformBridge.h")
    set_target_properties(MiaCodeAndroid PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${repo}/packaging/android"
        QT_ANDROID_MIN_SDK_VERSION 31
        QT_ANDROID_TARGET_SDK_VERSION 35
        QT_ANDROID_COMPILE_SDK_VERSION 36
        QT_ANDROID_SDK_BUILD_TOOLS_REVISION "36.0.0"
        QT_ANDROID_VERSION_CODE 1
        QT_ANDROID_VERSION_NAME "${MIACODE_VERSION_STRING}-android-dev"
    )
    target_link_options(MiaCodeAndroid PRIVATE "-Wl,-z,max-page-size=16384")
else()
    qt_add_executable(MiaCodeAndroid "${repo}/src/android/main.cpp")
    qt_add_executable(AndroidFoundationSpec "${repo}/src/android/tests/AndroidFoundationSpec.cpp")
    target_link_libraries(AndroidFoundationSpec PRIVATE MiaCodeAndroidFoundation)
    target_compile_definitions(AndroidFoundationSpec PRIVATE
        MIACODE_ANDROID_FIXTURE_DIR="${repo}/src/android/tests/fixtures")
    add_test(NAME AndroidFoundationSpec COMMAND AndroidFoundationSpec)
endif()
target_link_libraries(MiaCodeAndroid PRIVATE MiaCodeAndroidFoundation Qt6::Quick Qt6::QuickControls2)
target_sources(MiaCodeAndroid PRIVATE "${repo}/src/android/AndroidEditorTools.h")
