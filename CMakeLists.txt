cmake_minimum_required(VERSION 3.16)
project(devops_db_tool VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets Sql)

add_executable(devops_db_tool
    app/main.cpp
    src/schema_export_dialog.cpp
    src/schema_backup_dialog.cpp
    src/schema_analysis_dialog.cpp
    src/about_widget.cpp
    include/import_tool/connection_config.h
    include/import_tool/schema_export_dialog.h
    include/import_tool/schema_backup_dialog.h
    include/import_tool/schema_analysis_dialog.h
    include/import_tool/about_widget.h
)

target_compile_definitions(devops_db_tool PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)

# ── RPATH (Linux/macOS): бинарник ищет .so в ../lib рядом с собой ────────────
if(NOT WIN32)
    set_target_properties(devops_db_tool PROPERTIES
        INSTALL_RPATH               "$ORIGIN/../lib"
        INSTALL_RPATH_USE_LINK_PATH TRUE
        BUILD_WITH_INSTALL_RPATH    FALSE
    )
endif()

# ── Установка ─────────────────────────────────────────────────────────────────
install(TARGETS devops_db_tool RUNTIME DESTINATION bin)

# Определяем пути Qt через qmake
if(TARGET Qt5::qmake)
    get_target_property(_qmake Qt5::qmake IMPORTED_LOCATION)
else()
    find_program(_qmake NAMES qmake5 qmake-qt5 qmake)
endif()

if(_qmake)
    execute_process(
        COMMAND "${_qmake}" -query QT_INSTALL_PLUGINS
        OUTPUT_VARIABLE QT_INSTALL_PLUGINS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "Qt plugins dir: ${QT_INSTALL_PLUGINS}")
else()
    set(QT_INSTALL_PLUGINS "")
    message(WARNING "qmake не найден — Qt-плагины не будут скопированы автоматически.")
endif()

# ── Развёртывание зависимостей (платформо-специфично) ─────────────────────────
if(WIN32)
    # Ищем windeployqt рядом с qmake
    if(_qmake)
        get_filename_component(_qt_bin_dir "${_qmake}" DIRECTORY)
    else()
        set(_qt_bin_dir "")
    endif()
    find_program(WINDEPLOYQT NAMES windeployqt HINTS "${_qt_bin_dir}")
    if(WINDEPLOYQT)
        message(STATUS "windeployqt: ${WINDEPLOYQT}")
    else()
        set(WINDEPLOYQT "")
        message(WARNING "windeployqt не найден — DLL будут собраны вручную.")
    endif()

    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/deploy_windows.cmake.in"
        "${CMAKE_BINARY_DIR}/deploy_windows.cmake"
        @ONLY
    )
    install(SCRIPT "${CMAKE_BINARY_DIR}/deploy_windows.cmake")

else()
    # Linux / macOS
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/deploy.cmake.in"
        "${CMAKE_BINARY_DIR}/deploy.cmake"
        @ONLY
    )
    install(SCRIPT "${CMAKE_BINARY_DIR}/deploy.cmake")
endif()

target_include_directories(devops_db_tool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(devops_db_tool PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Sql
)
