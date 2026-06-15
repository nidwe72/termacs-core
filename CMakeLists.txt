cmake_minimum_required(VERSION 3.16)
project(termacs_core CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- tvision backend dependency ---
set(TV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TV_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/tvision)

# --- the termacs core library (framework + C ABI later) ---
add_library(termacs STATIC
    src/cell.cpp
    src/theme.cpp
    src/core.cpp
    src/widget.cpp
    src/application.cpp
    backends/tvision/tvision_backend.cpp
    cabi/cabi.cpp
)
target_include_directories(termacs
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(termacs PUBLIC tvision)
set_target_properties(termacs PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)

# --- tests ---
enable_testing()
add_executable(tc_snapshot tests/cpp/snapshot_test.cpp)
target_link_libraries(tc_snapshot termacs)
add_test(NAME snapshot COMMAND tc_snapshot)
add_executable(tc_demo tests/cpp/demo_test.cpp)
target_link_libraries(tc_demo termacs)
add_test(NAME demo COMMAND tc_demo)

add_executable(tc_csmoke tests/c-smoke/smoke.c)
target_link_libraries(tc_csmoke termacs)
set_target_properties(tc_csmoke PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME csmoke COMMAND tc_csmoke)

# Shared C-ABI library (what Python/Dart load; JNI statically links it in P3)
add_library(termacs_c SHARED cabi/cabi.cpp
    src/cell.cpp src/theme.cpp src/core.cpp src/widget.cpp src/application.cpp
    backends/tvision/tvision_backend.cpp)
target_include_directories(termacs_c PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(termacs_c PRIVATE tvision)
set_target_properties(termacs_c PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON OUTPUT_NAME termacs)
target_link_options(termacs_c PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/cabi/termacs.map")

# --- JNI shim: single self-contained libtermacsjni.so (JNI + C ABI + core + tvision) ---
find_package(JNI QUIET)
if(JNI_FOUND)
    add_library(termacsjni SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/../termacs-java/native/termacs_jni.cpp
        cabi/cabi.cpp
        src/cell.cpp src/theme.cpp src/core.cpp src/widget.cpp src/application.cpp
        backends/tvision/tvision_backend.cpp)
    target_include_directories(termacsjni PRIVATE ${JNI_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(termacsjni PRIVATE tvision)
    set_target_properties(termacsjni PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
    target_link_options(termacsjni PRIVATE
        "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/../termacs-java/native/jni.map")
    message(STATUS "termacs: JNI found, building libtermacsjni")
else()
    message(STATUS "termacs: JNI not found, skipping libtermacsjni")
endif()
add_executable(tvterm_smoke tests/cpp/tvterm_smoke.cpp)
target_link_libraries(tvterm_smoke termacs)
