#pragma once

#include <cstdint>
#include <cstddef>

// Stable C ABI for future native OpenAstroLink plug-ins. The host and driver
// exchange JSON messages so ABI compatibility does not depend on C++ STL/Qt.
#if defined(_WIN32)
#  define OAL_DRIVER_EXPORT __declspec(dllexport)
#else
#  define OAL_DRIVER_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {

struct OalDriverHostV1 {
    std::uint32_t abiVersion;
    void (*log)(int level, const char *messageUtf8);
    void *(*allocate)(std::size_t bytes);
    void (*deallocate)(void *ptr);
};

struct OalDriverV1 {
    std::uint32_t abiVersion;
    const char *driverId;
    const char *driverName;
    const char *driverVersion;

    void *context;
    bool (*start)(void *context, const char *configJsonUtf8);
    void (*stop)(void *context);
    const char *(*enumerateDevicesJson)(void *context);
    const char *(*invokeJson)(void *context, const char *deviceIdUtf8,
                              const char *methodUtf8, const char *requestJsonUtf8);
    void (*releaseString)(void *context, const char *value);
};

OAL_DRIVER_EXPORT const OalDriverV1 *oalCreateDriverV1(const OalDriverHostV1 *host);
}
