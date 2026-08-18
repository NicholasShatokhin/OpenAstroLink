cmake_minimum_required(VERSION 3.24)
project(OpenAstroSuite VERSION 0.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(OAS_ENABLE_QHY "Enable QHYCCD SDK backend" OFF)
option(OAS_ENABLE_GPHOTO2 "Enable Canon/libgphoto2 backend" OFF)
option(OAS_ENABLE_INDI "Enable experimental built-in INDI XML client backends" OFF)
option(OAS_BUILD_TESTS "Build lightweight tests" OFF)
option(OAS_BUILD_EXAMPLE_DRIVER "Build the example native OAL plug-in" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core Gui Widgets Network SerialPort WebSockets HttpServer)
find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc imgcodecs videoio calib3d)
find_package(Qt6 6.4 QUIET COMPONENTS Positioning)

set(OAS_SOURCES
    src/core/application_controller.h
    src/algorithms/scheduler.h
    src/oal/oal_server.h
    src/oal/oal_ws_server.h
    src/oal/driver_plugin_loader.h
    src/gui/main_window.h
    src/app/main.cpp
    src/core/settings.cpp
    src/core/application_controller.cpp
    src/backends/simulated_devices.cpp
    src/backends/opencv_camera.cpp
    src/backends/serial_lx200_mount.cpp
    src/backends/http_json_client.cpp
    src/backends/alpaca_devices.cpp
    src/backends/gemini_eaf_focuser.cpp
    src/backends/gemini_eaf_focuser.h
    src/backends/oal_client_devices.cpp
    src/algorithms/star_detector.cpp
    src/algorithms/star_catalog.cpp
    src/algorithms/pattern_plate_solver.cpp
    src/algorithms/motion_estimator.cpp
    src/algorithms/autofocus_engine.cpp
    src/algorithms/guiding_engine.cpp
    src/algorithms/polar_alignment.cpp
    src/algorithms/scheduler.cpp
    src/algorithms/neural_solver.cpp
    src/oal/oal_server.cpp
    src/oal/oal_ws_server.cpp
    src/oal/driver_plugin_loader.cpp
    src/gui/main_window.cpp
)

if(OAS_ENABLE_QHY)
    find_path(QHYCCD_INCLUDE_DIR qhyccd.h REQUIRED)
    find_library(QHYCCD_LIBRARY NAMES qhyccd REQUIRED)
    list(APPEND OAS_SOURCES src/backends/qhy_camera.cpp)
endif()

if(OAS_ENABLE_GPHOTO2)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GPHOTO2 REQUIRED libgphoto2)
    list(APPEND OAS_SOURCES src/backends/canon_camera.cpp)
endif()

if(OAS_ENABLE_INDI)
    list(APPEND OAS_SOURCES src/backends/indi_devices.cpp)
endif()

add_executable(OpenAstroSuite ${OAS_SOURCES})

target_include_directories(OpenAstroSuite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_compile_definitions(OpenAstroSuite PRIVATE
    OAS_VERSION="${PROJECT_VERSION}"
    $<$<BOOL:${OAS_ENABLE_QHY}>:OAS_HAVE_QHY>
    $<$<BOOL:${OAS_ENABLE_GPHOTO2}>:OAS_HAVE_GPHOTO2>
    $<$<BOOL:${OAS_ENABLE_INDI}>:OAS_HAVE_INDI>
)

target_link_libraries(OpenAstroSuite PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::SerialPort
    Qt6::WebSockets Qt6::HttpServer
    ${OpenCV_LIBS}
)

if(OAS_ENABLE_QHY)
    target_include_directories(OpenAstroSuite PRIVATE ${QHYCCD_INCLUDE_DIR})
    target_link_libraries(OpenAstroSuite PRIVATE ${QHYCCD_LIBRARY})
endif()

if(OAS_ENABLE_GPHOTO2)
    target_include_directories(OpenAstroSuite PRIVATE ${GPHOTO2_INCLUDE_DIRS})
    target_link_directories(OpenAstroSuite PRIVATE ${GPHOTO2_LIBRARY_DIRS})
    target_link_libraries(OpenAstroSuite PRIVATE ${GPHOTO2_LIBRARIES})
endif()


if(TARGET Qt6::Positioning)
    target_link_libraries(OpenAstroSuite PRIVATE Qt6::Positioning)
    target_compile_definitions(OpenAstroSuite PRIVATE OAS_HAVE_POSITIONING)
endif()


if(OAS_BUILD_EXAMPLE_DRIVER)
    add_library(oal_sim_driver SHARED examples/oal_sim_driver.cpp)
    target_include_directories(oal_sim_driver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set_target_properties(oal_sim_driver PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        PREFIX ""
        OUTPUT_NAME "oal_sim_driver")
    install(TARGETS oal_sim_driver
        RUNTIME DESTINATION plugins
        LIBRARY DESTINATION plugins
        ARCHIVE DESTINATION plugins)
endif()

install(TARGETS OpenAstroSuite RUNTIME DESTINATION bin)
install(DIRECTORY config docs DESTINATION share/OpenAstroSuite)
