# Runs in ScintillaQuick's project scope. Metatype custom commands must belong
# to the dependency's directory so CMake attaches them to its build graph.
cmake_language(DEFER CALL qt_extract_metatypes ScintillaQuick)

function(miacode_scintillaquick_local_coordinates)
    # Scintilla paints in item-local coordinates. Window::GetPosition remains
    # parent-relative for popup placement; GetClientPosition must start at zero
    # when the QML editor moves below the find bar or inside another layout.
    set(platform_source "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/scintillaquick_platqt.cpp")
    file(READ "${platform_source}" platform_code)
    set(original "PRectangle Window::GetClientPosition() const\n{\n    // The client position is the window position\n    return GetPosition();\n}")
    set(replacement "PRectangle Window::GetClientPosition() const\n{\n    QQuickItem* item = resolve_window_item_for_owner(*this);\n    return item ? PRectangle(0, 0, item->width(), item->height())\n                : PRectangle(0, 0, 1000, 1000);\n}")
    string(FIND "${platform_code}" "${original}" match)
    if(match EQUAL -1)
        message(FATAL_ERROR "Update the MiaCode ScintillaQuick client-coordinate patch for this dependency revision")
    endif()
    string(REPLACE "${original}" "${replacement}" patched_code "${platform_code}")
    set(patched_source "${CMAKE_CURRENT_BINARY_DIR}/miacode_scintillaquick_platqt.cpp")
    file(CONFIGURE OUTPUT "${patched_source}" CONTENT "${patched_code}" @ONLY)
    get_target_property(sources ScintillaQuick SOURCES)
    list(REMOVE_ITEM sources src/platform/scintillaquick_platqt.cpp)
    set_property(TARGET ScintillaQuick PROPERTY SOURCES "${sources}")
    target_sources(ScintillaQuick PRIVATE "${patched_source}")
endfunction()
cmake_language(DEFER CALL miacode_scintillaquick_local_coordinates)
