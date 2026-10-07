# Explicit spec targets; contract IDs stay stable across source/target renames.

miacode_add_spec(timeline_model_spec
    OWNER src/timeline
    CONTRACT timeline.timeline-model
    DOMAIN timeline KIND integration RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/timeline/TimelineModelSpec.cpp
    LIBS miacode_timeline Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)
# Source-contract assertions in TimelineModelSpec must work from both the
# standard `build/` CTest directory and the developer build tree.
target_compile_definitions(timeline_model_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_spec(timeline_marker_offset_spec
    OWNER src/core/chart/model
    CONTRACT timeline.timeline-marker-offset
    DOMAIN timeline KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/timeline/TimelineMarkerOffsetSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(timeline_quick_texture_cache_policy_spec
    OWNER src/timeline
    CONTRACT timeline.timeline-quick-texture-cache-policy
    DOMAIN timeline KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/timeline/TimelineQuickTextureCachePolicySpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(timeline_cadence_arbitration_policy_spec
    OWNER src/timeline
    CONTRACT timeline.timeline-cadence-arbitration-policy
    DOMAIN timeline KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/timeline/TimelineCadenceArbitrationPolicySpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)
