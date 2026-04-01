cmake_minimum_required(VERSION 3.16)
project(VTKQtCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt MOC auto-generation
set(CMAKE_AUTOMOC ON)

# ---------------------------------------------------------------------------
# Qt – try Qt6 first, fall back to Qt5
# ---------------------------------------------------------------------------
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Gui Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Gui Network)

# ---------------------------------------------------------------------------
# VTK
# ---------------------------------------------------------------------------
find_package(VTK REQUIRED COMPONENTS
    GUISupportQt
    RenderingOpenGL2
    RenderingAnnotation
    InteractionStyle
    InteractionWidgets
    FiltersSources
    FiltersGeneral
    CommonCore
    CommonDataModel
    CommonTransforms
)

# ---------------------------------------------------------------------------
# Sources – collect every .h and .cpp under src/
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(VTKQtCore ${SOURCES})

# ---------------------------------------------------------------------------
# Include directories – src/ and every subdirectory beneath it
# ---------------------------------------------------------------------------
file(GLOB_RECURSE ALL_SRC_FILES LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/src/*)
set(INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src)
foreach(ENTRY ${ALL_SRC_FILES})
    if(IS_DIRECTORY ${ENTRY})
        list(APPEND INCLUDE_DIRS ${ENTRY})
    endif()
endforeach()
list(REMOVE_DUPLICATES INCLUDE_DIRS)

target_include_directories(VTKQtCore PRIVATE ${INCLUDE_DIRS})

# ---------------------------------------------------------------------------
# Link libraries
# ---------------------------------------------------------------------------
target_link_libraries(VTKQtCore PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Network
    ${VTK_LIBRARIES}
)

# Initialize VTK modules (required for vtk_module_autoinit)
vtk_module_autoinit(
    TARGETS VTKQtCore
    MODULES ${VTK_LIBRARIES}
)
