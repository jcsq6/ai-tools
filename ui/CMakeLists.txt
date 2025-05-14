find_package(Qt6 COMPONENTS Widgets REQUIRED)

qt_standard_project_setup()

file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*")
file(GLOB HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/*")

file(GLOB RESOURCE_FILES "${CMAKE_SOURCE_DIR}/assets/*.qrc")

set(CMAKE_AUTORCC ON)
qt_add_library(ui STATIC
    ${RESOURCE_FILES}
    ${SRC_FILES}
    ${HEADER_FILES}
)

target_link_libraries(ui PUBLIC ai Qt6::Widgets Qt6::Core Qt6::Gui QHotkey::QHotkey json sys)
target_include_directories(ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# set_target_properties(ai-tool PROPERTIES
#     WIN32_EXECUTABLE TRUE
#     MACOSX_BUNDLE TRUE
# )

if (MSVC)
    target_compile_options(ui PUBLIC /Zc:__cplusplus)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
    target_link_libraries(ui PUBLIC stdc++exp)
endif()

add_executable(ai-tool main.cpp)
target_link_libraries(ai-tool PUBLIC ui)

add_executable(ai-test test.cpp)
target_link_libraries(ai-test PUBLIC ui)