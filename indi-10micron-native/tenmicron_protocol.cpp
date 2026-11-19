#include "tenmicron_protocol.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <cerrno>
#include <cmath>
#include <climits>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <locale>
namespace tenmicron {
namespace {
using Clock = std::chrono::steady_clock;
void ready(int fd, short events, Clock::time_point deadline) {
 for (;;) {
  auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline-Clock::now()).count();
  if (left <= 0) throw std::runtime_error("protocol timeout");
  pollfd p{fd,events,0};
  int result = ::poll(&p,1,static_cast<int>(left > INT_MAX ? INT_MAX : left));
  if (result < 0 && errno == EINTR) continue;
  if (result < 0) throw std::runtime_error("poll failed");
  if (result == 0) throw std::runtime_error("protocol timeout");
  if (p.revents & events) return;
  throw std::runtime_error("connection closed or failed");
 }
}
double number(const std::string &s, double low, double high) {
 size_t used = 0;
 double v = std::stod(s,&used);
 if (used != s.size() || !std::isfinite(v) || v < low || v > high)
  throw std::runtime_error("invalid numeric Ginfo field");
 return v;
}
int integer(const std::string &s) {
 size_t used = 0;
 int v = std::stoi(s,&used);
 if (used != s.size() || v < 0) throw std::runtime_error("invalid status code");
 return v;
}
}
State parseInfo(const std::string &payload) {
 std::vector<std::string> f;
 size_t start=0;
 for (;;) {
  auto comma=payload.find(',',start);
  f.push_back(payload.substr(start,comma==std::string::npos?comma:comma-start));
  if (comma==std::string::npos) break;
  start=comma+1;
 }
 if (f.size()<8) throw std::runtime_error("Ginfo needs eight fields");
 State s{};
 s.raHours=number(f[0],0,24);
 if (s.raHours==24) throw std::runtime_error("RA must be below 24h");
 s.decDegrees=number(f[1],-90,90);
 if (f[2]!="E" && f[2]!="W") throw std::runtime_error("invalid native pier side");
 s.pierSide=f[2][0];
 s.azDegrees=number(f[3],0,360);
 if (s.azDegrees==360) throw std::runtime_error("azimuth must be below 360");
 s.altDegrees=number(f[4],-90,90);
 s.leapSecond=!f[5].empty() && f[5].back()=='L';
 auto jd=f[5]; if(s.leapSecond) jd.pop_back();
 s.julianDate=number(jd,0,1e8);
 s.nativeStatus=integer(f[6]);
 if(f[7]!="0" && f[7]!="1") throw std::runtime_error("invalid slew flag");
 s.slewOrSettling=f[7]=="1";
 s.extraFields.assign(f.begin()+8,f.end());
 return s;
}
std::string statusName(int c) {
 switch(c) {
 case 0:return "tracking";case 1:return "stopped";case 2:return "parking";
 case 3:return "unparking";case 4:return "slewing_home";case 5:return "parked";
 case 6:return "slewing_or_stopping";case 7:return "tracking_off_stationary";
 case 8:return "motors_cold";case 9:return "tracking_outside_limits";
 case 10:return "satellite";case 11:return "user_authorization_required";
 case 98:return "unknown";case 99:return "error";default:return "unrecognized";
 }
}
Protocol::Protocol(std::chrono::milliseconds t):timeout_(t) {
 if(t.count()<=0) throw std::invalid_argument("timeout must be positive");
}
Protocol::~Protocol(){closeSocket();}
void Protocol::closeSocket(){if(fd_>=0){::close(fd_);fd_=-1;}pendingFlagTerminator_=false;serial_=false;}
void Protocol::disconnect(){std::lock_guard<std::mutex> lock(mutex_);closeSocket();}
void Protocol::connect(const std::string &host,unsigned short port) {
 std::lock_guard<std::mutex> lock(mutex_);closeSocket();
 addrinfo hints{};hints.ai_socktype=SOCK_STREAM;hints.ai_family=AF_UNSPEC;
 addrinfo *addresses=nullptr;
 auto service=std::to_string(port);
 if(::getaddrinfo(host.c_str(),service.c_str(),&hints,&addresses)!=0)
  throw std::runtime_error("address resolution failed");
 // DNS resolution is OS-managed and is not covered by the socket deadline.
 auto deadline=Clock::now()+timeout_;
 for(auto a=addresses;a;a=a->ai_next) {
  int fd=::socket(a->ai_family,a->ai_socktype,a->ai_protocol);
  if(fd<0) continue;
  int flags=::fcntl(fd,F_GETFL,0);
  if(flags<0 || ::fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0){::close(fd);continue;}
  bool connected=::connect(fd,a->ai_addr,a->ai_addrlen)==0;
  if(!connected && errno==EINPROGRESS) {
   try {ready(fd,POLLOUT,deadline);int err=0;socklen_t len=sizeof(err);
    connected=::getsockopt(fd,SOL_SOCKET,SO_ERROR,&err,&len)==0 && err==0;
   } catch(...) {connected=false;}
  }
  if(connected){fd_=fd;break;}
  ::close(fd);
 }
 ::freeaddrinfo(addresses);
 if(fd_<0) throw std::runtime_error("TCP connection failed");
}
void Protocol::connectSerial(const std::string &path,unsigned baud) {
 std::lock_guard<std::mutex> lock(mutex_);closeSocket();
 speed_t speed;
 switch(baud) {
 case 1200:speed=B1200;break;case 2400:speed=B2400;break;
 case 4800:speed=B4800;break;case 9600:speed=B9600;break;
 case 19200:speed=B19200;break;case 38400:speed=B38400;break;
 case 57600:speed=B57600;break;case 115200:speed=B115200;break;
 default:throw std::invalid_argument("Unsupported serial baud rate");
 }
 int fd=::open(path.c_str(),O_RDWR|O_NOCTTY|O_NONBLOCK|O_CLOEXEC);
 if(fd<0)throw std::runtime_error("Cannot open serial device");
 termios settings{};
 if(::tcgetattr(fd,&settings)<0){::close(fd);throw std::runtime_error("Cannot read serial settings");}
 ::cfmakeraw(&settings);
 settings.c_cflag=(settings.c_cflag & ~(CSIZE|PARENB|CSTOPB|CRTSCTS)) | CS8 | CLOCAL | CREAD;
 settings.c_cc[VMIN]=0;settings.c_cc[VTIME]=0;
 if(::cfsetispeed(&settings,speed)<0 || ::cfsetospeed(&settings,speed)<0 ||
    ::tcsetattr(fd,TCSANOW,&settings)<0 || ::tcflush(fd,TCIOFLUSH)<0) {
  ::close(fd);throw std::runtime_error("Cannot configure serial device");
 }
 fd_=fd;serial_=true;
}
std::string Protocol::exchange(const std::string &command,bool response,bool oneByte,const std::string &allowedBytes) {
 std::lock_guard<std::mutex> lock(mutex_);
 return exchangeUnlocked(command,response,oneByte,allowedBytes);
}
std::string Protocol::exchangeUnlocked(const std::string &command,bool response,bool oneByte,const std::string &allowedBytes,bool slewReply) {
 if(fd_<0) throw std::runtime_error("not connected");
 auto started=Clock::now();
 auto deadline=started+timeout_;
 const char *phase="write";
 size_t received=0;
 try {
  size_t offset=0;
  while(offset<command.size()) {
   ready(fd_,POLLOUT,deadline);
   auto sent=serial_ ? ::write(fd_,command.data()+offset,command.size()-offset) : ::send(fd_,command.data()+offset,command.size()-offset,MSG_NOSIGNAL);
   if(sent<0 && (errno==EINTR || errno==EAGAIN || errno==EWOULDBLOCK)) continue;
   if(sent<=0) throw std::runtime_error("send failed");
   offset+=static_cast<size_t>(sent);
  }
  if(!response) return {};
  phase="read";
  std::string result;
  // One outstanding request; consume exactly through its terminator.
  for(;;) {
   ready(fd_,POLLIN,deadline);char c;
   auto n=serial_ ? ::read(fd_,&c,1) : ::recv(fd_,&c,1,0);
   if(n<0 && (errno==EINTR || errno==EAGAIN || errno==EWOULDBLOCK)) continue;
   if(n<=0) throw std::runtime_error("connection closed during response");
   ++received;
   if(pendingFlagTerminator_) {
    pendingFlagTerminator_=false;
    if(c=='#') continue;
   }
   if(oneByte && result.empty()) {
    if(allowedBytes.find(c)==std::string::npos) throw std::runtime_error("invalid single-byte response");
    if(slewReply && c!='0') { result.push_back(c); continue; }
    pendingFlagTerminator_=true;
    return std::string(1,c);
   }
   if(c=='#') return result;
   if(result.size()>=4096) throw std::runtime_error("response exceeds limit");
   result.push_back(c);
  }
 } catch(const std::exception &e) {
  // Never retry a command whose acceptance is unknown; discard framing too.
  const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-started).count();
  closeSocket();
  throw std::runtime_error("request="+command+" phase="+phase+
      " elapsed_ms="+std::to_string(elapsed)+" deadline_ms="+std::to_string(timeout_.count())+
      " received_bytes="+std::to_string(received)+"; "+e.what());
 }
}
void Protocol::ultraPrecision(){exchange(":U2#",false);}
std::string Protocol::product(){return exchange(":GVP#",true);}
std::string Protocol::firmware(){return exchange(":GVN#",true);}
std::string Protocol::controller(){return exchange(":GVZ#",true);}
MeridianState Protocol::meridian(){
 MeridianState s{};
 s.slewLimitDegrees=integer(exchange(":Glms#",true));
 s.trackLimitDegrees=integer(exchange(":Glmt#",true));
 s.timeToEndMinutes=integer(exchange(":Gmte#",true));
 s.allowedSide=integer(exchange(":GMF#",true));
 if(s.allowedSide<1 || s.allowedSide>3) throw std::runtime_error("invalid allowed meridian side");
 s.unattendedFlip=exchange(":Guaf#",true,true)=="1";
 s.targetDestinationSide=integer(exchange(":GTsid#",true,true,"023"));
 return s;
}
State Protocol::poll(){return parseInfo(exchange(":Ginfo#",true));}
std::string formatTargetRA(double hours) {
 if(!std::isfinite(hours) || hours<0 || hours>=24) throw std::invalid_argument("RA must be finite and in [0,24) hours");
 // Round first, so carry across seconds/minutes and midnight is valid.
 auto ticks=static_cast<long long>(std::llround(hours*360000.0))%8640000;
 std::ostringstream out;out.imbue(std::locale::classic());out<<std::setfill('0');
 out<<std::setw(2)<<ticks/360000<<':'<<std::setw(2)<<(ticks/6000)%60<<':'<<std::setw(2)<<(ticks/100)%60<<'.'<<std::setw(2)<<ticks%100;
 return out.str();
}
std::string formatTargetDec(double degrees) {
 if(!std::isfinite(degrees) || degrees<-90 || degrees>90) throw std::invalid_argument("Declination must be finite and in [-90,90] degrees");
 auto ticks=static_cast<long long>(std::llround(std::abs(degrees)*36000.0));
 std::ostringstream out;out.imbue(std::locale::classic());out<<std::setfill('0');
 out<<(std::signbit(degrees)?'-':'+')<<std::setw(2)<<ticks/36000<<'*'<<std::setw(2)<<(ticks/600)%60<<':'<<std::setw(2)<<(ticks/10)%60<<'.'<<ticks%10;
 return out.str();
}
SlewResult Protocol::slew(double raHours,double decDegrees,int forcedSide) {
 auto ra=formatTargetRA(raHours),dec=formatTargetDec(decDegrees);
 if(forcedSide!=0 && forcedSide!=2 && forcedSide!=3) throw std::invalid_argument("Forced side must be 0, 2 (west), or 3 (east)");
 // One lock for fresh state, both target writes and slew; local polling cannot interleave.
 // Other mount connections still share target registers: caller must ensure one writer.
 std::lock_guard<std::mutex> lock(mutex_);
 auto state=parseInfo(exchangeUnlocked(":Ginfo#",true));
 if(state.slewOrSettling || (state.nativeStatus!=0 && state.nativeStatus!=1 && state.nativeStatus!=7))
  return {-1,"Slew blocked by native state: "+statusName(state.nativeStatus)};
 if(exchangeUnlocked(":Sr"+ra+"#",true,true)!="1")return {-1,"Mount rejected target RA"};
 if(exchangeUnlocked(":Sd"+dec+"#",true,true)!="1")return {-1,"Mount rejected target declination; stored RA may have changed"};
 auto reply=exchangeUnlocked(forcedSide?":MSfs"+std::to_string(forcedSide)+"#":":MS#",true,true,"012345",true);
 int code=reply[0]-'0';
 return {code,code==0?"Slew accepted; completion requires native telemetry":reply.substr(1)};
}
SlewResult Protocol::syncOffset(double raHours,double decDegrees) {
 auto ra=formatTargetRA(raHours),dec=formatTargetDec(decDegrees);
 std::lock_guard<std::mutex> lock(mutex_);
 auto state=parseInfo(exchangeUnlocked(":Ginfo#",true));
 if(state.slewOrSettling || (state.nativeStatus!=0 && state.nativeStatus!=1 && state.nativeStatus!=7))
  return {-1,"Offset sync blocked by native state: "+statusName(state.nativeStatus)};
 // CMCFG is controller state, not connection state. Assert offset mode on EVERY sync.
 // No other connection may write targets or sync configuration during this transaction.
 if(exchangeUnlocked(":CMCFG0#",true)!="0") {
  closeSocket();throw std::runtime_error("Offset sync mode was not acknowledged; sync not sent");
 }
 if(exchangeUnlocked(":Sr"+ra+"#",true,true)!="1")return {-1,"Mount rejected sync RA"};
 if(exchangeUnlocked(":Sd"+dec+"#",true,true)!="1")return {-1,"Mount rejected sync declination; stored RA may have changed"};
 auto reply=exchangeUnlocked(":CM#",true);
 // Firmware may pad within words as well as at the end of its display-style reply.
 // Normalize ASCII whitespace only; still require the complete known response.
 std::string normalized;
 bool gap=false;
 for(char c:reply){
  if(c==' ' || c=='\t' || c=='\r' || c=='\n'){if(!normalized.empty())gap=true;}
  else {if(gap)normalized+=' ';normalized+=c;gap=false;}
 }
 reply=normalized;
 if(reply=="Coordinates matched")return {0,"Native offset sync acknowledged; model refinement disabled"};
 if(reply=="Match fail: dist. too large")return {-1,reply};
 closeSocket();throw std::runtime_error("Unexpected offset sync reply; result uncertain: "+reply);
}
FlipRequestResult Protocol::requestFlip(double raHours,double decDegrees){
 formatTargetRA(raHours);formatTargetDec(decDegrees);
 std::lock_guard<std::mutex> lock(mutex_);
 auto before=parseInfo(exchangeUnlocked(":Ginfo#",true));
 // Conservative preflight: the native controller remains the authority on flip legality.
 if(before.nativeStatus!=0 || before.slewOrSettling)
  return {false,"Flip blocked: native mount must be tracking and settled",before};
 double dra=std::abs(before.raHours-raHours);dra=std::min(dra,24-dra);
 if(dra*15>1.0/60 || std::abs(before.decDegrees-decDegrees)>1.0/60)
  return {false,"Native FLIP requires the current pointing coordinates; slew to the target first",before};
 auto reply=exchangeUnlocked(":FLIP#",true,true);
 if(reply=="1")return {true,"Flip accepted; opposite pier side and native completion required",before};
 if(reply!="0")throw std::runtime_error("Unexpected FLIP response: "+reply);
 const auto after=parseInfo(exchangeUnlocked(":Ginfo#",true));
 double moved=std::abs(before.raHours-after.raHours);moved=std::min(moved,24-moved);
 const bool unchanged=(before.pierSide=='E' || before.pierSide=='W') && after.nativeStatus==0 && !after.slewOrSettling &&
     after.pierSide==before.pierSide && moved*15<=1.0/60 &&
     std::abs(before.decDegrees-after.decDegrees)<=1.0/60;
 return {false,unchanged?"Mount rejected flip; unchanged settled tracking confirmed":"Mount rejected flip; motion state changed",before,unchanged};
}
void Protocol::haltSlew(){exchange(":Q#",false);}
void Protocol::stopAll(){exchange(":STOP#",false);}
void Protocol::park(){exchange(":hP#",false);}
void Protocol::unpark(){exchange(":PO#",false);}
void Protocol::setTracking(bool enabled){exchange(enabled?":AP#":":AL#",false);}

namespace {
std::string decimal(double value, int precision=4, bool sign=false) {
 std::ostringstream out;out.imbue(std::locale::classic());
 if(sign)out<<std::showpos;
 out<<std::fixed<<std::setprecision(precision)<<value;return out.str();
}
std::string refractionField(double value,bool sign) {
 std::ostringstream out;out.imbue(std::locale::classic());
 if(sign)out<<std::showpos;
 // Native sTTT.T / PPPP.P, matching the legacy 10Micron driver.
 out<<std::internal<<std::setfill('0')<<std::setw(6)<<std::fixed<<std::setprecision(1)<<value;
 return out.str();
}
void range(double v,double low,double high,const char *what){
 if(!std::isfinite(v)||v<low||v>high)throw std::invalid_argument(what);
}
void directionValid(char d){if(d!='n'&&d!='s'&&d!='e'&&d!='w')throw std::invalid_argument("Invalid direction");}
double angle(std::string text,double limit){
 for(char &c:text)if(c=='*')c=':';
 size_t a=text.find(':'),b=text.find(':',a==std::string::npos?0:a+1);
 if(a==std::string::npos||b==std::string::npos)throw std::runtime_error("Invalid native angle");
 double deg=number(text.substr(0,a),-limit,limit),min=number(text.substr(a+1,b-a-1),0,59),sec=number(text.substr(b+1),0,59.9999);
 double value=std::abs(deg)+min/60+sec/3600;
 if(value>limit)throw std::runtime_error("Native angle out of range");
 return text[0]=='-'?-value:value;
}
std::string siteAngle(double value,int width){
 auto ticks=static_cast<long long>(std::llround(std::abs(value)*36000));
 std::ostringstream out;out.imbue(std::locale::classic());out<<std::setfill('0')<<(std::signbit(value)?'-':'+')
 <<std::setw(width)<<ticks/36000<<'*'<<std::setw(2)<<(ticks/600)%60<<':'<<std::setw(2)<<(ticks/10)%60<<'.'<<ticks%10;return out.str();
}
}
void Protocol::guide(char d,unsigned ms){
 directionValid(d);if(ms<1||ms>9999)throw std::invalid_argument("Guide pulse must be 1..9999 ms");
 std::lock_guard<std::mutex> lock(mutex_);
 auto state=parseInfo(exchangeUnlocked(":Ginfo#",true));
 if(state.nativeStatus!=0||state.slewOrSettling)throw std::invalid_argument("Guide requires settled native tracking");
 std::ostringstream out;out<<":Mg"<<d<<std::setfill('0')<<std::setw(4)<<ms<<'#';exchangeUnlocked(out.str(),false);
}
void Protocol::move(char d,bool start){directionValid(d);exchange(std::string(start?":M":":Q")+d+"#",false);}
void Protocol::slewRate(int index){
 static const char *rates[]={":RG#",":RC#",":RM#",":RS#"};
 if(index<0||index>3)throw std::invalid_argument("Invalid slew rate index");
 exchange(rates[index],false);
}
double Protocol::guideRate(){return number(exchange(":Ggui#",true),0,15.1);}
void Protocol::setGuideRate(double rate){range(rate,0.1,15.0417,"Guide rate must be 0.1..15.0417 arcsec/s");exchange(":Rg"+decimal(rate,4)+"#",false);}
void Protocol::trackRate(double ra,double dec){
 range(ra,-150,150,"RA tracking rate outside candidate range");range(dec,-150,150,"DEC tracking rate outside candidate range");
 std::lock_guard<std::mutex> lock(mutex_);
 auto state=parseInfo(exchangeUnlocked(":Ginfo#",true));
 if(state.slewOrSettling||(state.nativeStatus!=0&&state.nativeStatus!=1&&state.nativeStatus!=7))throw std::invalid_argument("Custom rate requires settled unparked mount");
 // INDI RA is total sidereal-axis rate; native RR is an OFFSET from sidereal.
 if(exchangeUnlocked(":RR"+decimal(ra/15.041067-1,6,true)+"#",true,true)!="1")throw std::runtime_error("RA tracking rate rejected");
 if(exchangeUnlocked(":RD"+decimal(dec/15.041067,6,true)+"#",true,true)!="1")throw std::runtime_error("DEC tracking rate rejected; RA may have changed");
}
void Protocol::trackMode(int mode,double ra,double dec){
 if(mode<0||mode>3)throw std::invalid_argument("Invalid tracking mode");
 if(mode==3){trackRate(ra,dec);return;}
 std::lock_guard<std::mutex> lock(mutex_);
 auto state=parseInfo(exchangeUnlocked(":Ginfo#",true));
 if(state.slewOrSettling||(state.nativeStatus!=0&&state.nativeStatus!=1&&state.nativeStatus!=7))throw std::invalid_argument("Tracking mode requires settled unparked mount");
 static const char *modes[]={":RT2#",":RT1#",":RT0#"};exchangeUnlocked(modes[mode],false);
 if(state.nativeStatus!=0)exchangeUnlocked(":AL#",false);
}
double Protocol::utcOffset(){return -angle(exchange(":GG#",true),14);}
void Protocol::setUTC(const std::string &utc,double localOffset){
 range(localOffset,-14,14,"Invalid local UTC offset");
 // Only the driver formats this value; forbid command injection at protocol boundary.
 if(utc.size()!=19||utc[4]!='-'||utc[7]!='-'||utc[10]!=','||utc[13]!=':'||utc[16]!=':')throw std::invalid_argument("Invalid UTC format");
 for(size_t i=0;i<utc.size();++i)if(i!=4&&i!=7&&i!=10&&i!=13&&i!=16&&(utc[i]<'0'||utc[i]>'9'))throw std::invalid_argument("Invalid UTC field");
 std::lock_guard<std::mutex> lock(mutex_);
 if(exchangeUnlocked(":SUDT"+utc+"#",true,true)!="1")throw std::invalid_argument("Mount rejected UTC date/time");
 // Native SG has the opposite sign to INDI's UTC offset. It changes display conversion only.
 auto seconds=static_cast<long long>(std::llround(std::abs(localOffset)*3600));
 std::ostringstream zone;zone<<":SG"<<(localOffset>0?'-':'+')<<std::setfill('0')<<std::setw(2)<<seconds/3600<<':'<<std::setw(2)<<(seconds/60)%60<<':'<<std::setw(2)<<seconds%60<<'#';
 if(exchangeUnlocked(zone.str(),true,true)!="1")throw std::runtime_error("UTC set but display timezone rejected");
}
void Protocol::setLocation(double lat,double lon,double elevation){
 range(lat,-90,90,"Invalid latitude");range(lon,0,360,"Longitude must be INDI east-positive 0..360");range(elevation,-1000,9999.9,"Invalid elevation");
 double west=-(lon>180?lon-360:lon);
 std::lock_guard<std::mutex> lock(mutex_);
 if(exchangeUnlocked(":St"+siteAngle(lat,2)+"#",true,true)!="1")throw std::runtime_error("Latitude rejected");
 if(exchangeUnlocked(":Sg"+siteAngle(west,3)+"#",true,true)!="1")throw std::runtime_error("Longitude rejected; latitude may have changed");
 if(exchangeUnlocked(":Sev"+decimal(elevation,1,true)+"#",true,true)!="1")throw std::runtime_error("Elevation rejected; latitude/longitude may have changed");
}
std::vector<double> Protocol::location(){
 std::lock_guard<std::mutex> lock(mutex_);
 double lat=angle(exchangeUnlocked(":Gt#",true),90),west=angle(exchangeUnlocked(":Gg#",true),180),elev=number(exchangeUnlocked(":Gev#",true),-1000,9999.9);
 double east=-west;if(east<0)east+=360;return {lat,east,elev};
}
namespace {
int parseWeatherMode(const std::string &reply) {
 if(reply!="0" && reply!="1" && reply!="2")throw std::runtime_error("Invalid direct-weather mode reply");
 return reply[0]-'0';
}
}
int Protocol::weatherUpdateMode(){return parseWeatherMode(exchange(":WSG#",true));}
void Protocol::setWeatherUpdateMode(int mode){
 if(mode<0 || mode>2)throw std::invalid_argument("Direct-weather mode must be 0, 1 or 2");
 std::lock_guard<std::mutex> lock(mutex_);
 if(exchangeUnlocked(":WSS"+std::to_string(mode)+"#",true,true)!="1")throw SettingsRejected("Mount rejected direct-weather mode");
 if(parseWeatherMode(exchangeUnlocked(":WSG#",true))!=mode)throw SettingsRejected("Direct-weather mode readback mismatch");
}
std::vector<double> Protocol::refraction(){
 std::lock_guard<std::mutex> lock(mutex_);
 double temperature=number(exchangeUnlocked(":GRTMP#",true),-100,100),pressure=number(exchangeUnlocked(":GRPRS#",true),0,1200);
 return {temperature,pressure};
}
void Protocol::setRefraction(double temperature,double pressure,bool writeTemperature,bool writePressure){
 range(temperature,-100,100,"Temperature must be -100..100 C");range(pressure,0,1200,"Station pressure must be 0..1200 hPa");
 std::lock_guard<std::mutex> lock(mutex_);
 if(writeTemperature && exchangeUnlocked(":SRTMP"+refractionField(temperature,true)+"#",true,true)!="1")throw SettingsRejected("Mount rejected temperature; no pressure write sent");
 if(writePressure && exchangeUnlocked(":SRPRS"+refractionField(pressure,false)+"#",true,true)!="1")throw SettingsRejected(writeTemperature?"Mount rejected pressure; temperature may have changed":"Mount rejected pressure");
}

}
