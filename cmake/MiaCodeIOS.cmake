# iOS shares the desktop application and its module graph.
set(MIACODE_BASS_IOS_DIR "${PROJECT_SOURCE_DIR}/third_party/bass/lib/ios"
    CACHE PATH "BASS iOS XCFramework directory")
set(MIACODE_IOS_DEVELOPMENT_TEAM "" CACHE STRING "Apple development team for device signing")
set(MIACODE_BASS_IOS_FRAMEWORKS)
foreach(name bass bassmix bass_fx bassopus bassflac)
    set(xcframework "${MIACODE_BASS_IOS_DIR}/${name}.xcframework")
    if(NOT EXISTS "${xcframework}/Info.plist")
        message(FATAL_ERROR "Missing ${xcframework}; run scripts/build/provision-ios-dependencies.sh")
    endif()
    list(APPEND MIACODE_BASS_IOS_FRAMEWORKS "${xcframework}")
endforeach()

function(miacode_configure_ios_app target)
    set_target_properties(${target} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/resources/ios/Info.plist.in"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${MIACODE_IOS_DEVELOPMENT_TEAM}"
        XCODE_EMBED_FRAMEWORKS "${MIACODE_BASS_IOS_FRAMEWORKS}"
        XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY YES
        XCODE_EMBED_FRAMEWORKS_REMOVE_HEADERS_ON_COPY YES
        BUILD_RPATH "@executable_path/Frameworks"
        INSTALL_RPATH "@executable_path/Frameworks"
    )
    target_sources(${target} PRIVATE
        resources/ios/Assets.xcassets
        src/app/ui/chrome/WindowChromeIOS.mm
        src/app/ui/editor/PointerInputIOS.mm
        src/app/platform/DocumentFileAccessIOS.mm
        src/app/platform/DocumentFileAccess.h)
    set_source_files_properties(resources/ios/Assets.xcassets PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    target_link_libraries(${target} PRIVATE Qt6::GuiPrivate "-framework UIKit" "-framework UniformTypeIdentifiers")
    file(GLOB_RECURSE assets CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/assets/*")
    foreach(asset IN LISTS assets)
        file(RELATIVE_PATH relative "${PROJECT_SOURCE_DIR}" "${asset}")
        get_filename_component(directory "${relative}" DIRECTORY)
        set_source_files_properties("${asset}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/${directory}")
    endforeach()
    target_sources(${target} PRIVATE ${assets})
endfunction()
