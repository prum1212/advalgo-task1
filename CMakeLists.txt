cmake_minimum_required(VERSION 3.16)
project(multigrep_gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenGL & Threads
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

# Fetch GLFW automatically
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)
FetchContent_MakeAvailable(glfw)

# Fetch Dear ImGui automatically
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.89.9
)
FetchContent_MakeAvailable(imgui)

# ImGui Core Sources
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

include_directories(include ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)

add_executable(multigrep_gui 
    src/gui_main.cpp 
    src/ahocorasick.cpp 
    ${IMGUI_SOURCES}
)

target_link_libraries(multigrep_gui PRIVATE glfw OpenGL::GL Threads::Threads ${CMAKE_DL_LIBS})