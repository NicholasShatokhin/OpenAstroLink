#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>

namespace oal::skywatcher_mc {

// EXPERIMENTAL direct Motor Controller codec: keep HIL safety gates enabled.
// Sky-Watcher Motor Controller Command Set codec.  This is the low-level
// protocol used by EQMOD-compatible controllers and by SynScan Wi-Fi direct
// transport (UDP/11880).  It is deliberately separate from the higher-level
// SynScan hand-controller/app protocols.
inline std::string command(char opcode, int axis, const std::string &payload={}) {
    return std::string(":") + opcode + char('0' + axis) + payload + "\r";
}

inline std::string trimLine(std::string value) {
    while (!value.empty() && (value.back()=='\r' || value.back()=='\n' || std::isspace(static_cast<unsigned char>(value.back())))) value.pop_back();
    std::size_t first=0;while(first<value.size()&&std::isspace(static_cast<unsigned char>(value[first])))++first;
    return value.substr(first);
}

inline bool isNormalResponse(const std::string &reply) {
    const auto t=trimLine(reply);return !t.empty()&&t.front()=='=';
}
inline bool isErrorResponse(const std::string &reply) {
    const auto t=trimLine(reply);return !t.empty()&&t.front()=='!';
}
inline std::string responsePayload(const std::string &reply) {
    const auto t=trimLine(reply);return (!t.empty()&&(t.front()=='='||t.front()=='!'))?t.substr(1):std::string{};
}

inline int hexNibble(char c) {
    if(c>='0'&&c<='9')return c-'0';
    c=char(std::toupper(static_cast<unsigned char>(c)));
    if(c>='A'&&c<='F')return 10+c-'A';
    return -1;
}
inline std::optional<std::uint32_t> decodeU24(const std::string &transport) {
    if(transport.size()!=6)return std::nullopt;
    auto byteAt=[&](std::size_t p)->std::optional<unsigned>{const int hi=hexNibble(transport[p]),lo=hexNibble(transport[p+1]);if(hi<0||lo<0)return std::nullopt;return unsigned((hi<<4)|lo);};
    auto b0=byteAt(0),b1=byteAt(2),b2=byteAt(4);if(!b0||!b1||!b2)return std::nullopt;
    return std::uint32_t(*b0)|std::uint32_t(*b1<<8)|std::uint32_t(*b2<<16);
}
inline char hexDigit(unsigned x){return x<10?char('0'+x):char('A'+(x-10));}
inline std::string encodeU24(std::uint32_t value) {
    value&=0xFFFFFFu;std::string out;out.reserve(6);
    for(unsigned shift: {0u,8u,16u}){const unsigned b=(value>>shift)&0xFFu;out.push_back(hexDigit(b>>4));out.push_back(hexDigit(b&0xF));}
    return out;
}

// Encoder positions are transported as unsigned 24-bit values with 0x800000
// representing the controller's zero reference.
inline std::optional<std::int32_t> decodePosition(const std::string &reply) {
    if(!isNormalResponse(reply))return std::nullopt;auto v=decodeU24(responsePayload(reply));if(!v)return std::nullopt;return std::int32_t(*v)-0x800000;
}
inline std::string encodePosition(std::int32_t counts) {
    return encodeU24(std::uint32_t(std::int64_t(counts)+0x800000ll));
}
inline std::optional<std::uint32_t> decodeCount(const std::string &reply) {
    if(!isNormalResponse(reply))return std::nullopt;return decodeU24(responsePayload(reply));
}

struct AxisStatus {
    bool valid{false};
    bool gotoMode{false};
    bool running{false};
    bool blocked{false};
    bool initialized{false};
    bool fastMode{false};
    bool clockwise{false};
    int modeNibble{0};
    int stateNibble{0};
    int initNibble{0};
};
inline AxisStatus parseStatus(const std::string &reply) {
    AxisStatus s;if(!isNormalResponse(reply))return s;const auto p=responsePayload(reply);if(p.size()<3)return s;
    const int a=hexNibble(p[0]),b=hexNibble(p[1]),c=hexNibble(p[2]);if(a<0||b<0||c<0)return s;
    s.valid=true;s.modeNibble=a;s.stateNibble=b;s.initNibble=c;
    // Sky-Watcher/EQMOD semantics (also used by INDI's skywatcherAPI):
    //   mode bit0 == 0 : SlewTo/GOTO, == 1 : continuous slew/tracking
    //   mode bit1 == 0 : forward,      == 1 : reverse
    //   state bit0 == 1: axis running, == 0 : full stop
    //   init  bit0 == 1: initialization complete
    // Previous OAL revisions inverted all three of these bits, which made a
    // stopped direct-Wi-Fi axis look "running" and could short-circuit motion
    // sequencing before :J ever had a chance to move the motor.
    s.gotoMode=(a&0x1)==0;s.clockwise=(a&0x2)==0;s.fastMode=(a&0x4)!=0;
    s.running=(b&0x1)!=0;s.blocked=(b&0x2)!=0;s.initialized=(c&0x1)!=0;
    return s;
}

inline std::string getVersion(int axis){return command('e',axis);}
inline std::string getCountsPerRev(int axis){return command('a',axis);}
inline std::string getTimerFrequency(int axis=1){return command('b',axis);}
inline std::string getPosition(int axis){return command('j',axis);}
inline std::string getStatus(int axis){return command('f',axis);}
inline std::string initDone(int axis){return command('F',axis);}
inline std::string stop(int axis){return command('K',axis);}
inline std::string instantStop(int axis){return command('L',axis);}
inline std::string startMotion(int axis){return command('J',axis);}
inline std::string setMotionMode(int axis,bool gotoMode,bool highSpeed,bool forward){
    // EQMOD/Sky-Watcher convention:
    //   function 0 = high-speed SlewTo/GOTO
    //   function 1 = low-speed continuous slew
    //   function 2 = low-speed SlewTo/GOTO
    //   function 3 = high-speed continuous slew
    // Direction 0 means forward and 1 means reverse.  Keep the boolean surface
    // in physical "forward" form so a positive requested encoder increment does
    // not accidentally get encoded as reverse.
    const char func=gotoMode?(highSpeed?'0':'2'):(highSpeed?'3':'1');
    return command('G',axis,std::string(1,func)+(forward?"0":"1"));
}
inline std::string setGotoIncrement(int axis,std::uint32_t counts){return command('H',axis,encodeU24(counts));}
inline std::string setBrakeIncrement(int axis,std::uint32_t counts){return command('M',axis,encodeU24(counts));}
inline std::string setStepPeriod(int axis,std::uint32_t period){return command('I',axis,encodeU24(std::max<std::uint32_t>(1,period)));}

} // namespace oal::skywatcher_mc
