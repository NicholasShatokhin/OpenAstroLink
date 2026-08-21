cmake_minimum_required(VERSION 3.24)
project(OpenAstroSuite VERSION 0.2.7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Native OpenAstroLink is the reference hardware path. Compatibility backends
# remain available for existing devices/ecosystems.
option(OAS_ENABLE_QHY "Build native OAL QHYCCD driver (requires QHYCCD SDK)" OFF)
option(OAS_ENABLE_GPHOTO2 "Enable Canon/libgphoto2 compatibility backend" OFF)
option(OAS_ENABLE_INDI "Enable built-in INDI compatibility backends" ON)
option(OAS_ENABLE_NATIVE_GEMINI "Build native OAL Gemini EAF driver" ON)
option(OAS_ENABLE_NATIVE_SKYWATCHER "Build native OAL Sky-Watcher/SynScan mount driver" ON)
option(OAS_BUILD_TESTS "Build lightweight tests" OFF)
option(OAS_BUILD_NATIVE_REFERENCE_DRIVERS "Build ABI-v2 native OAL reference simulator" ON)
option(OAS_BUILD_EXAMPLE_DRIVER "Build legacy ABI-v1 example plug-in" ON)
option(OAS_BUILD_GUI "Build the OpenAstroSuite Qt GUI client" ON)
option(OAS_BUILD_NODE "Build the headless openastrolink-node service" ON)
option(OAS_BUILD_HARDWARE_PROBE "Build oal-hardware-probe diagnostics utility" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network SerialPort WebSockets HttpServer Concurrent)
if(OAS_BUILD_GUI)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)
endif()
find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc imgcodecs videoio calib3d)
find_package(Qt6 6.4 QUIET COMPONENTS Positioning)

set(OAS_CORE_SOURCES
    src/core/observatory_controller.h
    src/core/application_controller.h
    src/core/operation_manager.h
    src/algorithms/scheduler.h
    src/oal/oal_server.h
    src/oal/oal_ws_server.h
    src/oal/driver_plugin_loader.h
    src/backends/oal_native_devices.h
    src/core/settings.cpp
    src/core/application_controller.cpp
    src/core/operation_manager.cpp
    src/backends/simulated_devices.cpp
    src/backends/opencv_camera.cpp
    src/backends/serial_lx200_mount.cpp
    src/backends/http_json_client.cpp
    src/backends/alpaca_devices.cpp
    src/backends/gemini_eaf_focuser.cpp
    src/backends/gemini_eaf_focuser.h
    src/backends/oal_client_devices.cpp
    src/backends/oal_native_devices.cpp
    src/algorithms/star_detector.cpp
    src/algorithms/star_catalog.cpp
    src/algorithms/pattern_plate_solver.cpp
    src/algorithms/astap_solver.cpp
    src/algorithms/astap_solver.h
    src/algorithms/motion_estimator.cpp
    src/algorithms/autofocus_engine.cpp
    src/algorithms/guiding_engine.cpp
    src/algorithms/polar_alignment.cpp
    src/algorithms/scheduler.cpp
    src/algorithms/neural_solver.cpp
    src/oal/oal_server.cpp
    src/oal/oal_ws_server.cpp
    src/oal/driver_plugin_loader.cpp
)

if(OAS_ENABLE_GPHOTO2)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GPHOTO2 REQUIRED libgphoto2)
    list(APPEND OAS_CORE_SOURCES src/backends/canon_camera.cpp)
endif()

if(OAS_ENABLE_INDI)
    list(APPEND OAS_CORE_SOURCES src/backends/indi_devices.cpp)
endif()

add_library(oas_core STATIC ${OAS_CORE_SOURCES})
target_include_directories(oas_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)
target_compile_definitions(oas_core PUBLIC
    OAS_VERSION="${PROJECT_VERSION}"
    $<$<BOOL:${OAS_ENABLE_GPHOTO2}>:OAS_HAVE_GPHOTO2>
    $<$<BOOL:${OAS_ENABLE_INDI}>:OAS_HAVE_INDI>
)
target_link_libraries(oas_core PUBLIC
    Qt6::Core Qt6::Gui Qt6::Network Qt6::SerialPort Qt6::WebSockets Qt6::HttpServer Qt6::Concurrent
    ${OpenCV_LIBS}
)

if(OAS_ENABLE_GPHOTO2)
    target_include_directories(oas_core PRIVATE ${GPHOTO2_INCLUDE_DIRS})
    target_link_directories(oas_core PRIVATE ${GPHOTO2_LIBRARY_DIRS})
    target_link_libraries(oas_core PUBLIC ${GPHOTO2_LIBRARIES})
endif()
if(TARGET Qt6::Positioning)
    target_link_libraries(oas_core PUBLIC Qt6::Positioning)
    target_compile_definitions(oas_core PUBLIC OAS_HAVE_POSITIONING)
endif()

if(OAS_BUILD_GUI)
    add_executable(OpenAstroSuite
        src/app/main.cpp
        src/gui/main_window.h
        src/gui/main_window.cpp
        src/core/remote_observatory_controller.h
        src/core/remote_observatory_controller.cpp
    )
    target_link_libraries(OpenAstroSuite PRIVATE oas_core Qt6::Widgets)
    install(TARGETS OpenAstroSuite RUNTIME DESTINATION bin)
endif()

if(OAS_BUILD_NODE)
    add_executable(openastrolink-node src/node/main.cpp)
    target_link_libraries(openastrolink-node PRIVATE oas_core)
    install(TARGETS openastrolink-node RUNTIME DESTINATION bin)
endif()

if(OAS_BUILD_HARDWARE_PROBE)
    add_executable(oal-hardware-probe src/tools/hardware_probe.cpp)
    target_link_libraries(oal-hardware-probe PRIVATE oas_core)
    install(TARGETS oal-hardware-probe RUNTIME DESTINATION bin)
endif()

# ---------------------------------------------------------------------------
# Native OpenAstroLink drivers (ABI v2)
# ---------------------------------------------------------------------------
set(OAL_DRIVER_BUILD_DIR "${CMAKE_BINARY_DIR}/drivers")
file(MAKE_DIRECTORY "${OAL_DRIVER_BUILD_DIR}")

function(oal_set_driver_output target)
    set_target_properties(${target} PROPERTIES
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY "${OAL_DRIVER_BUILD_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${OAL_DRIVER_BUILD_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${OAL_DRIVER_BUILD_DIR}")
    foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(${target} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_${cfg} "${OAL_DRIVER_BUILD_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_${cfg} "${OAL_DRIVER_BUILD_DIR}"
            ARCHIVE_OUTPUT_DIRECTORY_${cfg} "${OAL_DRIVER_BUILD_DIR}")
    endforeach()
endfunction()

if(OAS_BUILD_NATIVE_REFERENCE_DRIVERS)
    add_library(oal_driver_simulated SHARED drivers/reference_simulated/oal_driver_simulated.cpp)
    target_include_directories(oal_driver_simulated PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    oal_set_driver_output(oal_driver_simulated)
    configure_file(drivers/reference_simulated/oal_driver_simulated.manifest.json
                   ${OAL_DRIVER_BUILD_DIR}/oal_driver_simulated.manifest.json COPYONLY)
    install(TARGETS oal_driver_simulated
        RUNTIME DESTINATION lib/openastrolink/drivers
        LIBRARY DESTINATION lib/openastrolink/drivers
        ARCHIVE DESTINATION lib/openastrolink/drivers)
    install(FILES drivers/reference_simulated/oal_driver_simulated.manifest.json
        DESTINATION lib/openastrolink/drivers)
endif()

if(OAS_ENABLE_QHY)
    set(QHYCCD_ROOT "" CACHE PATH "QHYCCD SDK root")
    set(QHYCCD_INCLUDE_DIR "" CACHE PATH "Directory containing qhyccd.h")
    set(QHYCCD_LIBRARY "" CACHE FILEPATH "Exact QHYCCD SDK library path (for example /opt/qhyccd/lib/libqhy.so)")
    if(NOT QHYCCD_INCLUDE_DIR)
        find_path(QHYCCD_INCLUDE_DIR qhyccd.h
            HINTS ${QHYCCD_ROOT} ENV QHYCCD_ROOT ENV QHYCCD_INCLUDE_DIR
            PATH_SUFFIXES include usr/local/include
            REQUIRED)
    endif()
    if(NOT QHYCCD_LIBRARY)
        find_library(QHYCCD_LIBRARY NAMES qhyccd qhy
            HINTS ${QHYCCD_ROOT} ENV QHYCCD_ROOT
            PATH_SUFFIXES lib lib64 usr/local/lib
            REQUIRED)
    endif()
    add_library(oal_driver_qhy SHARED drivers/qhy/oal_driver_qhy.cpp)
    target_include_directories(oal_driver_qhy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${QHYCCD_INCLUDE_DIR})
    target_link_libraries(oal_driver_qhy PRIVATE ${QHYCCD_LIBRARY})
    oal_set_driver_output(oal_driver_qhy)
    configure_file(drivers/qhy/oal_driver_qhy.manifest.json
                   ${OAL_DRIVER_BUILD_DIR}/oal_driver_qhy.manifest.json COPYONLY)
    install(TARGETS oal_driver_qhy
        RUNTIME DESTINATION lib/openastrolink/drivers
        LIBRARY DESTINATION lib/openastrolink/drivers
        ARCHIVE DESTINATION lib/openastrolink/drivers)
    install(FILES drivers/qhy/oal_driver_qhy.manifest.json
        DESTINATION lib/openastrolink/drivers)
endif()


if(OAS_ENABLE_NATIVE_GEMINI)
    add_library(oal_driver_gemini SHARED
        drivers/gemini/oal_driver_gemini.cpp
        drivers/gemini/myfocuserpro2_protocol.h)
    target_include_directories(oal_driver_gemini PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/drivers/gemini)
    target_link_libraries(oal_driver_gemini PRIVATE Qt6::Core Qt6::SerialPort)
    oal_set_driver_output(oal_driver_gemini)
    configure_file(drivers/gemini/oal_driver_gemini.manifest.json
                   ${OAL_DRIVER_BUILD_DIR}/oal_driver_gemini.manifest.json COPYONLY)
    install(TARGETS oal_driver_gemini RUNTIME DESTINATION lib/openastrolink/drivers LIBRARY DESTINATION lib/openastrolink/drivers ARCHIVE DESTINATION lib/openastrolink/drivers)
    install(FILES drivers/gemini/oal_driver_gemini.manifest.json DESTINATION lib/openastrolink/drivers)
endif()

if(OAS_ENABLE_NATIVE_SKYWATCHER)
    add_library(oal_driver_skywatcher SHARED
        drivers/skywatcher/oal_driver_skywatcher.cpp
        drivers/skywatcher/synscan_protocol.h
        drivers/skywatcher/motor_controller_protocol.h)
    target_include_directories(oal_driver_skywatcher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/drivers/skywatcher)
    target_link_libraries(oal_driver_skywatcher PRIVATE Qt6::Core Qt6::SerialPort)
    oal_set_driver_output(oal_driver_skywatcher)
    configure_file(drivers/skywatcher/oal_driver_skywatcher.manifest.json
                   ${OAL_DRIVER_BUILD_DIR}/oal_driver_skywatcher.manifest.json COPYONLY)
    install(TARGETS oal_driver_skywatcher RUNTIME DESTINATION lib/openastrolink/drivers LIBRARY DESTINATION lib/openastrolink/drivers ARCHIVE DESTINATION lib/openastrolink/drivers)
    install(FILES drivers/skywatcher/oal_driver_skywatcher.manifest.json DESTINATION lib/openastrolink/drivers)
endif()

# Legacy ABI-v1 example retained only as a migration/conformance fixture.
if(OAS_BUILD_EXAMPLE_DRIVER)
    add_library(oal_sim_driver SHARED examples/oal_sim_driver.cpp)
    target_include_directories(oal_sim_driver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set_target_properties(oal_sim_driver PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON PREFIX "" OUTPUT_NAME "oal_sim_driver")
    install(TARGETS oal_sim_driver RUNTIME DESTINATION plugins LIBRARY DESTINATION plugins ARCHIVE DESTINATION plugins)
endif()

install(DIRECTORY config docs schemas DESTINATION share/OpenAstroSuite)
install(FILES packaging/systemd/openastrolink-node.service.in packaging/systemd/openastrolink-node.env.example packaging/systemd/openastrolink-indi.service DESTINATION share/OpenAstroSuite/systemd)
install(FILES packaging/indi/indi-drivers.conf.example DESTINATION share/OpenAstroSuite/indi)
install(FILES packaging/linux/openastrosuite-local.desktop DESTINATION share/applications)

if(OAS_BUILD_TESTS)
    enable_testing()
    add_executable(oal-native-protocol-smoke tests/native_protocol_smoke.cpp)
    target_include_directories(oal-native-protocol-smoke PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/gemini
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/skywatcher)
    add_test(NAME oal-native-protocol-smoke COMMAND oal-native-protocol-smoke)
endif()
