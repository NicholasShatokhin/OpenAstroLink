# Cross-compile Linux/ARMHF (32-bit Raspberry Pi OS) from Linux or WSL.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(OAS_CROSS_TRIPLE "arm-linux-gnueabihf" CACHE STRING "GNU target triple")
set(CMAKE_C_COMPILER "${OAS_CROSS_TRIPLE}-gcc" CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${OAS_CROSS_TRIPLE}-g++" CACHE FILEPATH "")

set(OAS_CROSS_SYSROOT "" CACHE PATH "Target Raspberry Pi sysroot containing ARMHF development files")
if(NOT OAS_CROSS_SYSROOT AND DEFINED ENV{OAS_CROSS_SYSROOT})
    set(OAS_CROSS_SYSROOT "$ENV{OAS_CROSS_SYSROOT}" CACHE PATH "Target Raspberry Pi sysroot" FORCE)
endif()
if(OAS_CROSS_SYSROOT)
    set(CMAKE_SYSROOT "${OAS_CROSS_SYSROOT}")
    list(PREPEND CMAKE_FIND_ROOT_PATH "${OAS_CROSS_SYSROOT}")

    # Debian/Raspberry Pi OS installs CMake package configs below the target
    # multiarch directory (for example
    #   /usr/lib/arm-linux-gnueabihf/cmake/Qt6/Qt6Config.cmake).
    # On some host CMake versions the implicit multiarch package search is not
    # seeded early enough during a foreign-sysroot configure.  Pin the target
    # architecture and seed the known package roots explicitly so find_package
    # never falls back to host Qt/OpenCV and never misses an otherwise valid
    # bootstrapped sysroot.
    set(CMAKE_LIBRARY_ARCHITECTURE "${OAS_CROSS_TRIPLE}" CACHE STRING
        "Target multiarch library directory" FORCE)
    list(PREPEND CMAKE_PREFIX_PATH "/usr" "/usr/local")

    set(_oas_qt6_candidates
        "${OAS_CROSS_SYSROOT}/usr/lib/${OAS_CROSS_TRIPLE}/cmake/Qt6/Qt6Config.cmake"
        "${OAS_CROSS_SYSROOT}/usr/lib/cmake/Qt6/Qt6Config.cmake")
    foreach(_oas_qt6_config IN LISTS _oas_qt6_candidates)
        if(EXISTS "${_oas_qt6_config}")
            get_filename_component(_oas_qt6_dir "${_oas_qt6_config}" DIRECTORY)
            set(Qt6_DIR "${_oas_qt6_dir}" CACHE PATH
                "Target Qt6 CMake package directory" FORCE)
            break()
        endif()
    endforeach()

    set(_oas_opencv_candidates
        "${OAS_CROSS_SYSROOT}/usr/lib/${OAS_CROSS_TRIPLE}/cmake/opencv4/OpenCVConfig.cmake"
        "${OAS_CROSS_SYSROOT}/usr/lib/cmake/opencv4/OpenCVConfig.cmake"
        "${OAS_CROSS_SYSROOT}/usr/share/OpenCV/OpenCVConfig.cmake")
    foreach(_oas_opencv_config IN LISTS _oas_opencv_candidates)
        if(EXISTS "${_oas_opencv_config}")
            get_filename_component(_oas_opencv_dir "${_oas_opencv_config}" DIRECTORY)
            set(OpenCV_DIR "${_oas_opencv_dir}" CACHE PATH
                "Target OpenCV CMake package directory" FORCE)
            break()
        endif()
    endforeach()
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
