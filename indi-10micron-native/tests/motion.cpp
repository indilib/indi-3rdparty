#include "tenmicron_protocol.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <atomic>
void check(bool value){if(!value)throw std::runtime_error("motion check failed");}
int main(int argc,char **argv){
 try {
  check(tenmicron::formatTargetRA(23.9999999)=="00:00:00.00");
  check(tenmicron::formatTargetRA(3.5)=="03:30:00.00");
  check(tenmicron::formatTargetDec(-0.0)=="-00*00:00.0");
  check(tenmicron::formatTargetDec(90)=="+90*00:00.0");
  for(double bad:{-1.0,24.0,std::numeric_limits<double>::quiet_NaN()}){
   bool rejected=false;try{tenmicron::formatTargetRA(bad);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
  }
  bool rejected=false;try{tenmicron::formatTargetDec(90.1);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
  if(argc!=4)return 2;
  tenmicron::Protocol p(std::chrono::milliseconds(250));p.connect("127.0.0.1",static_cast<unsigned short>(std::stoi(argv[1])));p.ultraPrecision();
  int expected=std::stoi(argv[2]);std::string mode=argv[3];
  if(mode=="controls"){
   p.haltSlew();p.stopAll();p.park();p.unpark();p.setTracking(true);p.setTracking(false);check(p.poll().nativeStatus==7);return 0;
  }
  if(mode=="invalid_input"){
   bool fail=false;try{p.slew(3.5,42.25,1);}catch(const std::invalid_argument&){fail=true;}check(fail);check(p.poll().nativeStatus==7);return 0;
  }
  std::atomic<bool> done{false};std::exception_ptr pollError;
  std::thread polling;
  if(mode=="concurrent")polling=std::thread([&]{try{while(!done)p.poll();}catch(...){pollError=std::current_exception();}});
  try {
   auto result=p.slew(3.5,42.25,mode=="forced"?2:0);
   done=true;if(polling.joinable())polling.join();if(pollError)std::rethrow_exception(pollError);
   check(result.code==expected);check(result.accepted()==(expected==0));
   if(expected>0)check(!result.message.empty());
   check(p.poll().nativeStatus==(mode=="parked"?5:mode=="busy"?6:7));
  }catch(...){done=true;if(polling.joinable())polling.join();throw;}
  std::cout<<"Motion test passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
