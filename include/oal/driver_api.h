#pragma once

#include <cstddef>
#include <cstdint>

// OpenAstroLink native driver ABI.
//
// ABI v1 is retained for source/binary compatibility with the early prototype.
// ABI v2 is the reference native driver boundary. It intentionally uses only
// C-compatible POD types and UTF-8 JSON at the control boundary; large frame
// payloads use publishFrame() and never travel as JSON/Base64.
#if defined(_WIN32)
#  define OAL_DRIVER_EXPORT __declspec(dllexport)
#else
#  define OAL_DRIVER_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {

// ---------------------------------------------------------------------------
// ABI v1 (legacy plug-in compatibility)
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// ABI v2 (native OpenAstroLink reference boundary)
// ---------------------------------------------------------------------------
static constexpr std::uint32_t OAL_DRIVER_ABI_V2 = 2;

enum OalDriverFeatureV2 : std::uint64_t {
    OAL_DRIVER_FEATURE_EVENTS       = 1ull << 0,
    OAL_DRIVER_FEATURE_FRAME_PUBLISH= 1ull << 1,
    OAL_DRIVER_FEATURE_CANCELLATION = 1ull << 2,
    OAL_DRIVER_FEATURE_HEALTH       = 1ull << 3,
    OAL_DRIVER_FEATURE_CONCURRENT   = 1ull << 4
};

enum OalPixelFormatV2 : std::uint32_t {
    OAL_PIXEL_UNKNOWN = 0,
    OAL_PIXEL_MONO8   = 1,
    OAL_PIXEL_MONO16  = 2,
    OAL_PIXEL_RGB8    = 3,
    OAL_PIXEL_RGB16   = 4,
    OAL_PIXEL_BAYER8  = 5,
    OAL_PIXEL_BAYER16 = 6
};

// The host copies data synchronously during publishFrame(). The driver may
// release/reuse the pointed memory as soon as publishFrame() returns.
struct OalFrameDescriptorV2 {
    std::uint32_t structSize;
    const char *frameIdUtf8;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t strideBytes;
    std::uint32_t pixelFormat;
    std::uint32_t bitsPerSample;
    std::uint32_t channels;
    std::int64_t capturedUnixNs;
    double exposureSec;
    double gain;
    const std::uint8_t *data;
    std::uint64_t dataBytes;
    const char *metadataJsonUtf8; // optional; copied during callback
};

struct OalDriverCallV2 {
    std::uint32_t structSize;
    const char *requestIdUtf8;       // optional correlation id
    const char *operationIdUtf8;     // optional OAL operation id
    std::uint64_t deadlineMonotonicNs; // 0 = no explicit deadline
};

struct OalDriverHostV2 {
    std::uint32_t abiVersion;
    std::uint32_t structSize;
    void *hostContext;

    void (*log)(void *hostContext, int level, const char *driverIdUtf8,
                const char *messageUtf8);
    void *(*allocate)(void *hostContext, std::size_t bytes);
    void (*deallocate)(void *hostContext, void *ptr);

    // Push event from driver to host. eventJsonUtf8 is copied before return.
    void (*emitEvent)(void *hostContext, const char *driverIdUtf8,
                      const char *deviceIdUtf8, const char *eventJsonUtf8);

    // Publish a science/raw frame out-of-band from JSON. Returns an opaque
    // host frame token (>0) that the driver may place in its invoke result.
    std::uint64_t (*publishFrame)(void *hostContext, const char *driverIdUtf8,
                                 const char *deviceIdUtf8,
                                 const OalFrameDescriptorV2 *frame);

    std::uint64_t (*monotonicTimeNs)(void *hostContext);
    bool (*isCancellationRequested)(void *hostContext, const char *operationIdUtf8);
};

struct OalDriverV2 {
    std::uint32_t abiVersion;
    std::uint32_t structSize;
    std::uint64_t featureBits;
    const char *driverId;
    const char *driverName;
    const char *driverVersion;

    void *context;

    // Driver-level manifest. The returned JSON is driver-owned until released.
    const char *(*manifestJson)(void *context);
    bool (*start)(void *context, const char *configJsonUtf8);
    void (*stop)(void *context);

    // enumerateDevicesJson returns an array of self-describing identity objects.
    const char *(*enumerateDevicesJson)(void *context);
    // capabilitiesJson returns a typed capability document for one device.
    const char *(*capabilitiesJson)(void *context, const char *deviceIdUtf8);
    // healthJson returns current driver/device health (optional).
    const char *(*healthJson)(void *context, const char *deviceIdUtf8);

    // Control calls are synchronous at the ABI boundary but are executed by the
    // OAL operation engine on worker threads for long operations. Drivers can
    // emit progress/events while this call is active and can be interrupted via
    // cancel() or device-specific abort methods.
    const char *(*invokeJson)(void *context, const char *deviceIdUtf8,
                              const char *methodUtf8, const char *requestJsonUtf8,
                              const OalDriverCallV2 *call);
    bool (*cancel)(void *context, const char *deviceIdUtf8,
                   const char *operationIdUtf8);
    void (*releaseString)(void *context, const char *value);
};

OAL_DRIVER_EXPORT const OalDriverV2 *oalCreateDriverV2(const OalDriverHostV2 *host);

} // extern "C"
