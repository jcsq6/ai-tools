file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*")
file(GLOB HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

add_library(ai ${SRC_FILES} ${HEADER_FILES})

find_package(CURL REQUIRED)

target_link_libraries(ai PUBLIC CURL::libcurl json cppcodec)
target_include_directories(ai PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(ai_test "test.cpp")
target_link_libraries(ai_test PUBLIC ai)

add_executable(tool_test "tool_test.cpp")
target_link_libraries(tool_test PUBLIC ai)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
    target_link_libraries(ai_test PUBLIC stdc++exp)
    target_link_libraries(tool_test PUBLIC stdc++exp)
endif()