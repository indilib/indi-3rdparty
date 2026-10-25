#pragma once
#include <string>
#include <memory>
#include <deque>
#include <array>
#include <cstdint>
#include <algorithm>
#include <stdexcept>
constexpr int LOW=0,HIGH=1,OUTPUT=1,INPUT_PULLUP=2,WL_CONNECTED=3;
inline uint32_t fakeTime=0;
inline std::array<int,32> levels{};
inline std::array<int,32> modes{};
inline unsigned int simultaneous=0,otaCalls=0;
inline bool otaWhileActive=false;
inline unsigned long millis(){return fakeTime;}
inline void delay(unsigned long ms){fakeTime+=static_cast<uint32_t>(ms);}
inline void pinMode(int pin,int mode){modes.at(pin)=mode;}
inline void digitalWrite(int pin,int value){
 levels.at(pin)=value;
 unsigned int n=0;for(int p:{4,7,8}) if(levels[p]==HIGH)++n;
 simultaneous=std::max(simultaneous,n);
}
inline int digitalRead(int pin){return levels.at(pin);}
class String {
 std::string s;
public:
 String()=default; String(const char* v):s(v){} String(char v):s(1,v){}
 String(const std::string&v):s(v){}
 size_t length()const{return s.size();}
 char charAt(unsigned int i)const{return i<s.size()?s[i]:0;}
 void trim(){auto a=s.find_first_not_of(" \r\n\t");if(a==std::string::npos){s.clear();return;}s=s.substr(a,s.find_last_not_of(" \r\n\t")-a+1);}
 bool startsWith(const char*v)const{return s.rfind(v,0)==0;}
 bool endsWith(const char*v)const{std::string t(v);return s.size()>=t.size()&&s.compare(s.size()-t.size(),t.size(),t)==0;}
 int indexOf(char c,int pos=0)const{auto p=s.find(c,pos);return p==std::string::npos?-1:static_cast<int>(p);}
 String substring(unsigned int a)const{return a>=s.size()?String(""):String(s.substr(a));}
 String substring(unsigned int a,unsigned int b)const{if(a>b)std::swap(a,b);a=std::min<size_t>(a,s.size());b=std::min<size_t>(b,s.size());return String(s.substr(a,b-a));}
 String&operator+=(char c){s+=c;return *this;}
 friend String operator+(const String&a,const String&b){return String(a.s+b.s);}
 bool operator==(const char*v)const{return s==v;}
 bool operator!=(const char*v)const{return s!=v;}
 const std::string&str()const{return s;}
};
struct FakeSerial{
 int availableForWrite()const{return 128;}
 void begin(int){} explicit operator bool()const{return true;}
 template<class T>void print(const T&){}template<class T>void println(const T&){}
};inline FakeSerial Serial;
struct Peer{bool connected=true;std::deque<char> rx;std::string tx;};
class WiFiClient{
public:
 std::shared_ptr<Peer> p;
 WiFiClient()=default;explicit WiFiClient(std::shared_ptr<Peer>x):p(x){}
 explicit operator bool()const{return bool(p);}
 bool connected()const{return p&&p->connected;}
 int available()const{return p?static_cast<int>(p->rx.size()):0;}
 int read(){if(!available())return -1;char c=p->rx.front();p->rx.pop_front();return static_cast<unsigned char>(c);}
 void stop(){if(p)p->connected=false;p.reset();}
 void print(const String&v){if(connected())p->tx+=v.str();}
 void print(char c){if(connected())p->tx+=c;}
 void print(const char* s){if(connected())p->tx+=s;}
};
class WiFiServer{
public:
 std::deque<WiFiClient> pending;
 explicit WiFiServer(int){} void begin(){} void end(){pending.clear();}
 WiFiClient available(){if(pending.empty())return {};auto c=pending.front();pending.pop_front();return c;}
};
struct FakeIP{String toString()const{return String("test-address");}};
struct FakeWiFi{void begin(const char*,const char*){}int status()const{return WL_CONNECTED;}FakeIP localIP()const{return {};}};inline FakeWiFi WiFi;
