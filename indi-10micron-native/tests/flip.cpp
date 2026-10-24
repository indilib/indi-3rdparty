#include "tenmicron_protocol.h"
#include <iostream>
#include <stdexcept>
int main(int argc,char **argv){
 if(argc!=3)return 2;
 try{
  const std::string mode=argv[2];tenmicron::Protocol p(std::chrono::milliseconds(150));
  p.connect("127.0.0.1",static_cast<unsigned short>(std::stoi(argv[1])));p.ultraPrecision();
  auto result=p.requestFlip(mode=="different_target"?4:3.5,42.25);
  bool accepted=mode=="accepted" || mode=="hash" || mode=="delayed_hash";
  if(result.accepted!=accepted)throw std::runtime_error("Unexpected flip acceptance");
  if(result.before.pierSide!='W' || result.before.raHours!=3.5 || result.before.decDegrees!=42.25)
   throw std::runtime_error("Missing fresh before-state");
  if(result.confirmedRejection!=(mode=="rejected"))throw std::runtime_error("Wrong confirmed rejection");
  p.poll(); // Verify next framing after byte-only or optional hash reply.
  return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
