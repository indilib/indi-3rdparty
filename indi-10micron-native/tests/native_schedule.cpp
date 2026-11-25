#include "native_schedule.h"
#include <limits>
#include <stdexcept>
#include <iostream>
using namespace tenmicron;
void check(bool b){if(!b)throw std::runtime_error("Native schedule assertion failed");}
int main(){
 TrackingBudgetInput i{true,true,true,true,4,0,120,5,10,60,15};
 auto b=trackingBudget(i);check(b.decision==ExposureDecision::Allow && b.remainingSeconds==180);
 i.exposureSeconds=165;check(trackingBudget(i).decision==ExposureDecision::Hold); // exact boundary
 i.exposureSeconds=164;check(trackingBudget(i).decision==ExposureDecision::Allow);
 i.sampleAgeSeconds=1;check(trackingBudget(i).decision==ExposureDecision::Hold); // age consumes slack
 i.sampleAgeSeconds=15;check(trackingBudget(i).decision==ExposureDecision::Unknown);
 i.sampleAgeSeconds=0;i.timeToEndMinutes=1;check(trackingBudget(i).decision==ExposureDecision::Hold);
 i.timeToEndMinutes=0;check(trackingBudget(i).decision==ExposureDecision::Hold);
 i.timeToEndMinutes=4;i.connected=false;check(trackingBudget(i).decision==ExposureDecision::Unknown);
 i.connected=true;i.nativeValid=false;check(trackingBudget(i).decision==ExposureDecision::Unknown);
 i.nativeValid=true;i.tracking=false;check(trackingBudget(i).decision==ExposureDecision::Hold);
 i.tracking=true;i.settled=false;check(trackingBudget(i).decision==ExposureDecision::Hold);
 i.settled=true;
 for(double bad:{-1.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){
  auto x=i;x.timeToEndMinutes=bad;check(trackingBudget(x).decision==ExposureDecision::Unknown);
  x=i;x.sampleAgeSeconds=bad;check(trackingBudget(x).decision==ExposureDecision::Unknown);
  x=i;x.exposureSeconds=bad;check(trackingBudget(x).decision==ExposureDecision::Unknown);
  x=i;x.marginSeconds=bad;check(trackingBudget(x).decision==ExposureDecision::Unknown);
 }
 auto x=i;x.timeToEndMinutes=std::numeric_limits<double>::max();check(trackingBudget(x).decision==ExposureDecision::Unknown);
 // Same policy for horizon and meridian deadlines: neither authorizes a flip.
 for(double end:{2.0,3.0,4.0}){i.timeToEndMinutes=end;check(trackingBudget(i).decision==(end==4?ExposureDecision::Allow:ExposureDecision::Hold));}
 std::cout<<"Native exposure budget: fresh/stale telemetry, exact boundaries, age, stopped/settling, invalid input and overflow passed\n";
}
