cmake_minimum_required(VERSION 3.16)
project(CriLib LANGUAGES CXX)
find_package(Threads REQUIRED)

set(tinyxml2_BUILD_TESTING OFF)
set(tinyxml2_SHARED_LIBS OFF)
add_subdirectory(dependencies/tinyxml2 EXCLUDE_FROM_ALL)

add_library(CriLib STATIC
    src/CriLib/Cpk.cpp
    src/CriLib/UtfTable.cpp
    src/CriLib/CriLayla.cpp
    src/CriLib/CriLibXml.cpp
)
target_include_directories(CriLib PUBLIC src/CriLib)
target_compile_features(CriLib PUBLIC cxx_std_17)
target_link_libraries(CriLib PUBLIC Threads::Threads PRIVATE tinyxml2::tinyxml2)

add_executable(cpktool src/cpktool/main.cpp)
target_link_libraries(cpktool PRIVATE CriLib)
