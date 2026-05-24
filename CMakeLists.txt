cmake_minimum_required(VERSION 3.21)
project(VTKQtCore LANGUAGES CXX)

include(CMakeParseArguments)

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
# VTK
# ---------------------------------------------------------------------------
set(VTK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirtyPart/vtk/lib/cmake/vtk-9.2")
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

list(FILTER SOURCES EXCLUDE REGEX ".*/src/logic/socket_client/.*")
set(APP_CORE_SOURCES ${SOURCES})
list(FILTER APP_CORE_SOURCES EXCLUDE REGEX ".*/src/main\\.cpp$")
list(FILTER APP_CORE_SOURCES EXCLUDE REGEX ".*/src/products/.*")
list(FILTER APP_CORE_SOURCES EXCLUDE REGEX ".*/src/app/software/ModuleUiAssemblers\\.cpp$")
list(FILTER APP_CORE_SOURCES EXCLUDE REGEX ".*/src/app/software/SoftwareInitializerFactory\\.cpp$")
list(FILTER APP_CORE_SOURCES EXCLUDE REGEX ".*/src/app/software/ConcreteSoftwareInitializers/.*")
list(FILTER APP_CORE_SOURCES EXCLUDE REGEX ".*/src/modules/datagen/.*")
list(FILTER APP_CORE_SOURCES EXCLUDE REGEX ".*/src/modules/params/.*")
list(FILTER APP_CORE_SOURCES EXCLUDE REGEX ".*/src/modules/pointpick/.*")
list(FILTER APP_CORE_SOURCES EXCLUDE REGEX ".*/src/modules/planning/.*")
list(FILTER APP_CORE_SOURCES EXCLUDE REGEX ".*/src/modules/navigation/.*")
list(FILTER APP_CORE_SOURCES EXCLUDE REGEX ".*/src/modules/reconstruction/.*")
list(FILTER APP_CORE_SOURCES EXCLUDE REGEX ".*/src/modules/kneePlane/.*")

# ---------------------------------------------------------------------------
# Core library and products
# ---------------------------------------------------------------------------
set(SOCKET_CLIENT_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SOCKET_CLIENT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SOCKET_CLIENT_ASIO_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/thirtyPart/asio-src/asio/include"
    CACHE PATH "Path to the Asio include directory containing asio.hpp" FORCE)
add_subdirectory(src/logic/socket_client)

add_library(vtkqtcore_core STATIC ${APP_CORE_SOURCES})

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

target_include_directories(vtkqtcore_core PUBLIC ${INCLUDE_DIRS})

target_compile_definitions(vtkqtcore_core PUBLIC
    VTKQTCORE_HAS_QT_MULTIMEDIA=$<BOOL:${VTKQTCORE_HAS_QT_MULTIMEDIA}>)

# ---------------------------------------------------------------------------
# Link libraries
# ---------------------------------------------------------------------------
target_link_libraries(vtkqtcore_core PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::OpenGL
    socket_dc::socket_client
    ${VTK_LIBRARIES}
)

if(VTKQTCORE_HAS_QT_MULTIMEDIA)
    target_link_libraries(vtkqtcore_core PUBLIC Qt${QT_VERSION_MAJOR}::Multimedia)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(vtkqtcore_core PUBLIC Qt${QT_VERSION_MAJOR}::OpenGLWidgets)
endif()

set(VTKQTCORE_MODULE_DATAGEN_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/datagen/DataGenModuleLogicHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/datagen/DataGenModuleLogicHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/datagen/DataGenPage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/datagen/DataGenPage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/datagen/DataGenModulePack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/datagen/DataGenModulePack.h)

set(VTKQTCORE_MODULE_PARAMS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/params/ParamsModuleLogicHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/params/ParamsModuleLogicHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/params/ParamsPage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/params/ParamsPage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/params/ParamsUiCommands.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/params/ParamsModulePack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/params/ParamsModulePack.h)

set(VTKQTCORE_MODULE_POINTPICK_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/pointpick/PointPickModuleLogicHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/pointpick/PointPickModuleLogicHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/pointpick/PointPickPage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/pointpick/PointPickPage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/pointpick/PointPickPage.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/pointpick/PointPickStatusPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/pointpick/PointPickStatusPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/pointpick/PointPickStatusPanel.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/pointpick/PointPickUiCommands.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/pointpick/pointpick.qrc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/pointpick/PointPickModulePack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/pointpick/PointPickModulePack.h)

set(VTKQTCORE_MODULE_PLANNING_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/planning/PlanningModuleLogicHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/planning/PlanningModuleLogicHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/planning/PlanningPage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/planning/PlanningPage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/planning/PlanningUiCommands.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/planning/PlanningModulePack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/planning/PlanningModulePack.h)

set(VTKQTCORE_MODULE_NAVIGATION_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/navigation/NavigationModuleLogicHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/navigation/NavigationModuleLogicHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/navigation/NavigationPage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/navigation/NavigationPage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/navigation/NavigationUiCommands.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/navigation/NavigationModulePack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/navigation/NavigationModulePack.h)

set(VTKQTCORE_MODULE_RECONSTRUCTION_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/reconstruction/ReconstructionModuleLogicHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/reconstruction/ReconstructionModuleLogicHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/reconstruction/ReconstructionPage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/reconstruction/ReconstructionPage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/reconstruction/ReconstructionUiCommands.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/reconstruction/ReconstructionModulePack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/reconstruction/ReconstructionModulePack.h)

set(VTKQTCORE_MODULE_KNEEPLANE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/kneePlane/kneePlaneLogicHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/kneePlane/kneePlaneLogicHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/kneePlane/kneePlaneContants.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/kneePlane/KneePlaneModulePack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/kneePlane/KneePlaneModulePack.h)

function(add_vtkqtcore_module_library target_name)
    add_library(${target_name} STATIC ${ARGN})
    target_link_libraries(${target_name} PUBLIC vtkqtcore_core)
endfunction()

function(add_vtkqtcore_runtime_deploy target_name)
    if(WIN32)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target_name}>
                $<TARGET_FILE_DIR:${target_name}>
            COMMAND_EXPAND_LISTS)

        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                "$<IF:$<CONFIG:Debug>,--debug,--release>"
                --no-translations
                --dir "$<TARGET_FILE_DIR:${target_name}>"
                "$<TARGET_FILE:${target_name}>")

        if(MSVC AND NOT VTK_QT_SUPPORT_LOCATION_DEBUG)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${QT_CORE_LOCATION_RELEASE}"
                    "${QT_GUI_LOCATION_RELEASE}"
                    "${QT_WIDGETS_LOCATION_RELEASE}"
                    "${QT_OPENGL_LOCATION_RELEASE}"
                    "$<TARGET_FILE_DIR:${target_name}>"
                COMMAND_EXPAND_LISTS)
        endif()

        if(MSVC AND NOT VTK_QT_SUPPORT_LOCATION_DEBUG AND QT_VERSION_MAJOR EQUAL 6)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${QT_OPENGLWIDGETS_LOCATION_RELEASE}"
                    "$<TARGET_FILE_DIR:${target_name}>"
                COMMAND_EXPAND_LISTS)
        endif()
    endif()
endfunction()

function(add_vtkqtcore_product target_name)
    set(options)
    set(oneValueArgs MAIN_SOURCE PRODUCT_QRC OUTPUT_NAME DISPLAY_NAME VERSION COMPANY_NAME ICON_FILE)
    set(multiValueArgs MODULE_LIBS)
    cmake_parse_arguments(PRODUCT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(product_sources ${PRODUCT_MAIN_SOURCE} ${PRODUCT_PRODUCT_QRC})

    if(WIN32)
        set(VTKQTCORE_RC_ICON_BLOCK "")
        if(PRODUCT_ICON_FILE)
            set(VTKQTCORE_RC_ICON_BLOCK "IDI_APP_ICON ICON \"${PRODUCT_ICON_FILE}\"")
        endif()

        set(VTKQTCORE_RC_COMPANY_NAME "${PRODUCT_COMPANY_NAME}")
        set(VTKQTCORE_RC_FILE_DESCRIPTION "${PRODUCT_DISPLAY_NAME}")
        set(VTKQTCORE_RC_FILE_VERSION "1,0,0,0")
        set(VTKQTCORE_RC_FILE_VERSION_TEXT "${PRODUCT_VERSION}")
        set(VTKQTCORE_RC_INTERNAL_NAME "${PRODUCT_OUTPUT_NAME}")
        set(VTKQTCORE_RC_ORIGINAL_FILENAME "${PRODUCT_OUTPUT_NAME}.exe")
        set(VTKQTCORE_RC_PRODUCT_NAME "${PRODUCT_DISPLAY_NAME}")
        set(VTKQTCORE_RC_PRODUCT_VERSION "1,0,0,0")
        set(VTKQTCORE_RC_PRODUCT_VERSION_TEXT "${PRODUCT_VERSION}")
        set(product_rc "${CMAKE_CURRENT_BINARY_DIR}/${target_name}_version.rc")
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/windows-product.rc.in
            ${product_rc}
            @ONLY)
        list(APPEND product_sources ${product_rc})
    endif()

    add_executable(${target_name} ${product_sources})
    target_link_libraries(${target_name} PRIVATE vtkqtcore_core ${PRODUCT_MODULE_LIBS})

    set(product_runtime_dir "${CMAKE_BINARY_DIR}/products/${PRODUCT_OUTPUT_NAME}")
    if(PRODUCT_OUTPUT_NAME)
        set_target_properties(${target_name} PROPERTIES
            OUTPUT_NAME ${PRODUCT_OUTPUT_NAME}
            RUNTIME_OUTPUT_DIRECTORY ${product_runtime_dir}
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${product_runtime_dir}/Debug
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${product_runtime_dir}/Release
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${product_runtime_dir}/RelWithDebInfo
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${product_runtime_dir}/MinSizeRel)
    endif()

    add_vtkqtcore_runtime_deploy(${target_name})

    vtk_module_autoinit(
        TARGETS ${target_name}
        MODULES ${VTK_LIBRARIES}
    )

    install(TARGETS ${target_name}
        RUNTIME DESTINATION ${PRODUCT_OUTPUT_NAME})

    add_custom_target(${target_name}_package
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/packages"
        COMMAND "${CMAKE_COMMAND}" -E tar cfv
            "${CMAKE_BINARY_DIR}/packages/${PRODUCT_OUTPUT_NAME}-${PRODUCT_VERSION}.zip"
            --format=zip
            "$<TARGET_FILE_DIR:${target_name}>"
        DEPENDS ${target_name})
endfunction()

add_vtkqtcore_module_library(vtkqtcore_module_datagen ${VTKQTCORE_MODULE_DATAGEN_SOURCES})
add_vtkqtcore_module_library(vtkqtcore_module_params ${VTKQTCORE_MODULE_PARAMS_SOURCES})
add_vtkqtcore_module_library(vtkqtcore_module_pointpick ${VTKQTCORE_MODULE_POINTPICK_SOURCES})
add_vtkqtcore_module_library(vtkqtcore_module_planning ${VTKQTCORE_MODULE_PLANNING_SOURCES})
add_vtkqtcore_module_library(vtkqtcore_module_navigation ${VTKQTCORE_MODULE_NAVIGATION_SOURCES})
add_vtkqtcore_module_library(vtkqtcore_module_reconstruction ${VTKQTCORE_MODULE_RECONSTRUCTION_SOURCES})
add_vtkqtcore_module_library(vtkqtcore_module_kneeplane ${VTKQTCORE_MODULE_KNEEPLANE_SOURCES})

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

    get_target_property(VTK_QT_SUPPORT_LOCATION_DEBUG VTK::GUISupportQt IMPORTED_LOCATION_DEBUG)
    get_target_property(QT_CORE_LOCATION_RELEASE Qt${QT_VERSION_MAJOR}::Core IMPORTED_LOCATION_RELEASE)
    get_target_property(QT_GUI_LOCATION_RELEASE Qt${QT_VERSION_MAJOR}::Gui IMPORTED_LOCATION_RELEASE)
    get_target_property(QT_WIDGETS_LOCATION_RELEASE Qt${QT_VERSION_MAJOR}::Widgets IMPORTED_LOCATION_RELEASE)
    get_target_property(QT_OPENGL_LOCATION_RELEASE Qt${QT_VERSION_MAJOR}::OpenGL IMPORTED_LOCATION_RELEASE)

    if(QT_VERSION_MAJOR EQUAL 6)
        get_target_property(QT_OPENGLWIDGETS_LOCATION_RELEASE Qt${QT_VERSION_MAJOR}::OpenGLWidgets IMPORTED_LOCATION_RELEASE)
    endif()
endif()

add_vtkqtcore_product(
    VTKQtCore
    MAIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    PRODUCT_QRC ${CMAKE_CURRENT_SOURCE_DIR}/src/products/default/product.qrc
    OUTPUT_NAME VTKQtCore
    DISPLAY_NAME "VTKQtCore"
    VERSION "1.0.0"
    COMPANY_NAME "VTKQtCore"
    ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/products/default/resources/app-icon.ico
    MODULE_LIBS
        vtkqtcore_module_datagen
        vtkqtcore_module_params
        vtkqtcore_module_pointpick
        vtkqtcore_module_planning
        vtkqtcore_module_navigation)

add_vtkqtcore_product(
    VTKQtCorePlanningNavigation
    MAIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/products/planning_navigation/main.cpp
    PRODUCT_QRC ${CMAKE_CURRENT_SOURCE_DIR}/src/products/planning_navigation/product.qrc
    OUTPUT_NAME VTKQtCorePlanningNavigation
    DISPLAY_NAME "VTKQtCore Planning Navigation"
    VERSION "1.0.0"
    COMPANY_NAME "VTKQtCore"
    ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/products/planning_navigation/resources/app-icon.ico
    MODULE_LIBS
        vtkqtcore_module_params
        vtkqtcore_module_planning
        vtkqtcore_module_navigation)

add_custom_target(package_all_products
    DEPENDS
        VTKQtCore_package
        VTKQtCorePlanningNavigation_package)
