cmake_minimum_required(VERSION 3.21.1)

option(LINK_INSIGHT "Link Qt Insight Tracker library" ON)
option(BUILD_QDS_COMPONENTS "Build design studio components" ON)

project(ScriptExecutorApp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Qml Quick)

if (WIN32)
    find_package(libssh CONFIG REQUIRED)
    if (TARGET ssh)
        set(SCRIPT_EXECUTOR_LIBSSH_TARGET ssh)
    elseif (TARGET libssh::ssh)
        set(SCRIPT_EXECUTOR_LIBSSH_TARGET libssh::ssh)
    else()
        message(FATAL_ERROR "libssh was found, but its CMake target is unknown")
    endif()
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSSH REQUIRED IMPORTED_TARGET libssh)
    set(SCRIPT_EXECUTOR_LIBSSH_TARGET PkgConfig::LIBSSH)
endif()

if (Qt6_VERSION VERSION_GREATER_EQUAL 6.3)
    qt_standard_project_setup()
endif()

qt_add_executable(ScriptExecutorApp

src/main.cpp

src/App/AppController.cpp

src/App/AppController.hpp

src/LoginService/LoginService.hpp

src/LoginService/LoginService.cpp
src/LoginService/SshWorker.hpp
src/LoginService/SshWorker.cpp

)

qt_add_resources(ScriptExecutorApp "configuration"
    PREFIX "/"
    FILES
        qtquickcontrols2.conf
)

target_link_libraries(ScriptExecutorApp PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)

target_compile_definitions(ScriptExecutorApp PRIVATE
    SCRIPT_EXECUTOR_APP_VERSION=\"${PROJECT_VERSION}\"
)

target_link_libraries(ScriptExecutorApp PRIVATE
    ${SCRIPT_EXECUTOR_LIBSSH_TARGET}
)

set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)
set(QML_IMPORT_PATH ${QT_QML_OUTPUT_DIRECTORY}
    CACHE STRING "Import paths for Qt Creator's code model"
    FORCE
)

if (BUILD_QDS_COMPONENTS)
    include(${CMAKE_CURRENT_SOURCE_DIR}/qmlcomponents)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/qmlmodules)

if (LINK_INSIGHT)
    include(${CMAKE_CURRENT_SOURCE_DIR}/insight)
endif ()

include(GNUInstallDirs)
install(TARGETS ScriptExecutorApp
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if (WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt windeployqt6
        HINTS "${Qt6_DIR}/../../../bin"
        REQUIRED
    )

    set(WINDOWS_PACKAGE_DIR "${CMAKE_BINARY_DIR}/windows-package")
    set(WINDOWS_PACKAGE_RUNTIME_COMMANDS)
    if (DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        set(VCPKG_RUNTIME_DIR
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
        if (EXISTS "${VCPKG_RUNTIME_DIR}")
            list(APPEND WINDOWS_PACKAGE_RUNTIME_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${VCPKG_RUNTIME_DIR}" "${WINDOWS_PACKAGE_DIR}")
        endif()
    endif()

    add_custom_target(package_windows
        COMMAND ${CMAKE_COMMAND} -E make_directory "${WINDOWS_PACKAGE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:ScriptExecutorApp>" "${WINDOWS_PACKAGE_DIR}"
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --release
            --compiler-runtime
            --qmldir "${CMAKE_CURRENT_SOURCE_DIR}"
            --dir "${WINDOWS_PACKAGE_DIR}"
            "$<TARGET_FILE:ScriptExecutorApp>"
        ${WINDOWS_PACKAGE_RUNTIME_COMMANDS}
        COMMENT "Deploying Qt and QML runtime files for Windows"
        DEPENDS ScriptExecutorApp
        VERBATIM
    )
endif()

# make IDEs aware of the QML import path
set(QML_IMPORT_PATH ${PROJECT_BINARY_DIR}/qml CACHE PATH
    "Path to the custom QML components defined by the project")
