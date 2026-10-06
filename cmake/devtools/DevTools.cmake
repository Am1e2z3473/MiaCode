include(${CMAKE_CURRENT_LIST_DIR}/SharedSources.cmake)

# Qt dependencies for diagnostic executables and specs.
find_package(Qt6 6.10 REQUIRED COMPONENTS Test Network Widgets)

# Mirror the product QML module for specs that instantiate its components.
set(MIACODE_QML_SPEC_IMPORT_ROOT "${CMAKE_CURRENT_BINARY_DIR}/qml_spec_imports")
set(_miacode_qml_spec_module_dir "${MIACODE_QML_SPEC_IMPORT_ROOT}/MiaCode/UI")
set(_miacode_qml_spec_qmldir "module MiaCode.UI\n")
foreach(qml_file IN LISTS MIACODE_UI_QML_FILES)
    get_filename_component(_qml_name "${qml_file}" NAME)
    get_filename_component(_qml_type "${qml_file}" NAME_WE)
    if (_qml_name STREQUAL "Theme.qml")
        string(APPEND _miacode_qml_spec_qmldir "singleton ${_qml_type} 1.0 ${_qml_name}\n")
    else()
        string(APPEND _miacode_qml_spec_qmldir "${_qml_type} 1.0 ${_qml_name}\n")
    endif()
    configure_file("${qml_file}" "${_miacode_qml_spec_module_dir}/${_qml_name}" COPYONLY)
endforeach()
file(WRITE "${_miacode_qml_spec_module_dir}/qmldir" "${_miacode_qml_spec_qmldir}")

# Shared executable registration; TEST also adds a CTest entry.
function(miacode_add_dev_tool NAME)
    cmake_parse_arguments(DT "TEST" "" "SOURCES;LIBS;INCLUDES" ${ARGN})
    add_executable(${NAME} ${DT_SOURCES})
    # Keep QObject-based specs self-contained even when a source header is
    # also part of the main application target.
    set_target_properties(${NAME} PROPERTIES AUTOMOC ON)
    if (DT_LIBS)
        target_link_libraries(${NAME} PRIVATE ${DT_LIBS})
    endif()
    if (DT_INCLUDES)
        target_include_directories(${NAME} PRIVATE ${DT_INCLUDES})
    endif()
    if (DT_TEST)
        # Resolve product assets relative to the source tree.
        add_test(NAME ${NAME} COMMAND ${NAME} WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        # Make Qt's runtime available to specs on Windows, and resolve Qt plugins
        # (the offscreen QPA used by QML specs) from the Qt installation rather
        # than from whatever windeployqt staged beside MiaCode.
        set_tests_properties(${NAME} PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;QT_PLUGIN_PATH=set:${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}")
    endif()
endfunction()

include(${CMAKE_CURRENT_LIST_DIR}/CliTools.cmake)

include(${CMAKE_CURRENT_LIST_DIR}/MiaCodeSpecRegistry.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/specs/index.cmake)
miacode_finalize_spec_registry()
