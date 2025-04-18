# MacApp.cmake - Helper to configure macOS app bundles with Info.plist, entitlements, and code signing.
# Usage:
#   include("cmake/MacApp.cmake")
#   add_executable(MyApp MACOSX_BUNDLE src/main.cpp)
#   setup_macos_app(MyApp
#       BUNDLE_ID      com.example.MyApp
#       VERSION        ${PROJECT_VERSION}
#       PLIST_TEMPLATE ${CMAKE_SOURCE_DIR}/cmake/plists/Info.plist.in
#       ENTITLEMENTS   ${CMAKE_SOURCE_DIR}/cmake/entitlements/macos.hardened-runtime.plist
#   )

function(setup_macos_app target)
    # Parse arguments: single-value args: BUNDLE_ID, VERSION, PLIST_TEMPLATE, ENTITLEMENTS
    cmake_parse_arguments(
        SETUP_MACAPP       # prefix for parsed variables
        ""                # no boolean options
        "BUNDLE_ID;VERSION;PLIST_TEMPLATE;ENTITLEMENTS"  # single-value args
        ""                # no multi-value args
        ${ARGN}
    )

    if(NOT APPLE)
        message(STATUS "Skipping macOS app setup for ${target}: not on Apple platform.")
        return()
    endif()

    # Ensure mandatory args
    if(NOT SETUP_MACAPP_PLIST_TEMPLATE)
        message(FATAL_ERROR "setup_macos_app: PLIST_TEMPLATE argument is required for target ${target}")
    endif()
    if(NOT SETUP_MACAPP_BUNDLE_ID)
        message(FATAL_ERROR "setup_macos_app: BUNDLE_ID argument is required for target ${target}")
    endif()
    if(NOT SETUP_MACAPP_VERSION)
        message(FATAL_ERROR "setup_macos_app: VERSION argument is required for target ${target}")
    endif()

    # 1) Configure Info.plist from template
    set(plist_in  "${SETUP_MACAPP_PLIST_TEMPLATE}")
    get_filename_component(plist_name "${plist_in}" NAME)
    set(plist_out "${CMAKE_CURRENT_BINARY_DIR}/${target}_${plist_name}")
    configure_file(
        "${plist_in}"  
        "${plist_out}" 
        @ONLY
    )
    set_target_properties(${target} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST     "${plist_out}"
        XCODE_ATTRIBUTE_INFOPLIST_FILE "${plist_out}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${SETUP_MACAPP_BUNDLE_ID}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${SETUP_MACAPP_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION     "${SETUP_MACAPP_VERSION}"
    )

    # 2) Copy and embed entitlements if provided
    if(SETUP_MACAPP_ENTITLEMENTS)
        set(ent_in   "${SETUP_MACAPP_ENTITLEMENTS}")
        get_filename_component(ent_name "${ent_in}" NAME)
        set(ent_out  "${CMAKE_CURRENT_BINARY_DIR}/${ent_name}")
        configure_file(
            "${ent_in}"
            "${ent_out}"
            COPYONLY
        )
        set_target_properties(${target} PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${ent_out}"
        )
    endif()

    # 3) Code signing: Xcode generator or manual post-build for other generators
    if(DEFINED APPLE_SIGN_ID)
        string(REPLACE "(" "\\(" _APPLE_SIGN_ID_ESC "${APPLE_SIGN_ID}")
        string(REPLACE ")" "\\)" _APPLE_SIGN_ID_ESC "${_APPLE_SIGN_ID_ESC}")
        # Xcode generator: automatic signing
        set_target_properties(${target} PROPERTIES
            CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM   "${APPLE_TEAM_ID}"
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE    "Automatic"
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${APPLE_SIGN_ID}"
        )

        # Non-Xcode generators: add final codesign step
        if(NOT "${CMAKE_GENERATOR}" MATCHES "Xcode")
            if(SETUP_MACAPP_ENTITLEMENTS)
                set(_MACAPP_CODESIGN_ENTS --entitlements "${ent_out}")
            else()
                set(_MACAPP_CODESIGN_ENTS "")
            endif()

            # Non‑Xcode generators: final codesign
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND /usr/bin/codesign
                ARGS --force --deep --timestamp
                    ${_MACAPP_CODESIGN_ENTS}
                    --sign
                    "${_APPLE_SIGN_ID_ESC}"
                    "$<TARGET_BUNDLE_DIR:${target}>"
                COMMENT "Codesigning ${target}.app with ${APPLE_SIGN_ID}..."
            )
        endif()
    else()
        message(STATUS "APPLE_TEAM_ID and/or APPLE_SIGN_ID not set; ${target} will be ad-hoc signed.")
    endif()
endfunction()
