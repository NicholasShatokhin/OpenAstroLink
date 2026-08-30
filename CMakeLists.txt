cmake_minimum_required(VERSION 3.20)
project(OpenAstroSuite VERSION 0.2.10.35 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Native OpenAstroLink is the reference hardware path. Compatibility backends
# remain available for existing devices/ecosystems.
option(OAS_ENABLE_QHY "Build native OAL QHYCCD driver (requires QHYCCD SDK)" OFF)
option(OAS_ENABLE_GPHOTO2 "Enable legacy in-core Canon/libgphoto2 compatibility backend" OFF)
option(OAS_ENABLE_NATIVE_CANON "Build native OAL Canon EOS driver" OFF)
set(OAS_CANON_TRANSPORT "AUTO" CACHE STRING "Native Canon transport: AUTO, EDSDK, or GPHOTO2")
set_property(CACHE OAS_CANON_TRANSPORT PROPERTY STRINGS AUTO EDSDK GPHOTO2)
set(CANON_EDSDK_ROOT "" CACHE PATH "Canon EDSDK root")
set(CANON_EDSDK_INCLUDE_DIR "" CACHE PATH "Directory containing EDSDK.h")
set(CANON_EDSDK_LIBRARY "" CACHE FILEPATH "Canon EDSDK import/shared library")
set(CANON_EDSDK_RUNTIME_DIR "" CACHE PATH "Directory containing EDSDK runtime DLLs (Windows build-tree staging + packaging)")
option(OAS_ENABLE_NATIVE_ZWO_ASI "Build native OAL ZWO ASI camera driver (requires ZWO ASI SDK)" OFF)
option(OAS_ENABLE_NATIVE_ZWO_EAF "Build native OAL ZWO EAF focuser driver (requires ZWO EAF SDK)" OFF)
option(OAS_ENABLE_INDI "Enable built-in INDI compatibility backends" ON)
option(OAS_ENABLE_NATIVE_GEMINI "Build native OAL Gemini EAF driver" ON)
option(OAS_ENABLE_NATIVE_SKYWATCHER "Build native OAL Sky-Watcher/SynScan mount driver" ON)
option(OAS_ENABLE_NATIVE_EQDRIVE "Build native OAL EQDrive ASTEP mount driver" ON)
option(OAS_ENABLE_ASCOM_CLASSIC "Build Windows Classic ASCOM compatibility helper/backend" ON)
option(OAS_BUILD_TESTS "Build lightweight tests" OFF)
option(OAS_BUILD_NATIVE_REFERENCE_DRIVERS "Build ABI-v2 native OAL reference simulator" ON)
option(OAS_BUILD_EXAMPLE_DRIVER "Build legacy ABI-v1 example plug-in" ON)
option(OAS_BUILD_GUI "Build the OpenAstroSuite Qt GUI client" ON)
option(OAS_BUILD_NODE "Build the headless openastrolink-node service" ON)
option(OAS_BUILD_HARDWARE_PROBE "Build oal-hardware-probe diagnostics utility" ON)
option(OAS_REQUIRE_MSVC "Fail configuration on Windows unless the MSVC toolchain is active" OFF)

if(WIN32 AND OAS_REQUIRE_MSVC AND NOT MSVC)
    message(FATAL_ERROR
        "This Windows preset requires the MSVC ABI, but CMake selected '${CMAKE_CXX_COMPILER_ID}' "
        "with compiler '${CMAKE_CXX_COMPILER}'. Do not mix MinGW/Strawberry GCC with "
        "Qt MSVC2022 or vendor .lib files. Delete the build directory and configure with "
        "a Ninja preset from an x64 MSVC developer environment (or use scripts/build_windows.ps1).")
endif()

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
    src/integrations/stellarium_telescope_server.h
    src/backends/oal_native_devices.h
    src/core/settings.cpp
    src/core/equatorial_frames.cpp
    src/core/mount_geometry.cpp
    src/core/equatorial_frames.h
    src/core/application_controller.cpp
    src/core/operation_manager.cpp
    src/backends/simulated_devices.cpp
    src/backends/opencv_camera.cpp
    src/backends/serial_lx200_mount.cpp
    src/backends/http_json_client.cpp
    src/backends/alpaca_devices.cpp
    src/backends/ascom_classic_mount.cpp
    src/backends/ascom_classic_mount.h
    src/backends/synscan_network_mount.cpp
    src/backends/synscan_network_mount.h
    src/backends/synscan_app_mount.cpp
    src/backends/synscan_app_mount.h
    src/backends/gemini_eaf_focuser.cpp
    src/backends/gemini_eaf_focuser.h
    src/backends/oal_client_devices.cpp
    src/backends/oal_native_devices.cpp
    src/algorithms/star_detector.cpp
    src/algorithms/star_catalog.cpp
    src/algorithms/pattern_plate_solver.cpp
    src/algorithms/astap_solver.cpp
    src/algorithms/astap_solver.h
    src/algorithms/adaptive_plate_solve.cpp
    src/algorithms/adaptive_plate_solve.h
    src/algorithms/motion_estimator.cpp
    src/algorithms/autofocus_engine.cpp
    src/algorithms/guiding_engine.cpp
    src/algorithms/polar_alignment.cpp
    src/algorithms/scheduler.cpp
    src/algorithms/neural_solver.cpp
    src/oal/oal_server.cpp
    src/oal/oal_ws_server.cpp
    src/oal/driver_plugin_loader.cpp
    src/integrations/stellarium_telescope_server.cpp
)

set(OAS_CANON_TRANSPORT_RESOLVED "")
if(OAS_ENABLE_NATIVE_CANON)
    string(TOUPPER "${OAS_CANON_TRANSPORT}" OAS_CANON_TRANSPORT_RESOLVED)
    if(OAS_CANON_TRANSPORT_RESOLVED STREQUAL "AUTO")
        if(WIN32)
            set(OAS_CANON_TRANSPORT_RESOLVED "EDSDK")
        else()
            set(OAS_CANON_TRANSPORT_RESOLVED "GPHOTO2")
        endif()
    endif()
    if(NOT OAS_CANON_TRANSPORT_RESOLVED MATCHES "^(EDSDK|GPHOTO2)$")
        message(FATAL_ERROR "OAS_CANON_TRANSPORT must be AUTO, EDSDK, or GPHOTO2")
    endif()
endif()

if(OAS_ENABLE_GPHOTO2 OR (OAS_ENABLE_NATIVE_CANON AND OAS_CANON_TRANSPORT_RESOLVED STREQUAL "GPHOTO2"))
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GPHOTO2 REQUIRED libgphoto2)
endif()

# Export selected build features for packaging/diagnostics.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/build_features.json.in
               ${CMAKE_BINARY_DIR}/build_features.json @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/build_features.json DESTINATION share/OpenAstroSuite)


if(OAS_ENABLE_GPHOTO2)
    list(APPEND OAS_CORE_SOURCES src/backends/canon_camera.cpp)
endif()

if(OAS_ENABLE_INDI)
    list(APPEND OAS_CORE_SOURCES src/backends/indi_devices.cpp)
endif()

add_library(oas_core STATIC ${OAS_CORE_SOURCES})
target_include_directories(oas_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/skywatcher
    ${OpenCV_INCLUDE_DIRS}
)
target_compile_definitions(oas_core PUBLIC
    OAS_VERSION="${PROJECT_VERSION}"
    $<$<BOOL:${OAS_ENABLE_GPHOTO2}>:OAS_HAVE_GPHOTO2>
    $<$<BOOL:${OAS_ENABLE_INDI}>:OAS_HAVE_INDI>
    $<$<AND:$<BOOL:${WIN32}>,$<BOOL:${OAS_ENABLE_ASCOM_CLASSIC}>>:OAS_HAVE_ASCOM_CLASSIC>
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
# The build-tree driver directory is a generated catalogue for the *current*
# configure.  Remove it on reconfigure so a driver disabled in CMake cannot
# survive as a stale DLL/manifest and be loaded by the next node run.
file(REMOVE_RECURSE "${OAL_DRIVER_BUILD_DIR}")
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

function(oal_is_microsoft_crt_dll dll_path out_var)
    get_filename_component(_oal_dll_name "${dll_path}" NAME)
    string(TOLOWER "${_oal_dll_name}" _oal_dll_name_lower)
    # Never app-local-stage Microsoft CRT/UCRT redistributables from vendor SDK
    # folders.  In particular, QHY Windows SDKs may ship msvcr90.dll/msvcp90.dll;
    # copying those beside an MSVC 2022 executable can trigger R6034/SxS loader
    # failures.  Install the appropriate Microsoft redistributable instead.
    if(_oal_dll_name_lower MATCHES "^(msvcr|msvcp|vcruntime|ucrtbase|concrt|vcamp|atl|mfc|mfcm)[0-9_]*.*\\.dll$"
       OR _oal_dll_name_lower MATCHES "^(api-ms-win-|ext-ms-win-).+\\.dll$")
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(oal_stage_windows_runtime runtime_dir label)
    if(NOT WIN32 OR NOT runtime_dir OR NOT IS_DIRECTORY "${runtime_dir}")
        return()
    endif()
    file(GLOB _oal_runtime_candidates CONFIGURE_DEPENDS "${runtime_dir}/*.dll" "${runtime_dir}/*.DLL")
    if(NOT _oal_runtime_candidates)
        message(WARNING "${label}: runtime directory contains no DLL files: ${runtime_dir}")
        return()
    endif()

    set(_oal_runtime_dlls)
    set(_oal_skipped_crt)
    foreach(_oal_dll IN LISTS _oal_runtime_candidates)
        oal_is_microsoft_crt_dll("${_oal_dll}" _oal_is_crt)
        if(_oal_is_crt)
            list(APPEND _oal_skipped_crt "${_oal_dll}")
        else()
            list(APPEND _oal_runtime_dlls "${_oal_dll}")
        endif()
    endforeach()

    # Build-tree execution must work without manually editing PATH.  Stage only
    # vendor/device DLLs; Microsoft CRTs must come from the installed VC redist.
    if(_oal_runtime_dlls)
        file(COPY ${_oal_runtime_dlls} DESTINATION "${CMAKE_BINARY_DIR}")
        file(COPY ${_oal_runtime_dlls} DESTINATION "${OAL_DRIVER_BUILD_DIR}")
    endif()
    list(LENGTH _oal_runtime_dlls _oal_runtime_count)
    list(LENGTH _oal_skipped_crt _oal_skipped_count)
    message(STATUS "${label}: staged ${_oal_runtime_count} vendor DLL(s); skipped ${_oal_skipped_count} Microsoft CRT/UCRT DLL(s)")
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
    set(QHYCCD_RUNTIME_DIR "" CACHE PATH "QHYCCD runtime DLL/shared-library directory (Windows build-tree staging + packaging)")
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
    # Do not place QHYCCD_INCLUDE_DIR on the compiler's normal include path.
    # Recent QHY Windows SDK packages ship compatibility headers named stdint.h
    # and stdint_windows.h.  With MSVC, /I <QHY sdk>/include can make the STL's
    # <cstdint> pick the vendor header instead of the CRT header, producing a
    # cascade of int64_t/std::chrono errors.  Generate one absolute-path wrapper
    # for qhyccd.h instead; quoted includes inside qhyccd.h still resolve relative
    # to the SDK directory, but the SDK directory cannot shadow standard headers.
    file(TO_CMAKE_PATH "${QHYCCD_INCLUDE_DIR}/qhyccd.h" OAL_QHYCCD_HEADER)
    if(NOT EXISTS "${OAL_QHYCCD_HEADER}")
        message(FATAL_ERROR "QHYCCD_INCLUDE_DIR does not contain qhyccd.h: ${QHYCCD_INCLUDE_DIR}")
    endif()
    set(OAL_QHY_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/qhy")
    file(MAKE_DIRECTORY "${OAL_QHY_GENERATED_INCLUDE_DIR}")
    configure_file(cmake/qhyccd_sdk_include.h.in
                   "${OAL_QHY_GENERATED_INCLUDE_DIR}/qhyccd_sdk_include.h" @ONLY)

    add_library(oal_driver_qhy SHARED drivers/qhy/oal_driver_qhy.cpp)
    target_include_directories(oal_driver_qhy PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OAL_QHY_GENERATED_INCLUDE_DIR})
    if(MSVC)
        target_compile_definitions(oal_driver_qhy PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
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
    oal_stage_windows_runtime("${QHYCCD_RUNTIME_DIR}" "QHYCCD")
endif()

if(OAS_ENABLE_NATIVE_ZWO_ASI)
    set(ZWO_ASI_ROOT "" CACHE PATH "ZWO ASI SDK root")
    set(ZWO_ASI_INCLUDE_DIR "" CACHE PATH "Directory containing ASICamera2.h")
    set(ZWO_ASI_LIBRARY "" CACHE FILEPATH "Exact ZWO ASI SDK library path")
    set(ZWO_ASI_RUNTIME_DIR "" CACHE PATH "ZWO ASI runtime DLL/shared-library directory (Windows build-tree staging + packaging)")
    if(NOT ZWO_ASI_INCLUDE_DIR)
        find_path(ZWO_ASI_INCLUDE_DIR ASICamera2.h HINTS ${ZWO_ASI_ROOT} ENV ZWO_ASI_ROOT ENV ZWO_ASI_INCLUDE_DIR PATH_SUFFIXES include Include libasi REQUIRED)
    endif()
    if(NOT ZWO_ASI_LIBRARY)
        find_library(ZWO_ASI_LIBRARY NAMES ASICamera2 ASICamera2.bin HINTS ${ZWO_ASI_ROOT} ENV ZWO_ASI_ROOT PATH_SUFFIXES lib lib64 armv8 x64 REQUIRED)
    endif()
    add_library(oal_driver_zwo_asi SHARED drivers/zwo_asi/oal_driver_zwo_asi.cpp)
    target_include_directories(oal_driver_zwo_asi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${ZWO_ASI_INCLUDE_DIR})
    target_link_libraries(oal_driver_zwo_asi PRIVATE ${ZWO_ASI_LIBRARY})
    oal_set_driver_output(oal_driver_zwo_asi)
    configure_file(drivers/zwo_asi/oal_driver_zwo_asi.manifest.json ${OAL_DRIVER_BUILD_DIR}/oal_driver_zwo_asi.manifest.json COPYONLY)
    install(TARGETS oal_driver_zwo_asi RUNTIME DESTINATION lib/openastrolink/drivers LIBRARY DESTINATION lib/openastrolink/drivers ARCHIVE DESTINATION lib/openastrolink/drivers)
    install(FILES drivers/zwo_asi/oal_driver_zwo_asi.manifest.json DESTINATION lib/openastrolink/drivers)
    oal_stage_windows_runtime("${ZWO_ASI_RUNTIME_DIR}" "ZWO ASI")
endif()

if(OAS_ENABLE_NATIVE_ZWO_EAF)
    set(ZWO_EAF_ROOT "" CACHE PATH "ZWO EAF SDK root")
    set(ZWO_EAF_INCLUDE_DIR "" CACHE PATH "Directory containing EAF_focuser.h")
    set(ZWO_EAF_LIBRARY "" CACHE FILEPATH "Exact ZWO EAF SDK library path")
    set(ZWO_EAF_RUNTIME_DIR "" CACHE PATH "ZWO EAF runtime DLL/shared-library directory (Windows build-tree staging + packaging)")
    if(NOT ZWO_EAF_INCLUDE_DIR)
        find_path(ZWO_EAF_INCLUDE_DIR EAF_focuser.h HINTS ${ZWO_EAF_ROOT} ENV ZWO_EAF_ROOT ENV ZWO_EAF_INCLUDE_DIR PATH_SUFFIXES include Include libasi REQUIRED)
    endif()
    if(NOT ZWO_EAF_LIBRARY)
        find_library(ZWO_EAF_LIBRARY NAMES EAFFocuser EAFFocuser.bin HINTS ${ZWO_EAF_ROOT} ENV ZWO_EAF_ROOT PATH_SUFFIXES lib lib64 armv8 x64 REQUIRED)
    endif()
    add_library(oal_driver_zwo_eaf SHARED drivers/zwo_eaf/oal_driver_zwo_eaf.cpp)
    target_include_directories(oal_driver_zwo_eaf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${ZWO_EAF_INCLUDE_DIR})
    target_link_libraries(oal_driver_zwo_eaf PRIVATE ${ZWO_EAF_LIBRARY})
    oal_set_driver_output(oal_driver_zwo_eaf)
    configure_file(drivers/zwo_eaf/oal_driver_zwo_eaf.manifest.json ${OAL_DRIVER_BUILD_DIR}/oal_driver_zwo_eaf.manifest.json COPYONLY)
    install(TARGETS oal_driver_zwo_eaf RUNTIME DESTINATION lib/openastrolink/drivers LIBRARY DESTINATION lib/openastrolink/drivers ARCHIVE DESTINATION lib/openastrolink/drivers)
    install(FILES drivers/zwo_eaf/oal_driver_zwo_eaf.manifest.json DESTINATION lib/openastrolink/drivers)
    oal_stage_windows_runtime("${ZWO_EAF_RUNTIME_DIR}" "ZWO EAF")
endif()


if(OAS_ENABLE_NATIVE_CANON)
    if(OAS_CANON_TRANSPORT_RESOLVED STREQUAL "EDSDK")
        if(NOT CANON_EDSDK_INCLUDE_DIR)
            find_path(CANON_EDSDK_INCLUDE_DIR EDSDK.h
                HINTS ${CANON_EDSDK_ROOT} ENV CANON_EDSDK_ROOT ENV CANON_EDSDK_INCLUDE_DIR
                PATH_SUFFIXES Header include Include EDSDK/Header REQUIRED)
        endif()
        if(NOT CANON_EDSDK_LIBRARY)
            find_library(CANON_EDSDK_LIBRARY NAMES EDSDK edsdk
                HINTS ${CANON_EDSDK_ROOT} ENV CANON_EDSDK_ROOT
                PATH_SUFFIXES Library lib Lib x64 REQUIRED)
        endif()
        add_library(oal_driver_canon SHARED drivers/canon/oal_driver_canon_edsdk.cpp)
        target_include_directories(oal_driver_canon PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include ${CANON_EDSDK_INCLUDE_DIR})
        target_link_libraries(oal_driver_canon PRIVATE ${CANON_EDSDK_LIBRARY} Qt6::Core Qt6::Gui)
        target_compile_definitions(oal_driver_canon PRIVATE OAL_CANON_TRANSPORT_EDSDK=1)
    elseif(OAS_CANON_TRANSPORT_RESOLVED STREQUAL "GPHOTO2")
        find_package(JPEG REQUIRED)
        add_library(oal_driver_canon SHARED drivers/canon/oal_driver_canon.cpp)
        target_include_directories(oal_driver_canon PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include ${GPHOTO2_INCLUDE_DIRS})
        target_link_directories(oal_driver_canon PRIVATE ${GPHOTO2_LIBRARY_DIRS})
        target_compile_options(oal_driver_canon PRIVATE ${GPHOTO2_CFLAGS_OTHER})
        target_link_libraries(oal_driver_canon PRIVATE ${GPHOTO2_LIBRARIES} JPEG::JPEG)
        target_compile_definitions(oal_driver_canon PRIVATE OAL_CANON_TRANSPORT_GPHOTO2=1)
    endif()
    oal_set_driver_output(oal_driver_canon)
    configure_file(drivers/canon/oal_driver_canon.manifest.json
                   ${OAL_DRIVER_BUILD_DIR}/oal_driver_canon.manifest.json COPYONLY)
    install(TARGETS oal_driver_canon
        RUNTIME DESTINATION lib/openastrolink/drivers
        LIBRARY DESTINATION lib/openastrolink/drivers
        ARCHIVE DESTINATION lib/openastrolink/drivers)
    install(FILES drivers/canon/oal_driver_canon.manifest.json DESTINATION lib/openastrolink/drivers)
    if(OAS_CANON_TRANSPORT_RESOLVED STREQUAL "EDSDK")
        oal_stage_windows_runtime("${CANON_EDSDK_RUNTIME_DIR}" "Canon EDSDK")
    endif()
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

if(OAS_ENABLE_NATIVE_EQDRIVE)
    add_library(oal_driver_eqdrive SHARED
        drivers/eqdrive/oal_driver_eqdrive.cpp
        drivers/skywatcher/motor_controller_protocol.h)
    target_include_directories(oal_driver_eqdrive PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/drivers/eqdrive)
    target_link_libraries(oal_driver_eqdrive PRIVATE Qt6::Core Qt6::SerialPort)
    oal_set_driver_output(oal_driver_eqdrive)
    configure_file(drivers/eqdrive/oal_driver_eqdrive.manifest.json
                   ${OAL_DRIVER_BUILD_DIR}/oal_driver_eqdrive.manifest.json COPYONLY)
    install(TARGETS oal_driver_eqdrive RUNTIME DESTINATION lib/openastrolink/drivers LIBRARY DESTINATION lib/openastrolink/drivers ARCHIVE DESTINATION lib/openastrolink/drivers)
    install(FILES drivers/eqdrive/oal_driver_eqdrive.manifest.json DESTINATION lib/openastrolink/drivers)
endif()

if(WIN32 AND OAS_ENABLE_ASCOM_CLASSIC)
    add_executable(oas-ascom-host tools/ascom_host/main.cpp)
    target_link_libraries(oas-ascom-host PRIVATE Qt6::Core ole32 oleaut32)
    set_target_properties(oas-ascom-host PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    if(TARGET OpenAstroSuite)
        add_dependencies(OpenAstroSuite oas-ascom-host)
    endif()
    if(TARGET openastrolink-node)
        add_dependencies(openastrolink-node oas-ascom-host)
    endif()
    install(TARGETS oas-ascom-host RUNTIME DESTINATION bin)
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
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/skywatcher
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/eqdrive)
    target_link_libraries(oal-native-protocol-smoke PRIVATE Qt6::Core)
    add_test(NAME oal-native-protocol-smoke COMMAND oal-native-protocol-smoke)
    add_executable(oal-adaptive-plate-solve-smoke tests/adaptive_plate_solve_smoke.cpp)
    target_link_libraries(oal-adaptive-plate-solve-smoke PRIVATE oas_core)
    add_test(NAME oal-adaptive-plate-solve-smoke COMMAND oal-adaptive-plate-solve-smoke)
    add_executable(oal-equatorial-frames-smoke tests/equatorial_frames_smoke.cpp)
    target_link_libraries(oal-equatorial-frames-smoke PRIVATE oas_core)
    add_test(NAME oal-equatorial-frames-smoke COMMAND oal-equatorial-frames-smoke)
    add_executable(oal-mount-geometry-smoke tests/mount_geometry_smoke.cpp)
    target_link_libraries(oal-mount-geometry-smoke PRIVATE oas_core)
    add_test(NAME oal-mount-geometry-smoke COMMAND oal-mount-geometry-smoke)
endif()
