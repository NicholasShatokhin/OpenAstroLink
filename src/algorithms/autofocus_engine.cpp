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
static double sceneFocusScore(const cv::Mat &im){
    if(im.empty())return 0.0;
    cv::Mat gray;
    if(im.channels()==3)cv::cvtColor(im,gray,cv::COLOR_BGR2GRAY);else gray=im;
    cv::Mat f;
    if(gray.depth()==CV_16U)gray.convertTo(f,CV_32F,1.0/65535.0);
    else if(gray.depth()==CV_8U)gray.convertTo(f,CV_32F,1.0/255.0);
    else {double mn=0,mx=0;cv::minMaxLoc(gray,&mn,&mx);gray.convertTo(f,CV_32F,1.0/std::max(1.0,mx));}
    if(std::max(f.cols,f.rows)>1400){const double k=1400.0/double(std::max(f.cols,f.rows));cv::resize(f,f,{},k,k,cv::INTER_AREA);}
    cv::Mat smooth;cv::GaussianBlur(f,smooth,{0,0},0.8);
    cv::Mat lap;cv::Laplacian(smooth,lap,CV_32F,3);cv::Scalar meanLap,stdLap;cv::meanStdDev(lap,meanLap,stdLap);
    cv::Mat gx,gy,mag;cv::Sobel(smooth,gx,CV_32F,1,0,3);cv::Sobel(smooth,gy,CV_32F,0,1,3);cv::magnitude(gx,gy,mag);cv::Scalar meanMag,stdMag;cv::meanStdDev(mag,meanMag,stdMag);
    const cv::Scalar meanI=cv::mean(smooth);cv::Mat clipped=smooth>=0.995f;const double clipFrac=double(cv::countNonZero(clipped))/double(std::max(1,smooth.rows*smooth.cols));
    const double texture=(stdLap[0]*stdLap[0])*1.0e5+(meanMag[0]+0.35*stdMag[0])*1.0e3;const double brightnessNorm=0.25+std::sqrt(std::max(0.0,meanI[0]));
    return texture/brightnessNorm*std::clamp(1.0-clipFrac,0.05,1.0);
}
double AutofocusEngine::score(const cv::Mat&im,AutofocusMode mode,bool autoRoi,int *detectedStars){
    if(detectedStars)*detectedStars=0;
    cv::Mat g=toGray8(im);cv::Rect roi(0,0,g.cols,g.rows);if(mode==AutofocusMode::Planet&&autoRoi)roi=planetRoi(g);cv::Mat c=g(roi);
    if(mode==AutofocusMode::Stars){StarDetector det;auto stars=det.detect(c);if(detectedStars)*detectedStars=int(stars.size());std::vector<double> q;for(const auto&s:stars)if(s.hfrPx>0.1)q.push_back(s.flux/(s.hfrPx*s.hfrPx));return median(q);}
    if(mode==AutofocusMode::Scene)return sceneFocusScore(im);
    cv::Mat gx,gy;cv::Sobel(c,gx,CV_32F,1,0,3);cv::Sobel(c,gy,CV_32F,0,1,3);cv::Mat mag;cv::magnitude(gx,gy,mag);cv::Scalar mean,stddev;cv::meanStdDev(mag,mean,stddev);double v=mean[0]+0.5*stddev[0];
    if(mode==AutofocusMode::Bahtinov){cv::Mat lap;cv::Laplacian(c,lap,CV_32F);cv::meanStdDev(lap,mean,stddev);v+=stddev[0];}
    return v;
}
bool AutofocusEngine::interruptibleSleep(int milliseconds,const Cancellation&cancel){int remaining=std::max(0,milliseconds);while(remaining>0){if(cancel&&cancel())return false;const int slice=std::min(remaining,25);QThread::msleep(slice);remaining-=slice;}return !(cancel&&cancel());}
static bool waitForFocuserIdle(IFocuser&foc,const AutofocusEngine::Cancellation&cancel,QString&error,int timeoutMs=120000){QElapsedTimer timer;timer.start();while(timer.elapsed()<timeoutMs){if(cancel&&cancel()){error="Autofocus cancelled";return false;}FocuserStatus status;if(!foc.status(status,&error))return false;if(!status.moving)return true;int remaining=200;while(remaining>0){if(cancel&&cancel()){error="Autofocus cancelled";return false;}const int slice=std::min(remaining,25);QThread::msleep(slice);remaining-=slice;}}error=QString("Focuser motion did not complete within %1 ms").arg(timeoutMs);return false;}
std::vector<FocusSample> AutofocusEngine::scan(ICamera&cam,IFocuser&foc,const AutofocusRequest&r,int start,int end,int step,const Progress&cb,const Cancellation&cancel,const FrameProgress&frameCb){
    std::vector<FocusSample> out;if(step<=0)return out;
    for(int p=start;p<=end;p+=step){
        if(cancel&&cancel())break;QString err;if(!foc.moveAbsolute(std::max(0,p),&err))continue;if(!waitForFocuserIdle(foc,cancel,err))break;if(!interruptibleSleep(std::max(0,r.settleMs),cancel))break;
        std::vector<double> vals;std::vector<int> starCounts;
        bool previewSent=false;for(int i=0;i<std::max(1,r.framesPerPosition);++i){if(cancel&&cancel())break;CameraFrame f;ExposureRequest e;e.exposureSec=std::max(0.000001,r.exposureSec);e.gain=std::max(0,r.gain);int stars=0;if(cam.capture(e,f,&err)){if(frameCb&&!previewSent){frameCb(f,std::max(0,p));previewSent=true;}const double v=score(f.image,r.mode,r.autoPlanetRoi,&stars);if(r.mode!=AutofocusMode::Stars||stars>=std::max(1,r.minStars))vals.push_back(v);starCounts.push_back(stars);}}
        if(cancel&&cancel())break;if(vals.empty()){FocusSample sm{std::max(0,p),0.0,0.0,starCounts.empty()?0:*std::max_element(starCounts.begin(),starCounts.end())};out.push_back(sm);if(cb)cb(sm);continue;}
        double med=median(vals),spread=0;for(double x:vals)spread+=std::abs(x-med);spread/=vals.size();const int stars=starCounts.empty()?0:*std::max_element(starCounts.begin(),starCounts.end());FocusSample sm{std::max(0,p),med,spread,stars};out.push_back(sm);if(cb)cb(sm);
    }
    return out;
}
AutofocusResult AutofocusEngine::run(ICamera&cam,IFocuser&foc,const AutofocusRequest&r,const Progress&cb,const Cancellation&cancel,const FrameProgress&frameCb){
    AutofocusResult out;FocuserStatus fs;QString err;if(cancel&&cancel()){out.message="Autofocus cancelled";return out;}if(!foc.status(fs,&err)){out.message=err;return out;}
    const int coarseStep=std::max(1,r.coarseStep),fineStep=std::max(1,r.fineStep);int half=std::max(coarseStep,r.rangeSteps/2);
    auto coarse=scan(cam,foc,r,std::max(0,fs.position-half),fs.position+half,coarseStep,cb,cancel,frameCb);if(cancel&&cancel()){foc.halt(nullptr);out.message="Autofocus cancelled";return out;}
    if(coarse.size()<3){out.message="Not enough coarse focus samples";return out;}
    if(r.mode==AutofocusMode::Stars){const int maxStars=std::max_element(coarse.begin(),coarse.end(),[](const auto&a,const auto&b){return a.detectedStars<b.detectedStars;})->detectedStars;if(maxStars<std::max(1,r.minStars)){out.samples=coarse;out.message=QString("No suitable stars detected (need at least %1 per focus frame). Use Scene autofocus for daytime/structured targets or increase exposure/gain.").arg(std::max(1,r.minStars));return out;}}
    auto bestIt=std::max_element(coarse.begin(),coarse.end(),[](const auto&a,const auto&b){return a.score<b.score;});if(bestIt==coarse.end()||bestIt->score<=0.0){out.samples=coarse;out.message="No usable focus contrast detected";return out;}
    if(r.mode==AutofocusMode::Scene&&bestIt==std::prev(coarse.end())){const int extStart=bestIt->position+coarseStep,extEnd=bestIt->position+half;if(extStart<=extEnd){auto ext=scan(cam,foc,r,extStart,extEnd,coarseStep,cb,cancel,frameCb);coarse.insert(coarse.end(),ext.begin(),ext.end());bestIt=std::max_element(coarse.begin(),coarse.end(),[](const auto&a,const auto&b){return a.score<b.score;});}}
    else if(r.mode==AutofocusMode::Scene&&bestIt==coarse.begin()&&bestIt->position>0){const int extStart=std::max(0,bestIt->position-half),extEnd=std::max(0,bestIt->position-coarseStep);if(extStart<=extEnd){auto ext=scan(cam,foc,r,extStart,extEnd,coarseStep,cb,cancel,frameCb);coarse.insert(coarse.end(),ext.begin(),ext.end());bestIt=std::max_element(coarse.begin(),coarse.end(),[](const auto&a,const auto&b){return a.score<b.score;});}}
    if(cancel&&cancel()){foc.halt(nullptr);out.message="Autofocus cancelled";return out;}if(bestIt==coarse.end()||bestIt->score<=0.0){out.samples=coarse;out.message="No usable focus contrast detected";return out;}
    FocusSample coarseBest=*bestIt;if(r.mode==AutofocusMode::Scene){std::vector<double> scores;for(const auto &x:coarse)if(x.score>0)scores.push_back(x.score);const double baseline=median(scores);if(baseline<=0||coarseBest.score<baseline*1.025){out.samples=coarse;out.message="Scene focus curve is too flat to identify a reliable peak; use a more detailed target, increase focus range, or adjust exposure";return out;}}
    const int fineHalf=std::max(coarseStep,fineStep*4);auto fine=scan(cam,foc,r,std::max(0,coarseBest.position-fineHalf),coarseBest.position+fineHalf,fineStep,cb,cancel,frameCb);out.samples=coarse;out.samples.insert(out.samples.end(),fine.begin(),fine.end());if(cancel&&cancel()){foc.halt(nullptr);out.message="Autofocus cancelled";return out;}
    FocusSample chosen=coarseBest;if(!fine.empty()){auto candidate=std::max_element(fine.begin(),fine.end(),[](const auto&a,const auto&b){return a.score<b.score;});if(candidate!=fine.end()&&candidate->score>chosen.score*1.005)chosen=*candidate;}
    if(chosen.score<=0.0){out.message="No usable focus metric peak detected";return out;}if(!foc.moveAbsolute(chosen.position,&err)){out.message=err;return out;}if(!waitForFocuserIdle(foc,cancel,err)){out.message=err;return out;}
    out.success=true;out.bestPosition=chosen.position;out.bestScore=chosen.score;out.message=r.mode==AutofocusMode::Scene?"Scene autofocus completed with a stable focus peak":"Autofocus completed";return out;
}

}
