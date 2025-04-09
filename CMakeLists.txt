cmake_minimum_required(VERSION 3.10)

project(MyProject VERSION 1.0 LANGUAGES CXX)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(QT_DEFAULT_MAJOR_VERSION 6)
add_subdirectory(dep)
add_subdirectory(ai)
add_subdirectory(ui)