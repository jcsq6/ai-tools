find_package(Qt6 COMPONENTS Widgets REQUIRED)
qt_standard_project_setup()

file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*")
file(GLOB HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

qt_add_executable(ai-tool main.cpp ${SRC_FILES} ${HEADER_FILES})

target_link_libraries(ai-tool PUBLIC Qt6::Widgets Qt6::Core Qt6::Gui ai)
target_include_directories(ai-tool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# set_target_properties(ai-tool PROPERTIES
#     WIN32_EXECUTABLE TRUE
#     MACOSX_BUNDLE TRUE
# )

if (MSVC)
    target_compile_options(ai-tool PUBLIC /Zc:__cplusplus)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
    target_link_libraries(ai-tool PUBLIC stdc++exp)
endif()