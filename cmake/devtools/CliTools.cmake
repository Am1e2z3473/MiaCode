# Link diagnostic tools to BASS and stage its runtime beside each executable.
function(miacode_link_dev_audio NAME)
    cmake_parse_arguments(AUDIO "MIXER" "" "" ${ARGN})
    target_include_directories(${NAME} PRIVATE third_party/bass/include)
    if(WIN32)
        set(bass_dir "${MIACODE_BASS_WINDOWS_LIB_DIR}")
        set(bass_library bass.lib)
        set(mixer_library bassmix.lib)
        set(runtime_files ${MIACODE_BASS_WINDOWS_DLLS})
        set(runtime_dir "${MIACODE_BASS_WINDOWS_BIN_DIR}")
    elseif(APPLE)
        set(bass_dir "${MIACODE_BASS_MACOS_DIR}")
        set(bass_library libbass.dylib)
        set(mixer_library libbassmix.dylib)
        set(runtime_files ${MIACODE_BASS_MACOS_LIBRARIES})
        set(runtime_dir "${bass_dir}")
        set_target_properties(${NAME} PROPERTIES BUILD_RPATH "@executable_path")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(bass_dir "${MIACODE_BASS_LINUX_DIR}")
        set(bass_library libbass.so)
        set(mixer_library libbassmix.so)
        set(runtime_files ${MIACODE_BASS_LINUX_LIBRARIES})
        set(runtime_dir "${bass_dir}")
        set_target_properties(${NAME} PROPERTIES BUILD_RPATH "$ORIGIN")
    endif()
    target_link_libraries(${NAME} PRIVATE "${bass_dir}/${bass_library}")
    if(AUDIO_MIXER)
        target_link_libraries(${NAME} PRIVATE "${bass_dir}/${mixer_library}")
        if(WIN32)
            target_link_libraries(${NAME} PRIVATE avrt)
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_link_libraries(${NAME} PRIVATE ${CMAKE_DL_LIBS})
        endif()
    endif()
    add_custom_command(TARGET ${NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${runtime_files} $<TARGET_FILE_DIR:${NAME}>
        WORKING_DIRECTORY "${runtime_dir}"
        VERBATIM)
endfunction()

# ---- CLI dump / probe helpers (manual diagnostics, not CTest cases) ----
miacode_add_dev_tool(miacode_muri_dump
    SOURCES
        src/devtools/MuriDump.cpp
        src/common/DebugOptions.h
        ${_miacode_log_core}
        ${_miacode_chart_core}
        ${_miacode_muri_analysis_core}
        src/tools/muri/MuriPanelEntries.h
        src/tools/muri/MuriPanelEntries.cpp
        resources/fonts.qrc
        resources/slide_data.qrc
    LIBS Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES
        src src/app/ui src/common src/core/chart
        src/core/chart/document src/core/chart/parser src/timeline src/tools
        src/tools/muri
)

miacode_add_dev_tool(miacode_simai_dump
    SOURCES
        src/devtools/SimaiDump.cpp
        ${_miacode_chart_core}
        ${_miacode_log_core}
        resources/fonts.qrc
        resources/slide_data.qrc
    LIBS Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src src/core/chart src/core/chart/parser src/timeline
)

miacode_add_dev_tool(miacode_audio_probe
    SOURCES
        src/audio/PreviewAudioBackend.h
        src/audio/PreviewAudioWorkerProtocol.h
        src/audio/PreviewAudioCommandQueue.h
        src/audio/PreviewAudioCommandQueue.cpp
        src/audio/PreviewBassEmergencyPause.h
        src/audio/PreviewBassEmergencyPause.cpp
        src/audio/PreviewAudioWorkerFactory.h
        src/audio/PreviewAudioWorkerFactory.cpp
        src/audio/PreviewAudioWorker.h
        src/audio/PreviewAudioWorker.cpp
        src/audio/PreviewBassDefaultDevice.h
        src/audio/PreviewBassDefaultDevice.cpp
        src/audio/PreviewBassDeviceLease.h
        src/audio/PreviewBassDeviceLease.cpp
        src/devtools/AudioProbe.cpp
        src/audio/BassPreviewAudioBackend.h
        src/audio/BassPreviewAudioBackend.cpp
        src/audio/BassPreviewAudioBackend_EngineInit.cpp
        src/audio/BassPreviewAudioBackend_Assets.cpp
        src/audio/BassPreviewAudioBackend_Transport.cpp
        src/audio/BassPreviewAudioBackend_PlaybackClock.cpp
        src/audio/BassPreviewAudioBackend_EventDrain.cpp
        src/audio/PreviewAudioSettings.h
        src/audio/PreviewAudioSettings.cpp
        src/audio/QtPreviewSfxRuntime.h
        src/audio/QtPreviewSfxRuntime.cpp
        src/common/Mmcss.h
        src/common/Mmcss.cpp
        ${_miacode_log_core}
    LIBS Qt6::Concurrent Qt6::Core Qt6::Gui Qt6::Widgets soundtouch
    INCLUDES
        src src/preview src/audio
        src/core/video src/timeline src/tools
)
miacode_link_dev_audio(miacode_audio_probe MIXER)

# Batch offset-detection evaluator. Walks a chart corpus and scores the
# offset detector against each chart's &first (error folded mod one
# 8th-note). Manual diagnostic — needs a real corpus, so NOT a CTest case.
miacode_add_dev_tool(miacode_latency_offset_batch
    SOURCES
        src/devtools/LatencyOffsetBatch.cpp
        src/tools/latency/LatencyAnalysis.h
        src/tools/latency/LatencyAnalysis.cpp
        src/audio/OfflineAudioDecoder.h
        src/audio/OfflineAudioDecoder.cpp
        src/audio/PreviewBassDeviceLease.h
        src/audio/PreviewBassDeviceLease.cpp
    LIBS Qt6::Core
    INCLUDES src src/tools/latency
)
miacode_link_dev_audio(miacode_latency_offset_batch)
