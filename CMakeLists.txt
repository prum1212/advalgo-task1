cmake_minimum_required(VERSION 3.16)
project(multigrep_gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Feature Toggle Flag
option(ENABLE_BENCHMARK_SUITE "Compile internal benchmark suite into GUI" OFF)

if(ENABLE_BENCHMARK_SUITE)
    add_compile_definitions(ENABLE_BENCHMARK_SUITE)
    message(STATUS "Benchmark Suite Enabled: -DENABLE_BENCHMARK_SUITE will be defined.")
endif()

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.89.9
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

include_directories(
    include 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
)

add_executable(multigrep_gui 
    src/gui_main.cpp 
    src/ahocorasick.cpp 
    ${IMGUI_SOURCES}
)

target_link_libraries(multigrep_gui PRIVATE glfw OpenGL::GL Threads::Threads ${CMAKE_DL_LIBS})