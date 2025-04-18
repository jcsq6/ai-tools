file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cpp")
if(APPLE)
    file(GLOB OBJC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.mm")
    list(APPEND SRC_FILES ${OBJC_FILES})
endif()

add_library(sys STATIC ${SRC_FILES})
target_include_directories(sys PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(sys_test test.cpp)
target_link_libraries(sys_test PRIVATE sys)

if (APPLE)
    target_link_libraries(sys PUBLIC
        "-framework Foundation"
        "-framework ApplicationServices"
        "-framework Cocoa"
        "-framework ScreenCaptureKit"
        "-framework CoreMedia"
        "-framework AVFoundation"
        "-framework CoreVideo"
        "-framework CoreImage"
        "-framework UniformTypeIdentifiers"
    )

    target_compile_options(sys PRIVATE
        -fobjc-arc
    )

    setup_macos_app(sys_test
        BUNDLE_ID      "com.jcsq6.sys_test"
        VERSION        "${PROJECT_VERSION}"
        PLIST_TEMPLATE "${CMAKE_SOURCE_DIR}/cmake/plists/Info.plist.in"
        ENTITLEMENTS   "${CMAKE_SOURCE_DIR}/cmake/entitlements/apple-events.plist"
    )
endif()

if (MSVC)
    target_compile_options(sys PUBLIC /Zc:__cplusplus)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
    target_link_libraries(sys PUBLIC stdc++exp)
endif()