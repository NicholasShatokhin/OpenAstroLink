#include "algorithms/adaptive_plate_solve.h"

#include <QCoreApplication>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <iterator>
#include <vector>

namespace {
cv::Mat syntheticUrbanField(int dx, int dy) {
    constexpr int w = 640, h = 480;
    cv::Mat base(h, w, CV_32F);
    for (int y = 0; y < h; ++y) {
        float *row = base.ptr<float>(y);
        for (int x = 0; x < w; ++x)
            row[x] = 0.16f + 0.18f * float(x) / float(w) + 0.08f * float(y) / float(h);
    }
    const cv::Point stars[] = {
        {70,60},{130,85},{210,70},{315,95},{430,65},{545,105},
        {95,180},{180,155},{270,205},{365,165},{475,210},{570,170},
        {60,310},{155,280},{245,335},{340,290},{455,325},{555,300},
        {115,410},{225,390},{380,415},{515,400}
    };
    for (size_t i = 0; i < std::size(stars); ++i) {
        const float peak = 0.50f + 0.40f * float((i % 5) + 1) / 5.0f;
        cv::circle(base, stars[i], 2 + int(i % 2), cv::Scalar(peak), -1, cv::LINE_AA);
    }
    cv::GaussianBlur(base, base, cv::Size(), 0.8);
    cv::Mat shifted;
    cv::warpAffine(base, shifted, (cv::Mat_<double>(2,3) << 1,0,dx,0,1,dy), base.size(), cv::INTER_LINEAR, cv::BORDER_REFLECT);
    cv::Mat out; shifted.convertTo(out, CV_16U, 65535.0);
    return out;
}
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    std::vector<oas::CameraFrame> frames;
    for (int i = 0; i < 3; ++i) {
        oas::CameraFrame f;
        f.id = QString("synthetic-%1").arg(i);
        f.image = syntheticUrbanField(i * 5, -i * 3);
        f.capturedUtc = QDateTime::currentDateTimeUtc();
        f.exposureSec = 1.5;
        f.gain = 60;
        f.binX = f.binY = 2;
        frames.push_back(std::move(f));
    }

    oas::AdaptivePlateSolvePreprocessor pre;
    QString warning;
    const auto prepared = pre.prepare(frames, true, true, &warning);
    if (prepared.frame.image.empty()) {
        std::cerr << "prepared image is empty\n";
        return 1;
    }
    if (prepared.inputFrames != 3 || prepared.registeredFrames < 3) {
        std::cerr << "registration rejected synthetic drift: " << prepared.registeredFrames << "/3\n";
        return 2;
    }
    if (prepared.quality.detectedStars < 10) {
        std::cerr << "too few stars after urban preprocessing: " << prepared.quality.detectedStars << "\n";
        return 3;
    }
    if (prepared.frame.binX != 2 || prepared.frame.binY != 2) {
        std::cerr << "solver frame lost binning metadata\n";
        return 4;
    }
    std::cout << "adaptive plate solve smoke PASS: stars=" << prepared.quality.detectedStars
              << " registered=" << prepared.registeredFrames << "\n";
    return 0;
}
