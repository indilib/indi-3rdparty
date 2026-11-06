#include "tenmicron_protocol.h"
#include <iostream>
#include <stdexcept>
int main(int argc,char **argv) {
 try {
  if(argc!=3) return 2;
  tenmicron::Protocol p;
  p.connect("127.0.0.1",static_cast<unsigned short>(std::stoi(argv[1])));
  p.ultraPrecision();
  auto m=p.meridian();
  if(m.slewLimitDegrees!=5 || m.trackLimitDegrees!=15 || m.timeToEndMinutes!=123 || m.allowedSide!=1 || !m.unattendedFlip || m.targetDestinationSide!=std::stoi(argv[2]))
   throw std::runtime_error("meridian mismatch");
  auto s=p.poll();
  if(s.nativeStatus!=5) throw std::runtime_error("subsequent state corrupted");
  std::cout<<"Meridian tests passed\n";
 } catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
