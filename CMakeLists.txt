cmake_minimum_required(VERSION 3.16)
project(termacs_core CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- tvision backend dependency ---
set(TV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TV_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/tvision)

# --- vendored librope (MIT; UTF-8 rope backing the text-editing model §5.11) ---
add_library(librope STATIC third_party/librope/rope.c)
target_include_directories(librope PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/librope)
set_property(TARGET librope PROPERTY POSITION_INDEPENDENT_CODE ON)

# --- the termacs core library (framework + C ABI later) ---
add_library(termacs STATIC
    src/cell.cpp
    src/theme.cpp
    src/core.cpp
    src/widget.cpp
    src/application.cpp
    src/text_buffer.cpp
    backends/tvision/tvision_backend.cpp
    cabi/cabi.cpp
)
target_include_directories(termacs
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(termacs PUBLIC tvision librope)
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

add_executable(tc_widgets tests/cpp/widgets_test.cpp)
target_link_libraries(tc_widgets termacs)
add_test(NAME widgets COMMAND tc_widgets)

add_executable(tc_librope tests/cpp/librope_smoke.cpp)
target_link_libraries(tc_librope termacs)
add_test(NAME librope COMMAND tc_librope)

add_executable(tc_textbuffer tests/cpp/textbuffer_test.cpp)
target_link_libraries(tc_textbuffer termacs)
add_test(NAME textbuffer COMMAND tc_textbuffer)

add_executable(tc_edit tests/cpp/edit_test.cpp)
target_link_libraries(tc_edit termacs)
add_test(NAME edit COMMAND tc_edit)

# Shared C-ABI library (libtermacs.so) — what the language bindings link against
add_library(termacs_c SHARED cabi/cabi.cpp
    src/cell.cpp src/theme.cpp src/core.cpp src/widget.cpp src/application.cpp
    src/text_buffer.cpp backends/tvision/tvision_backend.cpp)
target_include_directories(termacs_c PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(termacs_c PRIVATE tvision librope)
set_target_properties(termacs_c PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON OUTPUT_NAME termacs)
target_link_options(termacs_c PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/cabi/termacs.map")

# NOTE: the JNI shim (libtermacsjni.so) is NO LONGER built here. Under the
# repos-as-workspace model (spec §15.2), the JNI build is owned by termacs-java,
# which consumes termacs-core via FetchContent. termacs-core knows nothing of
# Java or JNI — it exposes only the public C++ headers and the C ABI.

add_executable(tvterm_smoke tests/cpp/tvterm_smoke.cpp)
target_link_libraries(tvterm_smoke termacs)

# --- install / export: headers + libs consumable downstream (R0) ---
include(GNUInstallDirs)
install(TARGETS termacs termacs_c
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}   # libtermacs.a  (static core)
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})  # libtermacs.so (shared C ABI)
install(DIRECTORY include/termacs DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
