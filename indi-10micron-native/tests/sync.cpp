#include "tenmicron_protocol.h"
#include <iostream>
#include <stdexcept>
int main(int argc,char **argv){
 if(argc!=3)return 2;
 try{
  std::string mode=argv[2];tenmicron::Protocol p(std::chrono::milliseconds(150));
  p.connect("127.0.0.1",static_cast<unsigned short>(std::stoi(argv[1])));p.ultraPrecision();
  if(mode=="invalid"){
   try{p.syncOffset(24,42);}catch(const std::invalid_argument&){return 0;}
   return 1;
  }
  auto result=p.syncOffset(3.5,42.25);
  bool accepted=mode=="success" || mode=="internal_padding" || mode=="repeat";
  if(result.accepted()!=accepted)throw std::runtime_error("Unexpected sync acceptance");
  if(mode=="repeat" && !p.syncOffset(3.5,42.25).accepted())return 1;
  return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
