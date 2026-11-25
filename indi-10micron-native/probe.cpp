#include "tenmicron_protocol.h"
#include <iostream>
#include <stdexcept>
int main(int argc,char **argv) {
 try {
  if(argc<2 || argc>4){std::cerr<<"Usage: tenmicron-probe HOST [PORT=3490] [TIMEOUT_MS=2000]\n";return 2;}
  auto parse=[](const char *v){size_t n;int x=std::stoi(v,&n);if(v[n]!='\0')throw std::invalid_argument("invalid argument");return x;};
  int port=argc>2?parse(argv[2]):3490;
  int timeout=argc>3?parse(argv[3]):2000;
  if(port<1 || port>65535)throw std::invalid_argument("invalid port");
  tenmicron::Protocol p{std::chrono::milliseconds(timeout)};
  p.connect(argv[1],static_cast<unsigned short>(port));p.ultraPrecision();
  std::cout<<"Product: "<<p.product()<<"\nFirmware: "<<p.firmware()<<"\nController: "<<p.controller()<<'\n';
  auto s=p.poll();
  std::cout<<"RA hours: "<<s.raHours<<"\nDec degrees: "<<s.decDegrees
   <<"\nNative pier: "<<s.pierSide<<"\nNative status: "<<s.nativeStatus<<" ("<<tenmicron::statusName(s.nativeStatus)
   <<")\nSlew or settling: "<<s.slewOrSettling<<"\nLeap second: "<<s.leapSecond<<'\n';
 } catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
