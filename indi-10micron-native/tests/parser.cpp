#include "tenmicron_protocol.h"
#include <cassert>
#include <iostream>
int main(){
 auto s=tenmicron::parseInfo("12.5,-20,E,123,45,2461318.5L,0,1,future");
 assert(s.raHours==12.5 && s.decDegrees==-20 && s.pierSide=='E');
 assert(s.leapSecond && s.slewOrSettling && s.extraFields.size()==1);
 auto unknown=tenmicron::parseInfo("0,90,W,0,-90,2461318,123,0");
 assert(unknown.nativeStatus==123 && tenmicron::statusName(123)=="unrecognized");
 for(auto bad:{"", "1,2,E,3,4,5,0", "24,2,E,3,4,5,0,0", "1,nan,E,3,4,5,0,0", "1,2,X,3,4,5,0,0", "1,2,E,360,4,5,0,0", "1,2,E,3,4,5,0,2", "1,2,E,3,4,5,0x,0"}){
  bool rejected=false;try{tenmicron::parseInfo(bad);}catch(...){rejected=true;}assert(rejected);
 }
 std::cout<<"Parser tests passed\n";
}
