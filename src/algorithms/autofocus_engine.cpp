#include "algorithms/autofocus_engine.h"
#include <QThread>
#include <QElapsedTimer>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

namespace oas {
static cv::Mat toGray8(const cv::Mat&s){cv::Mat g;if(s.channels()==3)cv::cvtColor(s,g,cv::COLOR_BGR2GRAY);else g=s;cv::Mat o;double minv,maxv;cv::minMaxLoc(g,&minv,&maxv);g.convertTo(o,CV_8U,255.0/std::max(1.0,maxv-minv),-minv*255.0/std::max(1.0,maxv-minv));return o;}
double AutofocusEngine::median(std::vector<double>v){if(v.empty())return 0;auto m=v.begin()+v.size()/2;std::nth_element(v.begin(),m,v.end());double x=*m;if(v.size()%2==0){auto m2=std::max_element(v.begin(),m);x=(x+*m2)/2;}return x;}
cv::Rect AutofocusEngine::planetRoi(const cv::Mat&im){cv::Mat g=toGray8(im),b;cv::GaussianBlur(g,b,{5,5},1.2);cv::Scalar m,s;cv::meanStdDev(b,m,s);cv::Mat mask;cv::threshold(b,mask,std::min(250.0,m[0]+2.0*s[0]),255,cv::THRESH_BINARY);std::vector<std::vector<cv::Point>> cs;cv::findContours(mask,cs,cv::RETR_EXTERNAL,cv::CHAIN_APPROX_SIMPLE);double ba=0;cv::Rect br;for(auto&c:cs){double a=cv::contourArea(c);if(a>ba){ba=a;br=cv::boundingRect(c);}}if(ba<10)return {im.cols/4,im.rows/4,im.cols/2,im.rows/2};int pad=std::max(br.width,br.height);br.x=std::max(0,br.x-pad);br.y=std::max(0,br.y-pad);br.width=std::min(im.cols-br.x,br.width+2*pad);br.height=std::min(im.rows-br.y,br.height+2*pad);return br;}
double AutofocusEngine::score(const cv::Mat&im,AutofocusMode mode,bool autoRoi){cv::Mat g=toGray8(im);cv::Rect roi(0,0,g.cols,g.rows);if(mode==AutofocusMode::Planet&&autoRoi)roi=planetRoi(g);cv::Mat c=g(roi);if(mode==AutofocusMode::Stars){StarDetector det;auto stars=det.detect(c);std::vector<double> q;for(const auto&s:stars)if(s.hfrPx>0.1)q.push_back(s.flux/(s.hfrPx*s.hfrPx));return median(q);}cv::Mat gx,gy;cv::Sobel(c,gx,CV_32F,1,0,3);cv::Sobel(c,gy,CV_32F,0,1,3);cv::Mat mag;cv::magnitude(gx,gy,mag);cv::Scalar mean,stddev;cv::meanStdDev(mag,mean,stddev);double v=mean[0]+0.5*stddev[0];if(mode==AutofocusMode::Bahtinov){cv::Mat lap;cv::Laplacian(c,lap,CV_32F);cv::meanStdDev(lap,mean,stddev);v+=stddev[0];}return v;}
bool AutofocusEngine::interruptibleSleep(int milliseconds,const Cancellation&cancel){
    int remaining=std::max(0,milliseconds);
    while(remaining>0){if(cancel&&cancel())return false;const int slice=std::min(remaining,25);QThread::msleep(slice);remaining-=slice;}
    return !(cancel&&cancel());
}
static bool waitForFocuserIdle(IFocuser&foc,const AutofocusEngine::Cancellation&cancel,QString&error,int timeoutMs=120000){
    QElapsedTimer timer;timer.start();
    while(timer.elapsed()<timeoutMs){
        if(cancel&&cancel()){error="Autofocus cancelled";return false;}
        FocuserStatus status;
        if(!foc.status(status,&error))return false;
        if(!status.moving)return true;
        int remaining=200;
        while(remaining>0){if(cancel&&cancel()){error="Autofocus cancelled";return false;}const int slice=std::min(remaining,25);QThread::msleep(slice);remaining-=slice;}
    }
    error=QString("Focuser motion did not complete within %1 ms").arg(timeoutMs);
    return false;
}
std::vector<FocusSample> AutofocusEngine::scan(ICamera&cam,IFocuser&foc,AutofocusMode mode,int start,int end,int step,int frames,int settle,bool autoRoi,const Progress&cb,const Cancellation&cancel){
    std::vector<FocusSample> out;if(step<=0)return out;
    for(int p=start;p<=end;p+=step){
        if(cancel&&cancel())break;
        QString err;if(!foc.moveAbsolute(std::max(0,p),&err))continue;
        if(!waitForFocuserIdle(foc,cancel,err))break;
        if(!interruptibleSleep(std::max(0,settle),cancel))break;
        std::vector<double> vals;
        for(int i=0;i<std::max(1,frames);++i){
            if(cancel&&cancel())break;
            CameraFrame f;ExposureRequest e;e.exposureSec=0.05;if(cam.capture(e,f,&err))vals.push_back(score(f.image,mode,autoRoi));
        }
        if(cancel&&cancel())break;
        if(vals.empty())continue;
        double med=median(vals),spread=0;for(double x:vals)spread+=std::abs(x-med);spread/=vals.size();FocusSample sm{std::max(0,p),med,spread};out.push_back(sm);if(cb)cb(sm);
    }
    return out;
}
AutofocusResult AutofocusEngine::run(ICamera&cam,IFocuser&foc,const AutofocusRequest&r,const Progress&cb,const Cancellation&cancel){
    AutofocusResult out;FocuserStatus fs;QString err;
    if(cancel&&cancel()){out.message="Autofocus cancelled";return out;}
    if(!foc.status(fs,&err)){out.message=err;return out;}
    int half=r.rangeSteps/2;
    auto coarse=scan(cam,foc,r.mode,std::max(0,fs.position-half),fs.position+half,std::max(1,r.coarseStep),r.framesPerPosition,r.settleMs,r.autoPlanetRoi,cb,cancel);
    if(cancel&&cancel()){foc.halt(nullptr);out.message="Autofocus cancelled";return out;}
    if(coarse.size()<3){out.message="Not enough coarse focus samples";return out;}
    auto best=std::max_element(coarse.begin(),coarse.end(),[](auto&a,auto&b){return a.score<b.score;});
    int fineHalf=std::max(r.coarseStep*2,r.fineStep*3);
    auto fine=scan(cam,foc,r.mode,std::max(0,best->position-fineHalf),best->position+fineHalf,std::max(1,r.fineStep),r.framesPerPosition,r.settleMs,r.autoPlanetRoi,cb,cancel);
    out.samples=coarse;out.samples.insert(out.samples.end(),fine.begin(),fine.end());
    if(cancel&&cancel()){foc.halt(nullptr);out.message="Autofocus cancelled";return out;}
    if(!fine.empty())best=std::max_element(fine.begin(),fine.end(),[](auto&a,auto&b){return a.score<b.score;});
    if(!foc.moveAbsolute(best->position,&err)){out.message=err;return out;}
    if(!waitForFocuserIdle(foc,cancel,err)){out.message=err;return out;}
    out.success=true;out.bestPosition=best->position;out.bestScore=best->score;out.message="Autofocus completed";return out;
}

}
