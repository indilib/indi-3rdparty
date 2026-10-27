#include "tenmicron_protocol.h"
#include <iostream>
int main(int argc,char **argv) {
 if(argc!=3)return 2;
 try {
  tenmicron::Protocol p(std::chrono::milliseconds(250));
  p.connectSerial(argv[1],static_cast<unsigned>(std::stoul(argv[2])));
  p.ultraPrecision();std::cout<<p.product()<<'\n';
  p.setGuideRate(7.5);p.guide('n',123);p.disconnect();return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
