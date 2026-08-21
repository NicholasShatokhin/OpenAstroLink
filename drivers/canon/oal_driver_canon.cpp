#include "oal/driver_api.h"
#include <cstdio>

extern "C" {
#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-context.h>
#include <gphoto2/gphoto2-list.h>
#include <gphoto2/gphoto2-widget.h>
#include <gphoto2/gphoto2-abilities-list.h>
#include <gphoto2/gphoto2-port-info-list.h>
#include <gphoto2/gphoto2-file.h>
#include <jpeglib.h>
}

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cctype>
#include <setjmp.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {
OalDriverHostV2 host{};

struct CameraState {
    std::string id;
    std::string model;
    std::string port;
    std::string serial;
    Camera *camera{nullptr};
    GPContext *context{nullptr};
    std::atomic_bool connected{false};
    std::atomic_bool abortRequested{false};
    std::atomic_bool bulbActive{false};
    std::uint32_t lastW{0}, lastH{0};
    std::mutex operationMutex;
};

struct DriverState {
    std::mutex mutex;
    std::unordered_map<std::string, std::unique_ptr<CameraState>> cameras;
    std::string spoolDir;
    bool deleteFromCamera{true};
} state;

std::string defaultSpoolDir() {
#ifdef _WIN32
    if (const char *p = std::getenv("USERPROFILE")) return (std::filesystem::path(p) / "Pictures" / "OpenAstroLink" / "Canon").string();
#endif
    if (const char *p = std::getenv("HOME")) return (std::filesystem::path(p) / "Pictures" / "OpenAstroLink" / "Canon").string();
    return (std::filesystem::temp_directory_path() / "OpenAstroLink" / "Canon").string();
}

char *copyString(const std::string &s) {
    auto *p = static_cast<char *>(host.allocate(host.hostContext, s.size() + 1));
    std::memcpy(p, s.c_str(), s.size() + 1);
    return p;
}
std::string quote(const std::string &s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        switch (c) {
        case '\\': o += "\\\\"; break;
        case '"': o += "\\\""; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default: if (c >= 0x20) o += char(c); break;
        }
    }
    return o + '"';
}
const char *ok(const std::string &data = "{}") { return copyString("{\"ok\":true,\"data\":" + data + "}"); }
const char *fail(const std::string &code, const std::string &msg) { return copyString("{\"ok\":false,\"error\":{\"code\":" + quote(code) + ",\"message\":" + quote(msg) + "}}"); }
void event(const std::string &device, const std::string &type, const std::string &payload = "{}") {
    if (!host.emitEvent) return;
    const auto e = "{\"type\":" + quote(type) + ",\"payload\":" + payload + "}";
    host.emitEvent(host.hostContext, "oal.canon", device.c_str(), e.c_str());
}
void log(int level, const std::string &m) { if (host.log) host.log(host.hostContext, level, "oal.canon", m.c_str()); }

double number(const std::string &json, const std::string &key, double fallback) {
    const auto p = json.find("\"" + key + "\""); if (p == std::string::npos) return fallback;
    const auto c = json.find(':', p); if (c == std::string::npos) return fallback;
    char *end = nullptr; const double v = std::strtod(json.c_str() + c + 1, &end);
    return end == json.c_str() + c + 1 ? fallback : v;
}
bool boolean(const std::string &json, const std::string &key, bool fallback) {
    const auto p = json.find("\"" + key + "\""); if (p == std::string::npos) return fallback;
    const auto c = json.find(':', p); if (c == std::string::npos) return fallback;
    const auto t = json.find_first_not_of(" \t\r\n", c + 1); if (t == std::string::npos) return fallback;
    if (json.compare(t, 4, "true") == 0) return true;
    if (json.compare(t, 5, "false") == 0) return false;
    return fallback;
}
std::string stringValue(const std::string &json, const std::string &key, const std::string &fallback = {}) {
    const auto p = json.find("\"" + key + "\""); if (p == std::string::npos) return fallback;
    const auto c = json.find(':', p); if (c == std::string::npos) return fallback;
    const auto q1 = json.find('"', c + 1); if (q1 == std::string::npos) return fallback;
    std::string out; bool esc = false;
    for (std::size_t i = q1 + 1; i < json.size(); ++i) {
        const char ch = json[i];
        if (esc) { out += ch; esc = false; continue; }
        if (ch == '\\') { esc = true; continue; }
        if (ch == '"') return out;
        out += ch;
    }
    return fallback;
}

std::string gpError(const char *what, int rc) { return std::string(what) + " failed: " + gp_result_as_string(rc) + " (" + std::to_string(rc) + ")"; }

bool configureCameraObject(CameraState &c, std::string &error) {
    c.context = gp_context_new();
    if (!c.context) { error = "gp_context_new failed"; return false; }
    int rc = gp_camera_new(&c.camera);
    if (rc < GP_OK || !c.camera) { error = gpError("gp_camera_new", rc); return false; }

    CameraAbilitiesList *al = nullptr;
    GPPortInfoList *pl = nullptr;
    CameraAbilities abilities{};
    GPPortInfo portInfo{};
    if ((rc = gp_abilities_list_new(&al)) < GP_OK || (rc = gp_abilities_list_load(al, c.context)) < GP_OK) {
        error = gpError("gp_abilities_list_load", rc); if (al) gp_abilities_list_free(al); return false;
    }
    const int ai = gp_abilities_list_lookup_model(al, c.model.c_str());
    if (ai < GP_OK || gp_abilities_list_get_abilities(al, ai, &abilities) < GP_OK || gp_camera_set_abilities(c.camera, abilities) < GP_OK) {
        error = "Could not select Canon camera model in libgphoto2 abilities list"; gp_abilities_list_free(al); return false;
    }
    gp_abilities_list_free(al);

    if ((rc = gp_port_info_list_new(&pl)) < GP_OK || (rc = gp_port_info_list_load(pl)) < GP_OK) {
        error = gpError("gp_port_info_list_load", rc); if (pl) gp_port_info_list_free(pl); return false;
    }
    const int pi = gp_port_info_list_lookup_path(pl, c.port.c_str());
    if (pi < GP_OK || gp_port_info_list_get_info(pl, pi, &portInfo) < GP_OK || gp_camera_set_port_info(c.camera, portInfo) < GP_OK) {
        error = "Could not select Canon USB/PTP port " + c.port; gp_port_info_list_free(pl); return false;
    }
    gp_port_info_list_free(pl);

    rc = gp_camera_init(c.camera, c.context);
    if (rc < GP_OK) { error = gpError("gp_camera_init", rc); return false; }
    return true;
}

void closeCamera(CameraState &c) {
    c.abortRequested = true;
    if (c.camera) { gp_camera_exit(c.camera, c.context); gp_camera_free(c.camera); c.camera = nullptr; }
    if (c.context) { gp_context_unref(c.context); c.context = nullptr; }
    c.connected = false; c.bulbActive = false;
}

bool getConfigRoot(CameraState &c, CameraWidget **root, std::string &error) {
    const int rc = gp_camera_get_config(c.camera, root, c.context);
    if (rc < GP_OK || !*root) { error = gpError("gp_camera_get_config", rc); return false; }
    return true;
}
CameraWidget *child(CameraWidget *root, const char *name) {
    CameraWidget *w = nullptr;
    if (gp_widget_get_child_by_name(root, name, &w) < GP_OK) gp_widget_get_child_by_label(root, name, &w);
    return w;
}

std::vector<std::string> choices(CameraState &c, const char *name) {
    std::vector<std::string> out; CameraWidget *root = nullptr; std::string e;
    if (!getConfigRoot(c, &root, e)) return out;
    if (auto *w = child(root, name)) {
        const int n = gp_widget_count_choices(w);
        for (int i = 0; i < n; ++i) { const char *v = nullptr; if (gp_widget_get_choice(w, i, &v) >= GP_OK && v) out.emplace_back(v); }
    }
    gp_widget_free(root); return out;
}

bool setChoice(CameraState &c, const char *name, const std::string &value, std::string &error) {
    CameraWidget *root = nullptr; if (!getConfigRoot(c, &root, error)) return false;
    CameraWidget *w = child(root, name); if (!w) { gp_widget_free(root); error = std::string("Camera config not found: ") + name; return false; }
    const char *p = value.c_str(); int rc = gp_widget_set_value(w, p);
    if (rc >= GP_OK) rc = gp_camera_set_config(c.camera, root, c.context);
    gp_widget_free(root);
    if (rc < GP_OK) { error = gpError((std::string("set ") + name).c_str(), rc); return false; }
    return true;
}
bool setToggle(CameraState &c, const char *name, int value, std::string &error) {
    CameraWidget *root = nullptr; if (!getConfigRoot(c, &root, error)) return false;
    CameraWidget *w = child(root, name); if (!w) { gp_widget_free(root); error = std::string("Camera config not found: ") + name; return false; }
    int v = value; int rc = gp_widget_set_value(w, &v);
    if (rc >= GP_OK) rc = gp_camera_set_config(c.camera, root, c.context);
    gp_widget_free(root);
    if (rc < GP_OK) { error = gpError((std::string("set ") + name).c_str(), rc); return false; }
    return true;
}
std::string readTextConfig(CameraState &c, const char *name) {
    CameraWidget *root = nullptr; std::string e; if (!getConfigRoot(c, &root, e)) return {};
    CameraWidget *w = child(root, name); const char *v = nullptr; std::string out;
    if (w && gp_widget_get_value(w, &v) >= GP_OK && v) out = v;
    gp_widget_free(root); return out;
}

std::string lower(std::string s) { for (char &c : s) c = char(std::tolower(static_cast<unsigned char>(c))); return s; }
double parseExposureChoice(std::string s) {
    s = lower(s); s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c){ return std::isspace(c) || c == '"'; }), s.end());
    if (s.find("bulb") != std::string::npos) return std::numeric_limits<double>::infinity();
    if (!s.empty() && s.back() == 's') s.pop_back();
    const auto slash = s.find('/');
    try {
        if (slash != std::string::npos) { const double a = std::stod(s.substr(0, slash)), b = std::stod(s.substr(slash + 1)); return b != 0 ? a / b : -1; }
        return std::stod(s);
    } catch (...) { return -1; }
}
std::string nearestExposureChoice(const std::vector<std::string> &v, double seconds) {
    std::string best; double bestErr = std::numeric_limits<double>::infinity();
    for (const auto &s : v) { const double x = parseExposureChoice(s); if (!(x > 0) || !std::isfinite(x)) continue; const double e = std::abs(std::log(x / std::max(1e-6, seconds))); if (e < bestErr) { bestErr = e; best = s; } }
    return best;
}
std::string bulbChoice(const std::vector<std::string> &v) { for (const auto &s : v) if (lower(s).find("bulb") != std::string::npos) return s; return {}; }
std::string preferredChoice(const std::vector<std::string> &values, std::initializer_list<const char *> preferred) {
    for (const char *want : preferred) {
        const std::string target = lower(want ? std::string(want) : std::string{});
        for (const auto &actual : values) if (lower(actual) == target) return actual;
    }
    return {};
}
std::string canonRemoteReleasePressChoice(const std::vector<std::string> &values) {
    // libgphoto2 has changed Canon EOS remote-release choice labels across camera
    // generations. Prefer actions that do not request autofocus for astronomy.
    return preferredChoice(values, {"Immediate", "Press Full MF", "Press Full", "Press 3", "Press 2"});
}
std::string canonRemoteReleaseReleaseChoice(const std::vector<std::string> &values) {
    return preferredChoice(values, {"Release", "Release Full", "Release 3", "Release 2"});
}
std::string nearestIsoChoice(const std::vector<std::string> &v, double iso) {
    std::string best; double bestErr = std::numeric_limits<double>::infinity();
    for (const auto &s : v) { try { const double x = std::stod(s); const double e = std::abs(x - iso); if (e < bestErr) { bestErr = e; best = s; } } catch (...) {} }
    return best;
}

bool setExposure(CameraState &c, double sec, bool &useBulb, std::string &applied, std::string &error) {
    const auto ss = choices(c, "shutterspeed");
    if (ss.empty()) { applied = "camera-current"; useBulb = false; return true; }
    const auto bulb = bulbChoice(ss);
    if (sec > 30.0 && !bulb.empty()) { if (!setChoice(c, "shutterspeed", bulb, error)) return false; useBulb = true; applied = bulb; return true; }
    const auto best = nearestExposureChoice(ss, sec);
    if (best.empty()) { applied = "camera-current"; useBulb = false; return true; }
    if (!setChoice(c, "shutterspeed", best, error)) return false;
    useBulb = false; applied = best; return true;
}
void setIsoBestEffort(CameraState &c, double iso, std::string &applied) {
    if (iso <= 0) return;
    const auto vals = choices(c, "iso"); const auto best = nearestIsoChoice(vals, iso); if (best.empty()) return;
    std::string e; if (setChoice(c, "iso", best, e)) applied = best;
}

bool waitForFile(CameraState &c, CameraFilePath &path, int timeoutMs, std::string &error) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        CameraEventType type = GP_EVENT_UNKNOWN; void *data = nullptr;
        const int rc = gp_camera_wait_for_event(c.camera, 500, &type, &data, c.context);
        if (rc < GP_OK) { error = gpError("gp_camera_wait_for_event", rc); if (data) std::free(data); return false; }
        if (type == GP_EVENT_FILE_ADDED && data) { path = *static_cast<CameraFilePath *>(data); std::free(data); return true; }
        if (data) std::free(data);
    }
    error = "Timed out waiting for Canon capture file"; return false;
}

bool bulbCapture(CameraState &c, double seconds, CameraFilePath &path, std::string &error) {
    bool releaseViaEos = false, releaseViaBulbToggle = false;
    std::string releaseChoice;
    std::string e;

    const auto remoteChoices = choices(c, "eosremoterelease");
    const auto pressChoice = canonRemoteReleasePressChoice(remoteChoices);
    releaseChoice = canonRemoteReleaseReleaseChoice(remoteChoices);
    if (!pressChoice.empty() && !releaseChoice.empty() && setChoice(c, "eosremoterelease", pressChoice, e)) {
        releaseViaEos = true;
        log(1, "Canon Bulb started through eosremoterelease=" + pressChoice + "; release=" + releaseChoice);
    } else if (setToggle(c, "bulb", 1, e)) {
        releaseViaBulbToggle = true;
        log(1, "Canon Bulb started through legacy bulb toggle");
    } else {
        error = "Camera exposes Bulb shutter speed but no compatible eosremoterelease/bulb control";
        return false;
    }

    c.bulbActive = true;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max<long long>(1, llround(seconds * 1000.0)));
    while (std::chrono::steady_clock::now() < deadline && !c.abortRequested) std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Always try to release the shutter even when cancellation was requested.
    // Failure to release is surfaced in the log because leaving Bulb active is a
    // hardware-safety problem; the caller will still receive CANCELLED below.
    if (releaseViaEos) {
        std::string re;
        if (!setChoice(c, "eosremoterelease", releaseChoice, re)) log(3, "Canon Bulb release warning: " + re);
    }
    if (releaseViaBulbToggle) {
        std::string re;
        if (!setToggle(c, "bulb", 0, re)) log(3, "Canon Bulb release warning: " + re);
    }
    c.bulbActive = false;
    if (c.abortRequested) { error = "Canon Bulb exposure cancelled"; return false; }
    return waitForFile(c, path, 15000, error);
}

bool fileBytes(CameraState &c, const CameraFilePath &path, CameraFileType type, std::vector<std::uint8_t> &out, std::string &error) {
    CameraFile *file = nullptr; int rc = gp_file_new(&file);
    if (rc < GP_OK || !file) { error = gpError("gp_file_new", rc); return false; }
    rc = gp_camera_file_get(c.camera, path.folder, path.name, type, file, c.context);
    if (rc < GP_OK) { gp_file_free(file); error = gpError("gp_camera_file_get", rc); return false; }
    const char *ptr = nullptr; unsigned long n = 0; rc = gp_file_get_data_and_size(file, &ptr, &n);
    if (rc < GP_OK || !ptr || !n) { gp_file_free(file); error = gpError("gp_file_get_data_and_size", rc); return false; }
    out.assign(reinterpret_cast<const std::uint8_t *>(ptr), reinterpret_cast<const std::uint8_t *>(ptr) + n);
    gp_file_free(file); return true;
}

struct JpegImage { std::uint32_t w{0}, h{0}; std::vector<std::uint8_t> rgb; };
struct JpegError { jpeg_error_mgr pub; jmp_buf jump; };
void jpegErrorExit(j_common_ptr cinfo) { auto *e = reinterpret_cast<JpegError *>(cinfo->err); longjmp(e->jump, 1); }
bool decodeJpeg(const std::vector<std::uint8_t> &bytes, JpegImage &out) {
    if (bytes.size() < 4 || bytes[0] != 0xff || bytes[1] != 0xd8) return false;
    jpeg_decompress_struct cinfo{}; JpegError jerr{}; cinfo.err = jpeg_std_error(&jerr.pub); jerr.pub.error_exit = jpegErrorExit;
    if (setjmp(jerr.jump)) { jpeg_destroy_decompress(&cinfo); return false; }
    jpeg_create_decompress(&cinfo); jpeg_mem_src(&cinfo, bytes.data(), bytes.size());
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) { jpeg_destroy_decompress(&cinfo); return false; }
    cinfo.out_color_space = JCS_RGB; jpeg_start_decompress(&cinfo); out.w = cinfo.output_width; out.h = cinfo.output_height;
    out.rgb.resize(std::size_t(out.w) * out.h * 3); while (cinfo.output_scanline < cinfo.output_height) { JSAMPROW row = out.rgb.data() + std::size_t(cinfo.output_scanline) * out.w * 3; jpeg_read_scanlines(&cinfo, &row, 1); }
    jpeg_finish_decompress(&cinfo); jpeg_destroy_decompress(&cinfo); return true;
}

std::string extensionOf(const std::string &name) { const auto p = name.find_last_of('.'); return p == std::string::npos ? std::string{} : name.substr(p); }
std::string storeOriginal(const CameraFilePath &path, const std::vector<std::uint8_t> &bytes, const std::string &requestedPath) {
    try {
        std::filesystem::path target;
        if (!requestedPath.empty()) {
            target = requestedPath;
            if (!target.has_extension()) target /= path.name;
        } else {
            std::filesystem::path dir(state.spoolDir); target = dir / path.name;
            if (std::filesystem::exists(target)) {
                const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                target = dir / (std::filesystem::path(path.name).stem().string() + "-" + std::to_string(ns) + extensionOf(path.name));
            }
        }
        if (target.has_parent_path()) std::filesystem::create_directories(target.parent_path());
        std::ofstream f(target, std::ios::binary); if (!f) return {};
        f.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size())); if (!f) return {};
        return target.string();
    } catch (...) { return {}; }
}

std::string readSerial(CameraState &c) {
    for (const char *name : {"serialnumber", "serial", "eosserialnumber"}) { const auto s = readTextConfig(c, name); if (!s.empty()) return s; }
    return {};
}
std::string makeId(const std::string &model, const std::string &port, const std::string &serial) {
    return "canon:" + (!serial.empty() ? serial : model + "@" + port);
}

CameraState *camera(const std::string &device) {
    std::lock_guard<std::mutex> lock(state.mutex); auto it = state.cameras.find(device); return it == state.cameras.end() ? nullptr : it->second.get();
}

bool probeDevice(const std::string &model, const std::string &port, std::string &serial, std::string &manufacturer) {
    CameraState tmp; tmp.model = model; tmp.port = port; std::string e;
    if (!configureCameraObject(tmp, e)) return false;
    serial = readSerial(tmp);
    for (const char *name : {"manufacturer", "cameramanufacturer"}) { manufacturer = readTextConfig(tmp, name); if (!manufacturer.empty()) break; }
    closeCamera(tmp); return true;
}

bool start(void *, const char *config) {
    state.spoolDir = defaultSpoolDir();
    if (config) {
        const std::string j(config); const auto s = stringValue(j, "spoolDir"); if (!s.empty()) state.spoolDir = s;
        state.deleteFromCamera = boolean(j, "deleteFromCamera", true);
    }
    return true;
}
void stop(void *) { std::lock_guard<std::mutex> lock(state.mutex); for (auto &x : state.cameras) { std::lock_guard<std::mutex> op(x.second->operationMutex); closeCamera(*x.second); } }
const char *manifest(void *) { return copyString(R"({"driverId":"oal.canon","name":"OpenAstroLink native Canon EOS driver","version":"0.2.10","abiVersion":2,"threadModel":"per-device-serial","transport":"USB/PTP via linked libgphoto2"})"); }

const char *devices(void *) {
    CameraList *list = nullptr; GPContext *ctx = gp_context_new(); if (!ctx || gp_list_new(&list) < GP_OK) { if (ctx) gp_context_unref(ctx); return copyString("[]"); }
    const int n = gp_camera_autodetect(list, ctx); std::ostringstream o; o << '['; bool first = true;
    if (n > 0) for (int i = 0; i < n; ++i) {
        const char *model = nullptr, *port = nullptr; if (gp_list_get_name(list, i, &model) < GP_OK || gp_list_get_value(list, i, &port) < GP_OK || !model || !port) continue;
        const std::string m(model), p(port); std::string serial, manufacturer; const bool probed = probeDevice(m, p, serial, manufacturer);
        const std::string identityText = lower(m + " " + manufacturer);
        if (identityText.find("canon") == std::string::npos && identityText.find("eos") == std::string::npos) continue;
        const std::string id = makeId(m, p, serial);
        {
            std::lock_guard<std::mutex> lock(state.mutex); auto &slot = state.cameras[id]; if (!slot) slot = std::make_unique<CameraState>(); slot->id = id; slot->model = m; slot->port = p; if (!serial.empty()) slot->serial = serial;
        }
        if (!first) o << ',';
        first = false;
        o << "{\"id\":" << quote(id) << ",\"type\":\"camera\",\"name\":" << quote(m + (serial.empty() ? "" : " [" + serial + "]"))
          << ",\"vendor\":\"Canon\",\"transport\":{\"kind\":\"usb-ptp\",\"library\":\"libgphoto2\",\"port\":" << quote(p) << ",\"identityProbe\":" << (probed ? "true" : "false") << "}}";
    }
    o << ']'; gp_list_free(list); gp_context_unref(ctx); return copyString(o.str());
}

std::string jsonChoices(const std::vector<std::string> &v) { std::ostringstream o; o << '['; for (std::size_t i = 0; i < v.size(); ++i) { if (i) o << ','; o << quote(v[i]); } o << ']'; return o.str(); }
const char *caps(void *, const char *device) {
    auto *c = camera(device ? device : ""); if (!c) return copyString("{}");
    std::vector<std::string> shutter, iso, formats; if (c->connected) { shutter = choices(*c, "shutterspeed"); iso = choices(*c, "iso"); formats = choices(*c, "imageformat"); }
    const bool bulb = !bulbChoice(shutter).empty(); std::ostringstream o;
    o << "{\"schemaVersion\":\"1.0\",\"identity\":{\"vendor\":\"Canon\",\"model\":" << quote(c->model) << ",\"serial\":" << quote(c->serial) << ",\"port\":" << quote(c->port) << "},\"camera\":{";
    o << "\"sensor\":{\"widthPx\":" << c->lastW << ",\"heightPx\":" << c->lastH << "},";
    o << "\"exposure\":{\"supported\":true,\"mode\":\"discrete-shutter-plus-bulb\",\"abortSupported\":true,\"abortMode\":\"cooperative-bulb-best-effort-short\",\"bulbSupported\":" << (bulb ? "true" : "false") << ",\"choices\":" << jsonChoices(shutter) << "},";
    o << "\"gain\":{\"supported\":true,\"semantic\":\"ISO\",\"choices\":" << jsonChoices(iso) << "},\"offset\":{\"supported\":false},\"roi\":{\"supported\":false},\"binning\":{\"supported\":false},";
    o << "\"imageFormat\":{\"choices\":" << jsonChoices(formats) << "},\"rawCapture\":{\"supported\":true,\"spoolOnSaveRaw\":true,\"spoolDir\":" << quote(state.spoolDir) << "},";
    o << "\"frameTransport\":[\"host-frame-v2\"],\"preview\":{\"source\":\"captured-file-or-embedded-preview\"},\"liveView\":{\"supported\":false,\"status\":\"planned\"}}}";
    return copyString(o.str());
}
const char *health(void *, const char *device) { auto *c = camera(device ? device : ""); if (!c) return copyString("{\"state\":\"missing\"}"); return copyString(std::string("{\"state\":\"") + (c->connected ? "ok" : "disconnected") + "\",\"connected\":" + (c->connected ? "true" : "false") + ",\"bulbActive\":" + (c->bulbActive ? "true" : "false") + "}"); }

const char *invoke(void *, const char *device, const char *method, const char *request, const OalDriverCallV2 *) {
    const std::string dev = device ? device : "", m = method ? method : "", r = request ? request : "{}"; auto *c = camera(dev); if (!c) return fail("DEVICE_NOT_FOUND", "Canon camera is not currently discovered");
    if (m == "device.connect") {
        std::lock_guard<std::mutex> op(c->operationMutex); if (c->connected) return ok(); std::string e; if (!configureCameraObject(*c, e)) { closeCamera(*c); return fail("OPEN_FAILED", e); }
        c->serial = readSerial(*c); std::string captureModeError; setToggle(*c, "capture", 1, captureModeError);
        c->abortRequested = false; c->connected = true; event(dev, "device.connected"); return ok("{\"model\":" + quote(c->model) + ",\"serial\":" + quote(c->serial) + ",\"port\":" + quote(c->port) + "}");
    }
    if (m == "device.disconnect") { c->abortRequested = true; std::lock_guard<std::mutex> op(c->operationMutex); closeCamera(*c); event(dev, "device.disconnected"); return ok(); }
    if (!c->connected || !c->camera) return fail("DEVICE_DISCONNECTED", "Canon EOS camera is not connected");
    if (m == "camera.abortExposure") { c->abortRequested = true; return ok(std::string("{\"bestEffort\":true,\"bulbActive\":") + (c->bulbActive ? "true" : "false") + "}"); }
    if (m == "camera.capture") {
        std::lock_guard<std::mutex> op(c->operationMutex); c->abortRequested = false;
        const int binX = std::max(1, int(number(r, "binX", 1))), binY = std::max(1, int(number(r, "binY", 1)));
        if (binX != 1 || binY != 1 || r.find("\"roi\"") != std::string::npos) return fail("CAPABILITY_NOT_SUPPORTED", "Canon EOS native driver does not support sensor binning/ROI");
        const double exposureSec = std::max(0.0001, number(r, "exposureSec", 1.0)); const double requestedIso = number(r, "gain", 0.0);
        bool bulb = false; std::string shutterApplied, e; if (!setExposure(*c, exposureSec, bulb, shutterApplied, e)) return fail("CAMERA_CONFIG_FAILED", e);
        std::string isoApplied; setIsoBestEffort(*c, requestedIso, isoApplied);
        CameraFilePath path{}; int rc = GP_OK;
        if (bulb) { if (!bulbCapture(*c, exposureSec, path, e)) return c->abortRequested ? fail("CANCELLED", e) : fail("CAPTURE_FAILED", e); }
        else { rc = gp_camera_capture(c->camera, GP_CAPTURE_IMAGE, &path, c->context); if (rc < GP_OK) return fail("CAPTURE_FAILED", gpError("gp_camera_capture", rc)); }
        if (c->abortRequested) return fail("CANCELLED", "Canon capture cancellation requested");

        std::vector<std::uint8_t> original; if (!fileBytes(*c, path, GP_FILE_TYPE_NORMAL, original, e)) return fail("DOWNLOAD_FAILED", e);
        const bool saveRaw = boolean(r, "saveRaw", false); const std::string savePath = stringValue(r, "savePath");
        const std::string ext = lower(extensionOf(path.name)); const bool rawLike = !(ext == ".jpg" || ext == ".jpeg" || ext == ".jpe");
        std::string sciencePath;
        if (saveRaw || rawLike || !savePath.empty()) sciencePath = storeOriginal(path, original, savePath);
        if (rawLike && sciencePath.empty()) {
            return fail("SCIENCE_FILE_SAVE_FAILED", "Canon RAW/original file could not be saved; camera copy was retained");
        }

        JpegImage jpg; bool previewDerived = false;
        if (!decodeJpeg(original, jpg)) { std::vector<std::uint8_t> preview; std::string pe; if (!fileBytes(*c, path, GP_FILE_TYPE_PREVIEW, preview, pe) || !decodeJpeg(preview, jpg)) { if (state.deleteFromCamera) gp_camera_file_delete(c->camera, path.folder, path.name, c->context); return fail("PREVIEW_UNAVAILABLE", "Captured Canon file was saved/downloaded but no decodable JPEG preview is available; enable JPEG/RAW+JPEG on the camera"); } previewDerived = true; }
        if (state.deleteFromCamera) gp_camera_file_delete(c->camera, path.folder, path.name, c->context);
        c->lastW = jpg.w; c->lastH = jpg.h;
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); const std::string frameId = "canon-" + (c->serial.empty() ? "eos" : c->serial) + "-" + std::to_string(ns);
        const std::string meta = "{\"vendor\":\"Canon\",\"model\":" + quote(c->model) + ",\"serial\":" + quote(c->serial) + ",\"originalFileName\":" + quote(path.name) + ",\"originalBytes\":" + std::to_string(original.size()) + ",\"scienceFilePath\":" + quote(sciencePath) + ",\"previewDerived\":" + (previewDerived ? "true" : "false") + ",\"shutterApplied\":" + quote(shutterApplied) + ",\"isoApplied\":" + quote(isoApplied) + "}";
        OalFrameDescriptorV2 f{}; f.structSize = sizeof(f); f.frameIdUtf8 = frameId.c_str(); f.width = jpg.w; f.height = jpg.h; f.strideBytes = jpg.w * 3; f.pixelFormat = OAL_PIXEL_RGB8; f.bitsPerSample = 8; f.channels = 3; f.capturedUnixNs = ns; f.exposureSec = exposureSec; f.gain = requestedIso; f.data = jpg.rgb.data(); f.dataBytes = jpg.rgb.size(); f.metadataJsonUtf8 = meta.c_str();
        const auto token = host.publishFrame ? host.publishFrame(host.hostContext, "oal.canon", dev.c_str(), &f) : 0; if (!token) return fail("FRAME_PUBLISH_FAILED", "OAL host rejected Canon preview frame");
        event(dev, "camera.frameReady", "{\"frameToken\":" + std::to_string(token) + ",\"scienceFilePath\":" + quote(sciencePath) + "}");
        return ok("{\"frameToken\":" + std::to_string(token) + ",\"frameId\":" + quote(frameId) + ",\"scienceFilePath\":" + quote(sciencePath) + ",\"originalFileName\":" + quote(path.name) + "}");
    }
    return fail("NOT_IMPLEMENTED", "Method is not implemented by native Canon EOS driver");
}

bool cancel(void *, const char *device, const char *) { auto *c = camera(device ? device : ""); if (!c) return false; c->abortRequested = true; return true; }
void releaseString(void *, const char *p) { if (p) host.deallocate(host.hostContext, const_cast<char *>(p)); }
OalDriverV2 api{OAL_DRIVER_ABI_V2, sizeof(OalDriverV2), OAL_DRIVER_FEATURE_EVENTS | OAL_DRIVER_FEATURE_FRAME_PUBLISH | OAL_DRIVER_FEATURE_CANCELLATION | OAL_DRIVER_FEATURE_HEALTH,
                "oal.canon", "OpenAstroLink native Canon EOS driver", "0.2.10", nullptr, &manifest, &start, &stop, &devices, &caps, &health, &invoke, &cancel, &releaseString};
} // namespace

extern "C" OAL_DRIVER_EXPORT const OalDriverV2 *oalCreateDriverV2(const OalDriverHostV2 *h) {
    if (!h || h->abiVersion != OAL_DRIVER_ABI_V2 || h->structSize < sizeof(OalDriverHostV2)) return nullptr;
    host = *h; return &api;
}
