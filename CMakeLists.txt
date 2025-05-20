cmake_minimum_required(VERSION 3.24)

project(ai-tools VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(QT_DEFAULT_MAJOR_VERSION 6)
include(cmake/make_app.cmake)
add_subdirectory(dep)
add_subdirectory(system)
add_subdirectory(ai)
add_subdirectory(ui)