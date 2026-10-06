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
        src/core/analysis/MuriPanelEntries.h
        src/core/analysis/MuriPanelEntries.cpp
        resources/fonts.qrc
        resources/slide_data.qrc
    LIBS Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

miacode_add_dev_tool(miacode_simai_dump
    SOURCES
        src/devtools/SimaiDump.cpp
        ${_miacode_chart_core}
        ${_miacode_log_core}
        resources/fonts.qrc
        resources/slide_data.qrc
    LIBS Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

miacode_add_dev_tool(miacode_audio_probe
    SOURCES
        src/audio/PreviewAudioBackend.h
        src/audio/PreviewAudioWorkerProtocol.h
        src/audio/PreviewAudioCommandQueue.h
        src/audio/PreviewAudioCommandQueue.cpp
        src/audio/bass/PreviewBassEmergencyPause.h
        src/audio/bass/PreviewBassEmergencyPause.cpp
        src/audio/PreviewAudioWorkerFactory.h
        src/audio/PreviewAudioWorkerFactory.cpp
        src/audio/PreviewAudioWorker.h
        src/audio/PreviewAudioWorker.cpp
        src/audio/bass/PreviewBassDefaultDevice.h
        src/audio/bass/PreviewBassDefaultDevice.cpp
        src/audio/bass/PreviewBassDeviceLease.h
        src/audio/bass/PreviewBassDeviceLease.cpp
        src/devtools/AudioProbe.cpp
        src/audio/bass/BassPreviewAudioBackend.h
        src/audio/bass/BassPreviewAudioBackend.cpp
        src/audio/bass/BassPreviewAudioBackend_EngineInit.cpp
        src/audio/bass/BassPreviewAudioBackend_Assets.cpp
        src/audio/bass/BassPreviewAudioBackend_Transport.cpp
        src/audio/bass/BassPreviewAudioBackend_PlaybackClock.cpp
        src/audio/bass/BassPreviewAudioBackend_EventDrain.cpp
        src/audio/PreviewAudioSettings.h
        src/audio/PreviewAudioSettings.cpp
        src/audio/QtPreviewSfxRuntime.h
        src/audio/QtPreviewSfxRuntime.cpp
        src/common/Mmcss.h
        src/common/Mmcss.cpp
        ${_miacode_log_core}
    LIBS Qt6::Concurrent Qt6::Core Qt6::Gui Qt6::Widgets soundtouch
    INCLUDES src
)
miacode_link_dev_audio(miacode_audio_probe MIXER)

# Batch offset-detection evaluator. Walks a chart corpus and scores the
# offset detector against each chart's &first (error folded mod one
# 8th-note). Manual diagnostic — needs a real corpus, so NOT a CTest case.
miacode_add_dev_tool(miacode_latency_offset_batch
    SOURCES
        src/devtools/LatencyOffsetBatch.cpp
        src/app/runtime/latency/LatencyAnalysis.h
        src/app/runtime/latency/LatencyAnalysis.cpp
        src/audio/bass/OfflineAudioDecoder.h
        src/audio/bass/OfflineAudioDecoder.cpp
        src/audio/bass/PreviewBassDeviceLease.h
        src/audio/bass/PreviewBassDeviceLease.cpp
    LIBS Qt6::Core
    INCLUDES src
)
miacode_link_dev_audio(miacode_latency_offset_batch)
