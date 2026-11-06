#include "motion_state.h"
#include <iostream>
#include <stdexcept>
using namespace tenmicron;
void check(bool b){if(!b)throw std::runtime_error("motion state check failed");}
int main(){
 State s{};s.raHours=3;s.decDegrees=42;s.nativeStatus=0;
 MotionProgress m;m.begin(MotionKind::Slew,4,42);check(m.observe(s)==MotionOutcome::Waiting); // Old tracking state must not complete new slew.
 s.raHours=4;s.slewOrSettling=true;check(m.observe(s)==MotionOutcome::Waiting);
 s.slewOrSettling=false;check(m.observe(s)==MotionOutcome::Complete);
 s.nativeStatus=6;check(m.observe(s)==MotionOutcome::Waiting);
 s.nativeStatus=9;check(m.observe(s)==MotionOutcome::Failed);
 s.nativeStatus=5;check(m.observe(s)==MotionOutcome::Failed);
 s.nativeStatus=0;check(m.observe(s,m.started+std::chrono::seconds(181))==MotionOutcome::Failed);
 m.begin(MotionKind::Slew,23.9999,42);s.raHours=0.0001;check(m.observe(s)==MotionOutcome::Complete);
 m.begin(MotionKind::Park);check(m.observe(s)==MotionOutcome::Waiting);s.nativeStatus=2;check(m.observe(s)==MotionOutcome::Waiting);s.nativeStatus=5;check(m.observe(s)==MotionOutcome::Complete);
 m.begin(MotionKind::Unpark);check(m.observe(s)==MotionOutcome::Waiting);s.nativeStatus=3;check(m.observe(s)==MotionOutcome::Waiting);s.nativeStatus=7;check(m.observe(s)==MotionOutcome::Complete);
 // Outside-limits unpark remains pending; only later healthy state completes.
 m.begin(MotionKind::Unpark);s.nativeStatus=9;s.slewOrSettling=false;
 check(m.observe(s)==MotionOutcome::Waiting);
 m.unparkTrackingOffRequested=true;s.nativeStatus=0;check(m.observe(s)==MotionOutcome::Waiting);s.nativeStatus=9;
 check(m.observe(s,m.started+std::chrono::seconds(181))==MotionOutcome::Failed);
 for(int code:{8,11,98,99}){s.nativeStatus=code;check(m.observe(s)==MotionOutcome::Failed);}
 s.nativeStatus=7;check(m.observe(s)==MotionOutcome::Complete);
 for(auto kind:{MotionKind::Slew,MotionKind::Park,MotionKind::TrackingOn,MotionKind::TrackingOff,MotionKind::StopAll,MotionKind::Halt}){
  m.begin(kind);s.nativeStatus=9;check(m.observe(s)==MotionOutcome::Failed);
 }
 // Tracking has its own five-second observation window; no wall-clock sleep.
 for(auto kind:{MotionKind::TrackingOn,MotionKind::TrackingOff}){
  m.begin(kind);s.nativeStatus=kind==MotionKind::TrackingOn?1:0;s.slewOrSettling=false;
  check(m.trackingTransition());check(m.observe(s,m.started+std::chrono::milliseconds(4999))==MotionOutcome::Waiting);
  check(m.observe(s,m.started+std::chrono::seconds(5))==MotionOutcome::Failed);
  s.nativeStatus=kind==MotionKind::TrackingOn?0:7;
  check(m.observe(s,m.started+std::chrono::seconds(5))==MotionOutcome::Complete);
  s.slewOrSettling=true;check(m.observe(s,m.started+std::chrono::seconds(5))==MotionOutcome::Failed);
 }
 s.slewOrSettling=false;
 for(int code:{0,1,5,7}){s.nativeStatus=code;check(settledTrackingState(s));s.slewOrSettling=true;check(!settledTrackingState(s));s.slewOrSettling=false;}
 for(int code:{2,3,4,6,8,9,10,11,98,99}){s.nativeStatus=code;check(!settledTrackingState(s));}
 for(auto kind:{MotionKind::Slew,MotionKind::Flip,MotionKind::Park,MotionKind::Unpark,MotionKind::Halt,MotionKind::StopAll}){
  m.begin(kind);check(!m.trackingTransition());
 }
 s.nativeStatus=7;
 m.begin(MotionKind::TrackingOn);check(m.observe(s)==MotionOutcome::Waiting);s.nativeStatus=0;check(m.observe(s)==MotionOutcome::Complete);
 m.begin(MotionKind::TrackingOff);check(m.observe(s)==MotionOutcome::Waiting);s.nativeStatus=7;check(m.observe(s)==MotionOutcome::Complete);
 m.begin(MotionKind::StopAll);s.nativeStatus=0;check(m.observe(s)==MotionOutcome::Waiting);
 s.nativeStatus=6;s.slewOrSettling=true;check(m.observe(s)==MotionOutcome::Waiting);
 s.nativeStatus=1;check(m.observe(s)==MotionOutcome::Waiting);s.slewOrSettling=false;check(m.observe(s)==MotionOutcome::Complete);
 s.nativeStatus=7;check(m.observe(s)==MotionOutcome::Complete);s.nativeStatus=5;check(m.observe(s)==MotionOutcome::Complete);
 s.nativeStatus=99;check(m.observe(s)==MotionOutcome::Failed);
 s.nativeStatus=0;check(m.observe(s,m.started+std::chrono::seconds(181))==MotionOutcome::Failed);
 // Both directions: no successful flip without the opposite side AND normal completion.
 for(char side:{'E','W'}){
  s.raHours=23.9999;s.decDegrees=42;s.nativeStatus=0;s.slewOrSettling=false;s.pierSide=side;
  m.beginFlip(s);check(m.observe(s)==MotionOutcome::Waiting); // Old tracking sample, exact coordinates.
  s.nativeStatus=6;s.slewOrSettling=true;check(m.observe(s)==MotionOutcome::Waiting);
  s.pierSide=side=='E'?'W':'E';check(m.observe(s)==MotionOutcome::Waiting);
  s.slewOrSettling=false;check(m.observe(s)==MotionOutcome::Waiting); // Still native slewing.
  s.nativeStatus=0;s.raHours=0.0001;s.slewOrSettling=true;check(m.observe(s)==MotionOutcome::Waiting);
  s.slewOrSettling=false;check(m.observe(s)==MotionOutcome::Complete); // RA wrap.
  s.raHours=1;check(m.observe(s)==MotionOutcome::Waiting);
  s.raHours=0.0001;s.decDegrees=43;check(m.observe(s)==MotionOutcome::Waiting);
  s.decDegrees=42;s.nativeStatus=7;check(m.observe(s)==MotionOutcome::Waiting); // Tracking off.
  s.nativeStatus=5;check(m.observe(s)==MotionOutcome::Failed);
  for(int fault:{8,9,11,98,99}){s.nativeStatus=fault;check(m.observe(s)==MotionOutcome::Failed);}
  s.nativeStatus=0;s.pierSide=side;check(m.observe(s,m.started+std::chrono::seconds(181))==MotionOutcome::Failed);
  s.pierSide='?';check(m.observe(s)==MotionOutcome::Waiting); // Unknown side never completes.
 }
 for(int code:{1,5,6,7,9}){
  s.nativeStatus=code;s.slewOrSettling=false;s.pierSide='W';bool rejected=false;
  try{m.beginFlip(s);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
 }
 s.nativeStatus=0;s.pierSide='?';bool rejected=false;
 try{m.beginFlip(s);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
 m.begin(MotionKind::Flip,3,42);check(m.observe(s)==MotionOutcome::Failed); // Missing recorded sides.
 m.begin(MotionKind::Slew,3,42);s.raHours=3;s.decDegrees=42;s.pierSide='W';check(m.observe(s)==MotionOutcome::Complete); // Ordinary slew does not require a side change.
 m.begin(MotionKind::Halt);s.nativeStatus=6;s.slewOrSettling=true;check(m.observe(s)==MotionOutcome::Waiting);
 s.nativeStatus=0;check(m.observe(s)==MotionOutcome::Waiting);s.slewOrSettling=false;check(m.observe(s)==MotionOutcome::Complete);
 s.nativeStatus=1;check(m.observe(s)==MotionOutcome::Complete);s.nativeStatus=7;check(m.observe(s)==MotionOutcome::Complete);
 s.nativeStatus=5;check(m.observe(s)==MotionOutcome::Complete); // Halt does not clear interrupted-park safety.
 s.nativeStatus=99;check(m.observe(s)==MotionOutcome::Failed);
 std::cout<<"Native motion completion tests passed\n";
}
