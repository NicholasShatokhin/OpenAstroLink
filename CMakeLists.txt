cmake_minimum_required(VERSION 3.16)
project(AstroGui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets Gui Core REQUIRED)

add_executable(AstroGui
    main.cpp
    mainwindow.cpp
    mainwindow.h
    starview.cpp
    starview.h
    guiding.cpp
    guiding.h
    scheduler.cpp
    scheduler.h
    starcatalog.cpp
    starcatalog.h
    neuralsolver.cpp
    neuralsolver.h
    controlpanel.cpp
    controlpanel.h
)

target_link_libraries(AstroGui
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
)
