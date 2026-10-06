# Explicit spec targets; contract IDs stay stable across source/target renames.

miacode_add_spec(preview_asset_loader_spec
    OWNER src/preview/runtime
    CONTRACT preview.preview-asset-loader
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewSceneAssetLoaderSpec.cpp
    LIBS miacode_preview_quick Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_spec(preview_firework_lifecycle_spec
    OWNER src/core/scene
    CONTRACT preview.preview-firework-lifecycle
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewFireworkLifecycleSpec.cpp
    LIBS miacode_scene Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_spec(preview_end_of_media_policy_spec
    OWNER src/core/video
    CONTRACT preview.preview-end-of-media-policy
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewEndOfMediaPolicySpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(preview_firework_warmup_policy_spec
    OWNER src/core/scene
    CONTRACT preview.preview-firework-warmup-policy
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewFireworkWarmupPolicySpec.cpp
    LIBS miacode_scene Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_spec(preview_head_layer_spec
    OWNER src/core/scene
    CONTRACT preview.preview-head-layer
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewHeadLayerSpec.cpp
    LIBS miacode_scene Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

miacode_add_spec(preview_guide_layer_spec
    OWNER src/core/scene
    CONTRACT preview.preview-guide-layer
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewGuideLayerSpec.cpp
    LIBS miacode_scene Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

miacode_add_spec(preview_slide_erase_by_area_spec
    OWNER src/core/scene
    CONTRACT preview.preview-slide-erase-by-area
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewSlideEraseByAreaSpec.cpp
    LIBS miacode_scene Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_spec(preview_realtime_object_hot_path_spec
    OWNER src/core/scene
    CONTRACT preview.preview-realtime-object-hot-path
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewRealtimeObjectHotPathSpec.cpp
    LIBS miacode_scene Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_spec(preview_quick_sprite_batch_spec
    OWNER src/preview/quick_scene
    CONTRACT preview.preview-quick-sprite-batch
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewQuickSpriteBatchSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(preview_texture_generation_policy_spec
    OWNER src/preview/quick_scene
    CONTRACT preview.preview-texture-generation-policy
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewTextureGenerationPolicySpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(preview_sfx_timeline_spec
    OWNER src/core/scene
    CONTRACT preview.preview-sfx-timeline
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewSfxTimelineSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(preview_audio_settings_spec
    OWNER src/audio
    CONTRACT preview.preview-audio-settings
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewAudioSettingsSpec.cpp
    LIBS miacode_audio Qt6::Core
    INCLUDES src
)

miacode_add_spec(preview_audio_command_queue_spec
    OWNER src/audio
    CONTRACT preview.preview-audio-command-queue
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewAudioCommandQueueSpec.cpp
    LIBS miacode_audio Qt6::Core
    INCLUDES src
)

miacode_add_spec(preview_audio_worker_protocol_spec
    OWNER src/audio
    CONTRACT preview.preview-audio-worker-protocol
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewAudioWorkerProtocolSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(preview_audio_playback_flow_policy_spec
    OWNER src/audio
    CONTRACT preview.preview-audio-playback-flow-policy
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewAudioPlaybackFlowPolicySpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)
miacode_add_spec(preview_audio_worker_spec
    OWNER src/audio
    CONTRACT preview.preview-audio-worker
    DOMAIN preview KIND integration RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewAudioWorkerSpec.cpp
    LIBS miacode_audio_bass Qt6::Core soundtouch
    INCLUDES src
)
target_compile_definitions(preview_audio_worker_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")
# BASS import libraries come from miacode_audio_bass; stage the runtime the
# production backend loads beside the spec.
if (WIN32)
    add_custom_command(TARGET preview_audio_worker_spec POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MIACODE_BASS_WINDOWS_BIN_DIR}/bass.dll"
            "${MIACODE_BASS_WINDOWS_BIN_DIR}/bassmix.dll"
            "${MIACODE_BASS_WINDOWS_BIN_DIR}/bassflac.dll"
            $<TARGET_FILE_DIR:preview_audio_worker_spec>
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_custom_command(TARGET preview_audio_worker_spec POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MIACODE_BASS_LINUX_LIBRARIES}
            $<TARGET_FILE_DIR:preview_audio_worker_spec>
    )
endif()

miacode_add_spec(preview_audio_non_gui_barrier_spec
    OWNER src/audio
    CONTRACT preview.preview-audio-non-gui-barrier
    DOMAIN preview KIND integration RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewAudioNonGuiBarrierSpec.cpp
    LIBS miacode_audio Qt6::Core
    INCLUDES src
)
if (WIN32)
    target_link_libraries(preview_audio_non_gui_barrier_spec PRIVATE avrt)
endif()

miacode_add_spec(touch_pad_authoring_state_spec
    OWNER src/core/scene
    CONTRACT preview.touch-pad-authoring-state
    DOMAIN preview KIND source-contract RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/TouchPadAuthoringStateSpec.cpp
    LIBS Qt6::Core Qt6::Gui
    INCLUDES src
)
target_compile_definitions(touch_pad_authoring_state_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_spec(bass_preview_retained_state_spec
    OWNER src/audio/bass
    CONTRACT preview.bass-preview-retained-state
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/BassPreviewRetainedStateSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(bass_preview_debug_log_routing_spec
    OWNER src/audio/bass
    CONTRACT preview.bass-preview-debug-log-routing
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/BassPreviewDebugLogRoutingSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(preview_bass_device_lease_spec
    OWNER src/audio/bass
    CONTRACT preview.preview-bass-device-lease
    DOMAIN preview KIND integration RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewBassDeviceLeaseSpec.cpp
    LIBS miacode_audio_bass Qt6::Core
    INCLUDES src
)

miacode_add_spec(preview_audio_health_spec
    OWNER src/audio
    CONTRACT preview.preview-audio-health
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewAudioHealthSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(preview_audio_output_glitch_probe_spec
    OWNER src/audio
    CONTRACT preview.preview-audio-output-glitch-probe
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewAudioOutputGlitchProbeSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(preview_audio_device_change_policy_spec
    OWNER src/audio
    CONTRACT preview.preview-audio-device-change-policy
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewAudioDeviceChangePolicySpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(bass_preview_sfx_scheduler_policy_spec
    OWNER src/audio/bass
    CONTRACT preview.bass-preview-sfx-scheduler-policy
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/BassPreviewSfxSchedulerPolicySpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)
target_compile_definitions(bass_preview_sfx_scheduler_policy_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

# Simulation spec for the preview audio/stage-media cache mechanism: the
# content-stamp skip decision (size:mtime, NOT path-only) and the live,
# uncached track/bg resolvers. Drives the real production functions against
# real temp files, simulating an in-place track.mp3/bg.png rewrite.
miacode_add_spec(preview_media_cache_stamp_spec
    OWNER src/common
    CONTRACT preview.preview-media-cache-stamp
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PreviewMediaCacheStampSpec.cpp
    LIBS Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_spec(pv_memory_diagnostics_spec
    OWNER src/preview/stage_media
    CONTRACT preview.pv-memory-diagnostics
    DOMAIN preview KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PvMemoryDiagnosticsSpec.cpp
    LIBS miacode_stage_media Qt6::Core
    INCLUDES src
)
target_compile_definitions(pv_memory_diagnostics_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_spec(pv_memory_host_contract_spec
    OWNER src/preview/stage_media
    CONTRACT preview.pv-memory-host-contract
    DOMAIN preview KIND source-contract RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/PvMemoryHostContractSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)
target_compile_definitions(pv_memory_host_contract_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_spec(quickshell_preview_surface_policy_spec
    OWNER src/app/quick_shell
    CONTRACT preview.quickshell-preview-surface-policy
    DOMAIN preview KIND source-contract RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/preview/QuickShellPreviewSurfacePolicySpec.cpp
        src/app/quick_shell/QuickShellPreviewSurfacePolicy.h
    LIBS Qt6::Core
    INCLUDES src
)
target_compile_definitions(quickshell_preview_surface_policy_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")
