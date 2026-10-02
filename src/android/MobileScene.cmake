# The production v2 scene graph is shared by interactive preview and export.
find_package(Qt6 REQUIRED COMPONENTS Multimedia Svg ShaderTools OpenGL QuickDialogs2)
file(GLOB mobileSceneSources CONFIGURE_DEPENDS
    "${repo}/src/core/scene/*.cpp"
    "${repo}/src/preview/quick_scene/*.cpp")
file(GLOB mobileMuriSources CONFIGURE_DEPENDS "${repo}/src/tools/muri/*.cpp")
file(GLOB mobileTimelineSources CONFIGURE_DEPENDS
    "${repo}/src/timeline/quick/*.cpp"
    "${repo}/src/timeline/TimelineQuickModel*.cpp")
list(FILTER mobileMuriSources EXCLUDE REGEX "(MuriSpec|MuriDump).cpp$")
add_library(MiaCodeMobileScene STATIC ${mobileSceneSources}
    ${mobileMuriSources}
    ${mobileTimelineSources}
    "${repo}/src/timeline/TimelineSceneState.cpp"
    "${repo}/src/timeline/TimelineSceneStateBuilder.cpp"
    "${repo}/src/timeline/TimelineSlowRefresh.cpp"
    "${repo}/src/common/InputShortcutGesture.cpp"
    "${repo}/src/common/ProcessDiagnostics.cpp"
    "${repo}/src/common/WaveformCache.cpp"
    "${repo}/src/android/MobileOfflineAudioDecoder.cpp"
    "${repo}/src/app/services/AnalysisService.cpp"
    "${repo}/src/app/ui/document/AnalysisModel.cpp"
    "${repo}/src/app/ui/document/AnalysisProjection.cpp"
    "${repo}/src/app/ui/chrome/ShortcutRegistry.cpp"
    "${repo}/src/app/ui/chrome/ShortcutModel.cpp"
    "${repo}/src/common/MuriTypes.cpp"
    "${repo}/src/app/ui/preferences/PreferenceDocument.cpp"
    "${repo}/src/timeline/TimelineNoteAssets.cpp"
    "${repo}/src/preview/runtime/PreviewRuntime.cpp"
    "${repo}/src/preview/runtime/PreviewQuickExportSession.cpp"
    "${repo}/src/preview/runtime/PreviewQuickExportSession.h"
    "${repo}/src/audio/PreviewAudioSettings.cpp"
    "${repo}/src/app/services/ShellNotifications.cpp"
    "${repo}/src/app/services/UiRequestService.cpp"
    "${repo}/src/app/services/JobProgressService.cpp"
    "${repo}/src/app/services/PreviewAppearanceState.cpp"
    "${repo}/src/app/ui/export/ExportSession.cpp"
    "${repo}/src/app/ui/preview/PreviewSettingsModel.cpp"
    "${repo}/src/app/ui/preferences/LocaleService.cpp"
    "${repo}/src/tools/video_export/VideoExportSettings.cpp"
    "${repo}/src/tools/video_export/VideoExportRuntimePolicy.cpp"
    "${repo}/src/tools/video_export/VideoExportAudioRenderPlan.cpp"
    "${repo}/src/tools/video_export/VideoExportPauseOverlay.cpp"
    "${repo}/src/tools/video_export/FontLibrary.cpp"
    "${repo}/src/preview/runtime/PreviewRuntime.h"
    "${repo}/src/preview/runtime/PreviewSceneAssetLoader.cpp"
    "${repo}/src/preview/runtime/PreviewSceneAssetRepository.cpp"
    "${repo}/src/common/Mmcss.cpp")
target_include_directories(MiaCodeMobileScene PUBLIC "${repo}/src" "${repo}/src/core/chart/parser" "${repo}/src/app/ui" "${repo}/src/app")
target_compile_definitions(MiaCodeMobileScene PUBLIC HAVE_QT_MULTIMEDIA)
target_link_libraries(MiaCodeMobileScene PUBLIC MiaCodeAndroidFoundation Qt6::Quick Qt6::Multimedia Qt6::Svg Qt6::OpenGL)
target_link_libraries(MiaCodeAndroid PRIVATE MiaCodeMobileScene Qt6::QuickDialogs2)
if(WIN32)
    target_link_libraries(MiaCodeMobileScene PRIVATE avrt)
endif()
target_sources(MiaCodeAndroid PRIVATE "${repo}/src/android/MobilePreview.cpp" "${repo}/src/android/MobilePreview.h"
    "${repo}/src/android/AndroidFileRequests.cpp" "${repo}/src/android/AndroidFileRequests.h"
    "${repo}/src/android/MobileVideoExport.cpp" "${repo}/src/android/MobileVideoExport.h"
    "${repo}/src/android/MobileExportComposition.cpp" "${repo}/src/android/MobileExportComposition.h"
    "${repo}/src/android/MobileExportAudio.cpp"
    "${repo}/src/android/MobileTimeline.cpp" "${repo}/src/android/MobileTimeline.h")
if(ANDROID)
    set(mobilePackage "${CMAKE_BINARY_DIR}/android-package")
    file(COPY "${repo}/packaging/android/" DESTINATION "${mobilePackage}")
    foreach(assetFolder skin background noteguide SFX fonts)
        file(COPY "${repo}/assets/${assetFolder}" DESTINATION "${mobilePackage}/assets/miacode")
    endforeach()
    set_target_properties(MiaCodeAndroid PROPERTIES QT_ANDROID_PACKAGE_SOURCE_DIR "${mobilePackage}")
endif()
target_sources(MiaCodeAndroid PRIVATE
    "${repo}/resources/slide_data.qrc"
    "${repo}/resources/preview_judge_effects.qrc"
    "${repo}/resources/preview_runtime_qml.qrc")
target_sources(MiaCodeAndroid PRIVATE "${repo}/resources/intro.qrc")
if(MIACODE_ANDROID_HOST_PROBE AND NOT ANDROID)
    qt_add_executable(ExportDestinationSpec "${repo}/src/android/tests/ExportDestinationSpec.cpp")
    target_include_directories(ExportDestinationSpec PRIVATE "${repo}/src")
    target_link_libraries(ExportDestinationSpec PRIVATE Qt6::Core)
    add_test(NAME ExportDestinationSpec COMMAND ExportDestinationSpec)
    qt_add_executable(MobileExportAudioSpec
        "${repo}/src/android/tests/MobileExportAudioSpec.cpp"
        "${repo}/src/android/MobileExportAudio.cpp")
    target_include_directories(MobileExportAudioSpec PRIVATE "${repo}/src")
    target_link_libraries(MobileExportAudioSpec PRIVATE Qt6::Gui Qt6::Multimedia)
    add_test(NAME MobileExportAudioSpec COMMAND MobileExportAudioSpec)
endif()
qt_add_shaders(MiaCodeAndroid mobilePreviewShaders PREFIX "/" GLSL "300 es,330" FILES
    "src/preview/quick_scene/shaders/PreviewSpriteMaterial.vert"
    "src/preview/quick_scene/shaders/PreviewSpriteMaterial.frag"
    "src/preview/quick_scene/shaders/PreviewStageDimMaterial.vert"
    "src/preview/quick_scene/shaders/PreviewStageDimMaterial.frag"
    "src/preview/quick_scene/shaders/PreviewFireworkMaterial.vert"
    "src/preview/quick_scene/shaders/PreviewFireworkMaterial.frag"
    "src/intro/shaders/bg_texture.frag")
