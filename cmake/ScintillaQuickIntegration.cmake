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

function(miacode_scintillaquick_surface_background)
    # Scintilla's style messages only carry RGB. Preserve the public item's
    # optional scene background colour in the captured Scene Graph primitives;
    # QML can then own a single translucent surface beneath text and gutters.
    set(item_source "${CMAKE_CURRENT_SOURCE_DIR}/src/public/scintillaquick_item.cpp")
    file(READ "${item_source}" item_code)
    set(original "    m_render_data->captured_caret_primitives = frame.caret_primitives;")
    set(replacement [=[    const QVariant scene_background = property("sceneBackgroundColor");
    if (scene_background.isValid()) {
        const QColor style_background = QColorFromColourRGBA(m_core->vs.styles[StyleDefault].back);
        const QColor surface_background = scene_background.value<QColor>();
        snapshot.background = surface_background;
        for (auto& band : snapshot.gutter_bands) {
            if (band.color == style_background) band.color = surface_background;
            if (band.pattern_color == style_background) band.pattern_color = surface_background;
        }
        for (auto& primitive : frame.background_primitives) {
            if (primitive.color == style_background && !primitive.marker_underline)
                primitive.color = surface_background;
        }
    }

    m_render_data->captured_caret_primitives = frame.caret_primitives;]=])
    string(FIND "${item_code}" "${original}" match)
    if(match EQUAL -1)
        message(FATAL_ERROR "Update the MiaCode ScintillaQuick surface-background patch for this dependency revision")
    endif()
    string(REPLACE "${original}" "${replacement}" patched_code "${item_code}")
    set(patched_source "${CMAKE_CURRENT_BINARY_DIR}/miacode_scintillaquick_item.cpp")
    file(CONFIGURE OUTPUT "${patched_source}" CONTENT "${patched_code}" @ONLY)
    get_target_property(sources ScintillaQuick SOURCES)
    list(REMOVE_ITEM sources src/public/scintillaquick_item.cpp)
    set_property(TARGET ScintillaQuick PROPERTY SOURCES "${sources}")
    target_sources(ScintillaQuick PRIVATE "${patched_source}")
endfunction()
cmake_language(DEFER CALL miacode_scintillaquick_surface_background)
