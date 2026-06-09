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
    include/import_tool/connection_config.h
    include/import_tool/schema_export_dialog.h
    include/import_tool/schema_backup_dialog.h
)

target_include_directories(devops_db_tool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(devops_db_tool PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Sql
)
