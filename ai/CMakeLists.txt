file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*")
file(GLOB HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

add_library(ai ${SRC_FILES} ${HEADER_FILES})

find_package(CURL REQUIRED)
find_package(ICU REQUIRED COMPONENTS i18n uc data)

target_link_libraries(ai PUBLIC CURL::libcurl json cppcodec ICU::i18n ICU::uc ICU::data)
target_include_directories(ai PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

message(STATUS "ICU_INCLUDE_DIRS: ${ICU_INCLUDE_DIRS}")

add_executable(ai_test "test.cpp")
target_link_libraries(ai_test PUBLIC ai)

add_executable(tool_test "tool_test.cpp")
target_link_libraries(tool_test PUBLIC ai)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
    target_link_libraries(ai PUBLIC stdc++exp)
    target_link_libraries(ai_test PUBLIC stdc++exp)
    target_link_libraries(tool_test PUBLIC stdc++exp)
endif()