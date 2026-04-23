cmake_minimum_required(VERSION 3.21)
project(VTKQtCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt MOC auto-generation
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# ---------------------------------------------------------------------------
# Qt – try Qt6 first, fall back to Qt5
# ---------------------------------------------------------------------------
set(QT_COMMON_COMPONENTS Core Widgets Gui Network OpenGL)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS ${QT_COMMON_COMPONENTS})

if(QT_VERSION_MAJOR EQUAL 6)
    set(QT_VERSION_SPECIFIC_COMPONENTS OpenGLWidgets)
else()
    set(QT_VERSION_SPECIFIC_COMPONENTS)
endif()

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
    ${QT_COMMON_COMPONENTS}
    ${QT_VERSION_SPECIFIC_COMPONENTS}
)

set(VTKQTCORE_HAS_QT_MULTIMEDIA OFF)
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Multimedia)
if(TARGET Qt${QT_VERSION_MAJOR}::Multimedia)
    set(VTKQTCORE_HAS_QT_MULTIMEDIA ON)
    message(STATUS "Qt${QT_VERSION_MAJOR} Multimedia found: prompt audio playback enabled")
else()
    message(WARNING "Qt${QT_VERSION_MAJOR} Multimedia not found: prompt audio playback will be disabled")
endif()

# ---------------------------------------------------------------------------
# hiredis
# ---------------------------------------------------------------------------
find_package(hiredis CONFIG REQUIRED
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/thirtyPart/hiredis"
    NO_DEFAULT_PATH)

# ---------------------------------------------------------------------------
# VTK
# ---------------------------------------------------------------------------
set(VTK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirtyPart/vtk/lib/cmake/vtk-9.4")
find_package(VTK REQUIRED COMPONENTS
    GUISupportQt
    RenderingOpenGL2
    RenderingAnnotation
    RenderingLabel
    InteractionStyle
    InteractionWidgets
    FiltersSources
    FiltersGeneral
    CommonCore
    CommonDataModel
    CommonTransforms
)

# ---------------------------------------------------------------------------
# Sources – collect every .h/.cpp/.ui/.qrc under src/
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.qrc
)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(VTKQtCore ${SOURCES})

# ---------------------------------------------------------------------------
# Include directories – src/ and every subdirectory beneath it
# ---------------------------------------------------------------------------
file(GLOB_RECURSE ALL_SRC_FILES CONFIGURE_DEPENDS LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/src/*)
set(INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src)
foreach(ENTRY ${ALL_SRC_FILES})
    if(IS_DIRECTORY ${ENTRY})
        list(APPEND INCLUDE_DIRS ${ENTRY})
    endif()
endforeach()
list(REMOVE_DUPLICATES INCLUDE_DIRS)

list(APPEND INCLUDE_DIRS ${hiredis_INCLUDE_DIRS})

target_include_directories(VTKQtCore PRIVATE ${INCLUDE_DIRS})

target_compile_definitions(VTKQtCore PRIVATE
    VTKQTCORE_HAS_QT_MULTIMEDIA=$<BOOL:${VTKQTCORE_HAS_QT_MULTIMEDIA}>)

# ---------------------------------------------------------------------------
# Link libraries
# ---------------------------------------------------------------------------
target_link_libraries(VTKQtCore PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::OpenGL
    hiredis::hiredis
    ${VTK_LIBRARIES}
)

if(VTKQTCORE_HAS_QT_MULTIMEDIA)
    target_link_libraries(VTKQtCore PRIVATE Qt${QT_VERSION_MAJOR}::Multimedia)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(VTKQtCore PRIVATE Qt${QT_VERSION_MAJOR}::OpenGLWidgets)
endif()

if(WIN32)
    get_target_property(QT_CORE_LOCATION_DEBUG Qt${QT_VERSION_MAJOR}::Core IMPORTED_LOCATION_DEBUG)
    get_target_property(QT_CORE_LOCATION_RELEASE Qt${QT_VERSION_MAJOR}::Core IMPORTED_LOCATION_RELEASE)
    get_target_property(QT_CORE_LOCATION Qt${QT_VERSION_MAJOR}::Core IMPORTED_LOCATION)

    if(QT_CORE_LOCATION_DEBUG)
        set(QT_CORE_LOCATION_FOR_DEPLOY "${QT_CORE_LOCATION_DEBUG}")
    elseif(QT_CORE_LOCATION_RELEASE)
        set(QT_CORE_LOCATION_FOR_DEPLOY "${QT_CORE_LOCATION_RELEASE}")
    else()
        set(QT_CORE_LOCATION_FOR_DEPLOY "${QT_CORE_LOCATION}")
    endif()

    get_filename_component(QT_BIN_DIR "${QT_CORE_LOCATION_FOR_DEPLOY}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt windeployqt6
        HINTS "${QT_BIN_DIR}"
        REQUIRED)

    add_custom_command(TARGET VTKQtCore POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:VTKQtCore>
            $<TARGET_FILE_DIR:VTKQtCore>
        COMMAND_EXPAND_LISTS)

    add_custom_command(TARGET VTKQtCore POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            "$<IF:$<CONFIG:Debug>,--debug,--release>"
            --no-translations
            --dir "$<TARGET_FILE_DIR:VTKQtCore>"
            "$<TARGET_FILE:VTKQtCore>")

    get_target_property(VTK_QT_SUPPORT_LOCATION_DEBUG VTK::GUISupportQt IMPORTED_LOCATION_DEBUG)
    get_target_property(QT_CORE_LOCATION_RELEASE Qt${QT_VERSION_MAJOR}::Core IMPORTED_LOCATION_RELEASE)
    get_target_property(QT_GUI_LOCATION_RELEASE Qt${QT_VERSION_MAJOR}::Gui IMPORTED_LOCATION_RELEASE)
    get_target_property(QT_WIDGETS_LOCATION_RELEASE Qt${QT_VERSION_MAJOR}::Widgets IMPORTED_LOCATION_RELEASE)
    get_target_property(QT_OPENGL_LOCATION_RELEASE Qt${QT_VERSION_MAJOR}::OpenGL IMPORTED_LOCATION_RELEASE)

    if(QT_VERSION_MAJOR EQUAL 6)
        get_target_property(QT_OPENGLWIDGETS_LOCATION_RELEASE Qt${QT_VERSION_MAJOR}::OpenGLWidgets IMPORTED_LOCATION_RELEASE)
    endif()

    if(MSVC AND NOT VTK_QT_SUPPORT_LOCATION_DEBUG)
        add_custom_command(TARGET VTKQtCore POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${QT_CORE_LOCATION_RELEASE}"
                "${QT_GUI_LOCATION_RELEASE}"
                "${QT_WIDGETS_LOCATION_RELEASE}"
                "${QT_OPENGL_LOCATION_RELEASE}"
                "$<TARGET_FILE_DIR:VTKQtCore>"
            COMMAND_EXPAND_LISTS)
    endif()

    if(MSVC AND NOT VTK_QT_SUPPORT_LOCATION_DEBUG AND QT_VERSION_MAJOR EQUAL 6)
        add_custom_command(TARGET VTKQtCore POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${QT_OPENGLWIDGETS_LOCATION_RELEASE}"
                "$<TARGET_FILE_DIR:VTKQtCore>"
            COMMAND_EXPAND_LISTS)
    endif()
endif()

# Initialize VTK modules (required for vtk_module_autoinit)
vtk_module_autoinit(
    TARGETS VTKQtCore
    MODULES ${VTK_LIBRARIES}
)
