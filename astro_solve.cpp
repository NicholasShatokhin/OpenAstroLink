// astro_solve.cpp
// g++ -std=c++17 -O2 `pkg-config --cflags --libs opencv4 libgphoto2` astro_solve.cpp -lqhyccd -lgphoto2 -o astro_solve
//
// TODO: підключити SDK QHYCCD і замінити CaptureDevice на реальний.

#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <optional>
#include <string>
#include <sstream>
#include <thread>

#include <qhyccd.h>

#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-file.h>
#include <gphoto2/gphoto2-context.h>

#include <termios.h>

// Для ::open, ::read, ::write, ::close, O_RDWR, O_NOCTTY, O_SYNC і т.п.
#if !defined(_WIN32)
#  include <fcntl.h>
#  include <unistd.h>
#endif

// ===================== Параметри апаратури та спостерігача =====================
struct TelescopeParams {
    double focal_length_mm = 400.0;     // фокусна відстань
    double pixel_size_um   = 4.8;       // розмір пікселя
    int sensor_width_px    = 1920;
    int sensor_height_px   = 1080;
    // з цього можна порахувати масштаб в arcsec/pixel
    double arcsec_per_pixel() const {
        // scale["/px] = 206.265 * pixel_size[µm] / focal_length[mm]
        return 206.265 * (pixel_size_um / 1000.0) / (focal_length_mm / 1000.0);
    }
};

struct ObserverParams {
    double latitude_deg  = 50.4501; // Київ
    double longitude_deg = 30.5234;
    double elevation_m   = 150.0;
    // час беремо з системи або задаємо вручну
    std::tm utc{};
    bool has_manual_time = false;
};

// ===================== Утиліти =====================
static std::tm get_current_utc() {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t tt = system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &tt);
#else
    gmtime_r(&tt, &utc);
#endif
    return utc;
}

// можемо працювати або з QHYCCD, або з OpenCV, або з Canon
class CaptureDevice {
public:
    CaptureDevice() : use_qhy_(false), cam_(nullptr), imgW_(0), imgH_(0), bpp_(0), channels_(0) {}

    // --- Canon DSLR через libgphoto2 ---
    bool open_canon() {
        use_qhy_ = false;
        use_canon_ = true;

        GPContext *context = gp_context_new();
        if (gp_camera_new(&canon_cam_) != GP_OK) {
            std::cerr << "Canon: cannot allocate camera\n";
            return false;
        }
        int ret = gp_camera_init(canon_cam_, context);
        if (ret != GP_OK) {
            std::cerr << "Canon: init failed\n";
            gp_camera_free(canon_cam_);
            canon_cam_ = nullptr;
            return false;
        }

        canon_ctx_ = context;
        std::cout << "Canon DSLR connected\n";
        return true;
    }

    // Єдиний публічний grabFrame для всіх типів камер
    bool grabFrame(cv::Mat &frame) {
        if (use_canon_) {
            if (!canon_cam_) return false;
            CameraFile *file;
            CameraFilePath path;
            strcpy(path.folder, "/");
            strcpy(path.name, "capture.jpg");

            int ret = gp_camera_capture(canon_cam_, GP_CAPTURE_IMAGE, &path, canon_ctx_);
            if (ret != GP_OK) {
                std::cerr << "Canon: capture failed\n";
                return false;
            }

            ret = gp_file_new(&file);
            if (ret != GP_OK) return false;

            ret = gp_camera_file_get(canon_cam_, path.folder, path.name, GP_FILE_TYPE_NORMAL, file, canon_ctx_);
            if (ret != GP_OK) {
                std::cerr << "Canon: get file failed\n";
                gp_file_free(file);
                return false;
            }

            const char *data;
            unsigned long size;
            gp_file_get_data_and_size(file, &data, &size);

            std::vector<uchar> buf(data, data + size);
            frame = cv::imdecode(buf, cv::IMREAD_COLOR);

            gp_file_free(file);
            gp_camera_file_delete(canon_cam_, path.folder, path.name, canon_ctx_);

            return !frame.empty();
        }

        // решта варіантів нижче (opencv/qhy)
        if (!use_qhy_) return cap_.read(frame);
        else return grabFrameQHY(frame);
    }

    void close_canon() {
        if (canon_cam_) {
            gp_camera_exit(canon_cam_, canon_ctx_);
            gp_camera_free(canon_cam_);
            canon_cam_ = nullptr;
            gp_context_unref(canon_ctx_);
        }
    }

    // відкрити OpenCV-камеру
    bool open_opencv(int index = 0) {
        use_qhy_ = false;
        cap_.open(index);
        return cap_.isOpened();
    }

    // відкрити QHY (першу знайдену)
    bool open_qhy() {
        use_qhy_ = true;
        int ret = InitQHYCCDResource();
        if (ret != QHYCCD_SUCCESS) {
            std::cerr << "QHY: InitQHYCCDResource failed: " << ret << "\n";
            return false;
        }

        int camCount = ScanQHYCCD();
        if (camCount <= 0) {
            std::cerr << "QHY: no cameras found\n";
            return false;
        }

        char camId[32];
        ret = GetQHYCCDId(0, camId);
        if (ret != QHYCCD_SUCCESS) {
            std::cerr << "QHY: GetQHYCCDId failed: " << ret << "\n";
            return false;
        }

        cam_ = OpenQHYCCD(camId);
        if (cam_ == nullptr) {
            std::cerr << "QHY: OpenQHYCCD failed\n";
            return false;
        }

        // стрімовий режим
        ret = SetQHYCCDStreamMode(cam_, 1);
        if (ret != QHYCCD_SUCCESS) {
            std::cerr << "QHY: SetQHYCCDStreamMode failed: " << ret << "\n";
            return false;
        }

        // ініт камери
        ret = InitQHYCCD(cam_);
        if (ret != QHYCCD_SUCCESS) {
            std::cerr << "QHY: InitQHYCCD failed: " << ret << "\n";
            return false;
        }

        // можна тут виставити експозицію/GAIN, якщо потрібно
        // SetQHYCCDParam(cam_, CONTROL_EXPOSURE, 20000); // 20 ms
        // SetQHYCCDParam(cam_, CONTROL_GAIN, 10);

        // старт live
        ret = BeginQHYCCDLive(cam_);
        if (ret != QHYCCD_SUCCESS) {
            std::cerr << "QHY: BeginQHYCCDLive failed: " << ret << "\n";
            return false;
        }

        return true;
    }

    bool isOpened() const {
        if (use_qhy_) return cam_ != nullptr;
        return cap_.isOpened();
    }

    ~CaptureDevice() {
        if (use_qhy_ && cam_) {
            StopQHYCCDLive(cam_);
            CloseQHYCCD(cam_);
            ReleaseQHYCCDResource();
        }
    }

private:
    // QHY-гілка винесена в окрему функцію, щоб не дублювати код
    bool grabFrameQHY(cv::Mat &frame) {
        if (!use_qhy_) {
            return cap_.read(frame);
        } else {
            // QHY: читаємо один кадр
            int ret;
            unsigned char *imgData = nullptr;
            ret = GetQHYCCDLiveFrame(cam_, &imgW_, &imgH_, &bpp_, &channels_, nullptr);
            if (ret != QHYCCD_SUCCESS) {
                // друга форма з буфером
                // нам потрібен буфер, давай створимо статичний
            }
    
            // краще зробити буфер:
            static std::vector<unsigned char> buffer;
            buffer.resize(4096 * 4096 * 2); // з запасом
            ret = GetQHYCCDLiveFrame(cam_, &imgW_, &imgH_, &bpp_, &channels_, buffer.data());
            if (ret != QHYCCD_SUCCESS) {
                std::cerr << "QHY: GetQHYCCDLiveFrame failed: " << ret << "\n";
                return false;
            }
    
            // bpp може бути 8 або 16. channels, як правило, 1.
            if (bpp_ == 8) {
                frame = cv::Mat(imgH_, imgW_, CV_8UC1, buffer.data()).clone();
            } else if (bpp_ == 16) {
                // QHY дає 16-біт моно, перетворимо в 8-біт, щоб OpenCV показував
                cv::Mat tmp16(imgH_, imgW_, CV_16UC1, buffer.data());
                double minv, maxv;
                cv::minMaxLoc(tmp16, &minv, &maxv);
                cv::Mat tmp8;
                tmp16.convertTo(tmp8, CV_8UC1, 255.0 / (maxv - minv + 1e-6), -minv);
                frame = tmp8.clone();
            } else {
                std::cerr << "QHY: unsupported bpp=" << bpp_ << "\n";
                return false;
            }
            return true;
        }
    }

private:
    bool use_qhy_;
    bool use_canon_ = false;
    cv::VideoCapture cap_;
    qhyccd_handle *cam_;
    unsigned int imgW_, imgH_, bpp_, channels_;

    Camera *canon_cam_ = nullptr;
    GPContext *canon_ctx_ = nullptr;
};

// ===================== Підключення до монту SkyWatcher (LX200/SynScan) =====================
// Мінімальна ASCII-реалізація: :V#, :GR#, :GD#, :Sr hh:mm:ss#, :Sd sdd*mm#, :MS#
class MountController {
public:
    MountController() : fd_(-1), do_sync_(false) {}

    bool openPort(const std::string &port, int baud=9600) {
        fd_ = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
        if (fd_ < 0) {
            std::perror("open mount port");
            return false;
        }
        struct termios tty{};
        if (tcgetattr(fd_, &tty) != 0) {
            std::perror("tcgetattr");
            return false;
        }
        cfsetospeed(&tty, baud_to_flag(baud));
        cfsetispeed(&tty, baud_to_flag(baud));
        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
        tty.c_iflag &= ~IGNBRK;
        tty.c_lflag = 0;
        tty.c_oflag = 0;
        tty.c_cc[VMIN]  = 0;
        tty.c_cc[VTIME] = 10; // 1s
        tty.c_cflag |= (CLOCAL | CREAD);
        tty.c_cflag &= ~(PARENB | PARODD);
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;
        if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
            std::perror("tcsetattr");
            return false;
        }
        // перевіримо що живий
        std::string v = sendCommand(":V#");
        if (v.empty()) {
            std::cerr << "Mount: no response to :V#\n";
        } else {
            std::cout << "Mount version: " << v << "\n";
        }
        return true;
    }

    void enableSync(bool en) { do_sync_ = en; }

    // зчитати поточні RA/DEC
    bool getRADec(std::string &ra, std::string &dec) {
        ra  = sendCommand(":GR#");
        dec = sendCommand(":GD#");
        return !ra.empty() && !dec.empty();
    }

    // синхронізуватися на координати з plate solving
    // ra/dec у градусах → переганяємо у формат LX200
    bool syncTo(double ra_deg, double dec_deg) {
        // RA: 0..360 → години
        double ra_h = ra_deg / 15.0;
        int h = (int)ra_h;
        int m = (int)((ra_h - h) * 60.0);
        int s = (int)((((ra_h - h) * 60.0) - m) * 60.0);

        char ra_cmd[32];
        std::snprintf(ra_cmd, sizeof(ra_cmd), ":Sr %02d:%02d:%02d#", h, m, s);
        auto r1 = sendCommand(ra_cmd);

        // DEC: +DD*MM
        char sign = dec_deg >= 0 ? '+' : '-';
        double absd = std::fabs(dec_deg);
        int dd = (int)absd;
        int dm = (int)((absd - dd) * 60.0);
        char dec_cmd[32];
        std::snprintf(dec_cmd, sizeof(dec_cmd), ":Sd %c%02d*%02d#", sign, dd, dm);
        auto r2 = sendCommand(dec_cmd);

        auto r3 = sendCommand(":MS#"); // slew

        return !r1.empty() && !r2.empty() && !r3.empty();
    }

    // відносний рух по RA на deltaRaDeg (градуси)
    // реалізовано як: зчитати поточні RA/DEC, додати delta до RA і зробити slew
    bool slewRaOffset(double deltaRaDeg) {
        if (fd_ < 0) return false;
        std::string raStr, decStr;
        if (!getRADec(raStr, decStr)) return false;

        auto raDeg  = parseRaToDeg(raStr);
        auto decDeg = parseDecToDeg(decStr);
        if (!raDeg || !decDeg) return false;

        double newRa = *raDeg + deltaRaDeg;
        // нормалізуємо RA в [0,360)
        while (newRa < 0.0)   newRa += 360.0;
        while (newRa >= 360.) newRa -= 360.0;

        return syncTo(newRa, *decDeg);
    }

    bool isOpen() const { return fd_ >= 0; }

    ~MountController() {
        if (fd_ >= 0) ::close(fd_);
    }

    // утиліти для парсу RA/DEC
    static std::optional<double> parseRaToDeg(const std::string &ra);
    static std::optional<double> parseDecToDeg(const std::string &dec);

private:
    int fd_;
    bool do_sync_;

    speed_t baud_to_flag(int baud) {
        switch (baud) {
            case 115200: return B115200;
            case 57600: return B57600;
            case 38400: return B38400;
            case 19200: return B19200;
            default: return B9600;
        }
    }

    std::string sendCommand(const std::string &cmd) {
        if (fd_ < 0) return {};
        ssize_t w = ::write(fd_, cmd.c_str(), cmd.size());
        if (w < 0) {
            std::perror("write mount");
            return {};
        }
        char buf[128];
        ssize_t r = ::read(fd_, buf, sizeof(buf)-1);
        if (r <= 0) return {};
        buf[r] = 0;
        return std::string(buf);
    }
};

// RA у форматі "hh:mm:ss" -> градуси
std::optional<double> MountController::parseRaToDeg(const std::string &ra)
{
    int h=0,m=0,s=0;
    if (std::sscanf(ra.c_str(), "%d:%d:%d", &h, &m, &s) < 2) return std::nullopt;
    double hours = h + m/60.0 + s/3600.0;
    return hours * 15.0;
}

// DEC у форматі "+dd*mm" або "+dd*mm:ss" -> градуси
std::optional<double> MountController::parseDecToDeg(const std::string &dec)
{
    char signChar = '+';
    int d=0,m=0,s=0;
    if (std::sscanf(dec.c_str(), "%c%d*%d:%d", &signChar, &d, &m, &s) < 3) {
        if (std::sscanf(dec.c_str(), "%c%d*%d", &signChar, &d, &m) < 3)
            return std::nullopt;
        s = 0;
    }
    double sign = (signChar=='-') ? -1.0 : 1.0;
    double deg = d + m/60.0 + s/3600.0;
    return sign * deg;
}

// ===================== Структури для Polar Alignment (city mode) =====================
struct PaMeasurement {
    double mountRaDeg;
    double mountDecDeg;
    double solvedRaDeg;
    double solvedDecDeg;
};

struct PaResult {
    double meanRaOffsetDeg;   // solved - mount
    double meanDecOffsetDeg;  // solved - mount
    int    samples;
};

// груба оцінка: середній зсув між покажчиком монту і реальною позицією по всіx вимірах
static PaResult computePaResult(const std::vector<PaMeasurement> &ms)
{
    PaResult r{0,0,0};
    if (ms.empty()) return r;
    for (auto &m : ms) {
        r.meanRaOffsetDeg  += (m.solvedRaDeg  - m.mountRaDeg);
        r.meanDecOffsetDeg += (m.solvedDecDeg - m.mountDecDeg);
    }
    r.samples = (int)ms.size();
    r.meanRaOffsetDeg  /= r.samples;
    r.meanDecOffsetDeg /= r.samples;
    return r;
}

// ===================== Детектор зірок =====================
struct Star {
    cv::Point2d pos;    // координата в пікселях
    double flux;        // яскравість
};

class StarDetector {
public:
    std::vector<Star> detect(const cv::Mat &frame) {
        // Очікуємо монохром або конвертуємо
        cv::Mat gray;
        if (frame.channels() == 3)
            cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        else
            gray = frame;

        // легке розмиття щоб прибрати шум
        cv::Mat blurred;
        cv::GaussianBlur(gray, blurred, cv::Size(3,3), 0);

        // адаптивний поріг або простий поріг
        cv::Mat thresh;
        double maxVal;
        cv::minMaxLoc(blurred, nullptr, &maxVal);
        double t = maxVal * 0.6; // це можна зробити параметром
        cv::threshold(blurred, thresh, t, 255, cv::THRESH_BINARY);

        // знайдемо контури/блоби
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        std::vector<Star> stars;
        for (auto &c : contours) {
            cv::Moments m = cv::moments(c);
            if (m.m00 < 1.0) continue;
            double cx = m.m10 / m.m00;
            double cy = m.m01 / m.m00;
            // порахуємо сумарну яскравість всередині контуру
            double flux = m.m00;
            // можна відсікти занадто великі/занадто маленькі
            if (flux < 5) continue;
            stars.push_back({cv::Point2d(cx, cy), flux});
        }

        // відсортуємо по яскравості, щоб перші були найкращими
        std::sort(stars.begin(), stars.end(), [](auto &a, auto &b){
            return a.flux > b.flux;
        });

        return stars;
    }
};

// ===================== Дуже спрощений plate solver =====================
// Ідея: ми знаємо фокус, піксель, отже знаємо поле зору.
// Маємо час і місце → можемо приблизно сказати, де зеніт, де полюс.
// Але повноцінне розвʼязання "що це за поле" без каталогу ми не зробимо тут.
// Тому зробимо інтерфейс і фейкову реалізацію, яка просто повертає "я дивлюся туди-то".
struct SkySolution {
    // Пряме піднесення / схилення центра кадру
    double ra_deg  = 0.0;
    double dec_deg = 0.0;
    // обертання поля (position angle): кут від півночі до осі Y кадру
    double pa_deg  = 0.0;
    bool valid     = false;
};

class PlateSolver {
public:
    PlateSolver(const TelescopeParams &tp, const ObserverParams &op)
    : tp_(tp), op_(op) {}

    SkySolution solve(const std::vector<Star> &stars) {
        SkySolution sol;
        if (stars.size() < 3) {
            return sol; // недостатньо зірок
        }

        // TODO:
        // 1. завантажити каталог зірок, під який ми хочемо матчити.
        // 2. згенерувати інваріантні фігури (трикутники/квайди).
        // 3. знайти відповідність.
        //
        // Зараз: просто вкажемо, що рішення є, і поставимо центр = Полярна :)
        sol.ra_deg = 37.95;  // ~RA Полярної на 2025 близько 2h31m → 2.516h*15=37.7
        sol.dec_deg = 89.25; // ~DEC
        sol.pa_deg = 0.0;
        sol.valid = true;
        return sol;
    }

private:
    TelescopeParams tp_;
    ObserverParams op_;
};

// ===================== Оцінка руху між кадрами =====================
// Робимо простий матч: беремо перші N зірок з попереднього і поточного кадру, вважаємо що вони майже ті самі,
// і шукаємо перетворення "similarity" (scale+rotation+translation). Тут зробимо ще простіше: тільки rotation+translation.
struct FrameTransform {
    double dx = 0.0;   // зсув по x в пікселях (новий відносно старого)
    double dy = 0.0;
    double dtheta_deg = 0.0; // обертання кадру
    bool valid = false;
};

class MotionEstimator {
public:
    FrameTransform estimate(const std::vector<Star> &prev, const std::vector<Star> &curr) {
        FrameTransform ft;
        if (prev.empty() || curr.empty()) return ft;

        // візьмемо тільки перші k
        size_t k = std::min({prev.size(), curr.size(), size_t(15)});
        if (k < 3) return ft;

        // порахуємо середній центр
        cv::Point2d c_prev(0,0), c_curr(0,0);
        for (size_t i=0; i<k; ++i) {
            c_prev += prev[i].pos;
            c_curr += curr[i].pos;
        }
        c_prev *= (1.0 / k);
        c_curr *= (1.0 / k);

        // оцінка обертання: візьмемо одну яскраву зірку й подивимось кут до другої
        double angle_prev = 0.0, angle_curr = 0.0;
        {
            cv::Point2d v1 = prev[0].pos - c_prev;
            cv::Point2d v2 = prev[1].pos - c_prev;
            angle_prev = std::atan2(v2.y, v2.x) - std::atan2(v1.y, v1.x);

            cv::Point2d u1 = curr[0].pos - c_curr;
            cv::Point2d u2 = curr[1].pos - c_curr;
            angle_curr = std::atan2(u2.y, u2.x) - std::atan2(u1.y, u1.x);
        }

        double dtheta = angle_curr - angle_prev;
        // нормалізуємо кут
        while (dtheta >  M_PI) dtheta -= 2*M_PI;
        while (dtheta < -M_PI) dtheta += 2*M_PI;

        // зсув — просто різниця центрів (після врахування обертання можна точніше)
        ft.dx = c_curr.x - c_prev.x;
        ft.dy = c_curr.y - c_prev.y;
        ft.dtheta_deg = dtheta * 180.0 / M_PI;
        ft.valid = true;
        return ft;
    }
};

// ===================== Парсер аргументів =====================
struct Args {
    int cam_index = 0;
    double lat = NAN;
    double lon = NAN;
    bool override_loc = false;
    bool single_shot = false;
    bool use_qhy = false;
    bool use_canon = false;
    std::string mount_port;
    bool mount_sync = false;
    bool polar_align_city = false;
    int  pa_passes = 5;
    double pa_step_deg = 20.0;
};

Args parse_args(int argc, char **argv) {
    Args a;
    for (int i=1;i<argc;++i) {
        std::string s = argv[i];
        if (s == "--cam" && i+1<argc) {
            a.cam_index = std::stoi(argv[++i]);
        } else if (s == "--lat" && i+1<argc) {
            a.lat = std::stod(argv[++i]);
            a.override_loc = true;
        } else if (s == "--lon" && i+1<argc) {
            a.lon = std::stod(argv[++i]);
            a.override_loc = true;
        } else if (s == "--single") {
            a.single_shot = true;
        } else if (s == "--qhy") {
            a.use_qhy = true;
        } else if (s == "--pa-city") {
            a.polar_align_city = true;
        } else if (s == "--pa-passes" && i+1<argc) {
            a.pa_passes = std::max(3, std::stoi(argv[++i]));
        } else if (s == "--pa-step-deg" && i+1<argc) {
            a.pa_step_deg = std::stod(argv[++i]);
            if (a.pa_step_deg < 5.0) a.pa_step_deg = 5.0;
            if (a.pa_step_deg > 60.0) a.pa_step_deg = 60.0;
        } else if (s == "--canon") {
            a.use_canon = true;
        } else if (s.rfind("--mount-port=",0) == 0) {
            a.mount_port = s.substr(std::string("--mount-port=").size());
        } else if (s == "--mount-sync") {
            a.mount_sync = true;
        }
    }
    return a;
}

// ===================== Режим Polar Alignment (city) =====================
static bool runPolarAlignCity(CaptureDevice &cap,
                              StarDetector &detector,
                              PlateSolver &solver,
                              MountController &mount,
                              const TelescopeParams &telescope,
                              const ObserverParams &observer,
                              const Args &args)
{
    if (!mount.isOpen()) {
        std::cerr << "[PA] Mount is not connected, cannot run polar alignment.\n";
        return false;
    }

    std::cout << "[PA] City mode: passes=" << args.pa_passes
              << " RA step=" << args.pa_step_deg << " deg\n";

    std::vector<PaMeasurement> meas;

    for (int pass = 0; pass < args.pa_passes; ++pass) {
        std::cout << "[PA] Pass " << (pass+1) << "/" << args.pa_passes << "\n";

        // 1) кадр
        cv::Mat frame;
        if (!cap.grabFrame(frame)) {
            std::cerr << "[PA] No frame from camera\n";
            break;
        }

        auto stars = detector.detect(frame);
        std::cout << "[PA] Detected stars: " << stars.size() << "\n";
        if (stars.size() < 3) {
            std::cerr << "[PA] Too few stars for plate solving, try longer exposure.\n";
        }

        auto sol = solver.solve(stars);
        if (!sol.valid) {
            std::cerr << "[PA] Plate solution failed on this pass.\n";
        } else {
            std::cout << "[PA] Solved RA=" << sol.ra_deg << " DEC=" << sol.dec_deg << "\n";

            // RA/DEC з монту
            std::string mra, mdec;
            if (!mount.getRADec(mra, mdec)) {
                std::cerr << "[PA] Cannot get RA/DEC from mount.\n";
            } else {
                auto mraDeg  = MountController::parseRaToDeg(mra);
                auto mdecDeg = MountController::parseDecToDeg(mdec);
                if (!mraDeg || !mdecDeg) {
                    std::cerr << "[PA] Failed to parse mount RA/DEC: " << mra << " " << mdec << "\n";
                } else {
                    PaMeasurement m;
                    m.mountRaDeg   = *mraDeg;
                    m.mountDecDeg  = *mdecDeg;
                    m.solvedRaDeg  = sol.ra_deg;
                    m.solvedDecDeg = sol.dec_deg;
                    meas.push_back(m);

                    auto res = computePaResult(meas);
                    std::cout << "[PA] Samples=" << res.samples
                              << "  mean RA offset=" << res.meanRaOffsetDeg << " deg"
                              << "  mean DEC offset=" << res.meanDecOffsetDeg << " deg\n";
                }
            }
        }

        // 2) підготовка до наступного проходу: змістити RA
        if (pass < args.pa_passes - 1) {
            double sign = (pass % 2 == 0) ? +1.0 : -1.0; // чергуємо напрямок
            double step = sign * args.pa_step_deg;
            std::cout << "[PA] Slew RA by " << step << " deg...\n";
            if (!mount.slewRaOffset(step)) {
                std::cerr << "[PA] RA offset slew failed.\n";
                break;
            }
            // даємо монтуванню час доїхати
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    auto res = computePaResult(meas);
    if (res.samples >= 3) {
        std::cout << "[PA] Final city-mode PA estimate:\n"
                  << "      mean RA offset (solved - mount)  = " << res.meanRaOffsetDeg  << " deg\n"
                  << "      mean DEC offset (solved - mount) = " << res.meanDecOffsetDeg << " deg\n"
                  << "    (наближено: крутити полярну вісь так, щоб ці офсети прямували до 0)\n";
        return true;
    }
    std::cout << "[PA] Not enough valid samples for reliable result.\n";
    return false;
}

// ===================== main =====================
int main(int argc, char **argv) {
    Args args = parse_args(argc, argv);

    TelescopeParams telescope;
    ObserverParams observer;
    observer.utc = get_current_utc();
    if (args.override_loc) {
        observer.latitude_deg = args.lat;
        observer.longitude_deg = args.lon;
    }

    CaptureDevice cap;
    if (args.use_canon) {
        if (!cap.open_canon()) {
            std::cerr << "Cannot open Canon DSLR\n";
            return 1;
        }
    } else if (args.use_qhy) {
        if (!cap.open_qhy()) {
            std::cerr << "Cannot open QHY camera\n";
            return 1;
        }
    } else {
        if (!cap.open_opencv(args.cam_index)) {
            std::cerr << "Cannot open camera index " << args.cam_index << "\n";
            return 1;
        }
    }

    MountController mount;
    if (!args.mount_port.empty()) {
        if (mount.openPort(args.mount_port, 9600)) {
            std::cout << "Mount connected on " << args.mount_port << "\n";
            if (args.mount_sync) mount.enableSync(true);
        } else {
            std::cerr << "Cannot open mount on " << args.mount_port << "\n";
        }
    }

    StarDetector detector;
    PlateSolver solver(telescope, observer);
    MotionEstimator motion;

    std::vector<Star> prev_stars;

    std::cout << "Arcsec/px: " << telescope.arcsec_per_pixel() << "\n";

    // Якщо увімкнений режим міського Polar Alignment — запускаємо його і завершуємо
    if (args.polar_align_city) {
        MountController mount;
        if (!args.mount_port.empty()) {
            if (!mount.openPort(args.mount_port, 9600)) {
                std::cerr << "[PA] Cannot open mount on " << args.mount_port << "\n";
                return 1;
            }
        }
        bool ok = runPolarAlignCity(cap, detector, solver, mount, telescope, observer, args);
        return ok ? 0 : 2;
    }

    while (true) {
        cv::Mat frame;
        if (!cap.grabFrame(frame)) {
            std::cerr << "No frame\n";
            break;
        }

        auto stars = detector.detect(frame);
        std::cout << "Detected stars: " << stars.size() << "\n";

        auto sol = solver.solve(stars);
        if (sol.valid) {
            std::cout << "Solved sky: RA=" << sol.ra_deg << " deg, DEC=" << sol.dec_deg
                      << " deg, PA=" << sol.pa_deg << " deg\n";

            // якщо є монтування і нас попросили синхронізуватись — робимо
            if (mount.isOpen() && args.mount_sync) {
                if (mount.syncTo(sol.ra_deg, sol.dec_deg)) {
                    std::cout << "Mount synced to solved coords.\n";
                } else {
                    std::cout << "Mount sync failed.\n";
                }
            }

            // просто показати поточні координати монту
            if (mount.isOpen()) {
                std::string mra, mdec;
                if (mount.getRADec(mra, mdec)) {
                    std::cout << "Mount RA=" << mra << " DEC=" << mdec << "\n";
                }
            }
        } else {
            std::cout << "Sky solution not found.\n";
        }

        if (!prev_stars.empty()) {
            auto tf = motion.estimate(prev_stars, stars);
            if (tf.valid) {
                std::cout << "Frame motion: dx=" << tf.dx << " px, dy=" << tf.dy
                          << " px, dtheta=" << tf.dtheta_deg << " deg\n";
            }
        }
        prev_stars = stars;

        // покажемо детект
        for (auto &st : stars) {
            cv::circle(frame, st.pos, 4, cv::Scalar(0,255,0), 1);
        }
        cv::imshow("stars", frame);
        int key = cv::waitKey(1);
        if (key == 27) break; // ESC
        if (args.single_shot) break;
    }

    if (args.use_canon) cap.close_canon();

    return 0;
}
