#pragma once
#include "tenmicron_protocol.h"
#include <chrono>
#include <cmath>
#include <string>
#include <stdexcept>
namespace tenmicron {
using MotionClock=std::chrono::steady_clock;
enum class MotionKind {None,Slew,Flip,Park,Unpark,TrackingOn,TrackingOff,StopAll,Halt};
enum class MotionOutcome {None,Waiting,Complete,Failed};
struct MotionProgress {
 MotionKind kind=MotionKind::None;
 MotionClock::time_point started{};
 double ra=0,dec=0;
 char initialPier=0,expectedPier=0;
 bool unparkTrackingOffRequested=false;
 void begin(MotionKind next,double targetRA=0,double targetDec=0){kind=next;started=MotionClock::now();ra=targetRA;dec=targetDec;initialPier=expectedPier=0;unparkTrackingOffRequested=false;}
 void beginFlip(const State &initial){
  if((initial.pierSide!='E' && initial.pierSide!='W') || initial.nativeStatus!=0 || initial.slewOrSettling)
   throw std::invalid_argument("Flip requires a tracking, settled native state and known pier side");
  begin(MotionKind::Flip,initial.raHours,initial.decDegrees);
  initialPier=initial.pierSide;expectedPier=initialPier=='E'?'W':'E';
 }
 bool active()const{return kind!=MotionKind::None;}
 MotionOutcome observe(const State &s,MotionClock::time_point now=MotionClock::now())const{
  if(!active())return MotionOutcome::None;
  if(now-started>std::chrono::seconds(180))return MotionOutcome::Failed;
  // Unpark can enable tracking while still at a park position outside tracking
  // limits. The driver requests tracking OFF and must confirm a later healthy
  // stationary sample. Status 9 alone is never successful completion.
  if(kind==MotionKind::Unpark && s.nativeStatus==9)return MotionOutcome::Waiting;
  if(s.nativeStatus==8 || s.nativeStatus==9 || s.nativeStatus==11 || s.nativeStatus>11)return MotionOutcome::Failed;
  if(s.slewOrSettling)return MotionOutcome::Waiting;
  switch(kind){
   case MotionKind::Park:return s.nativeStatus==5?MotionOutcome::Complete:MotionOutcome::Waiting;
   case MotionKind::Unpark:return ((!unparkTrackingOffRequested && s.nativeStatus==0) || s.nativeStatus==1 || s.nativeStatus==7)?MotionOutcome::Complete:MotionOutcome::Waiting;
   case MotionKind::TrackingOn:return s.nativeStatus==0?MotionOutcome::Complete:MotionOutcome::Waiting;
   case MotionKind::TrackingOff:return (s.nativeStatus==1 || s.nativeStatus==7 || s.nativeStatus==5)?MotionOutcome::Complete:MotionOutcome::Waiting;
   case MotionKind::Halt:return (s.nativeStatus==0 || s.nativeStatus==1 || s.nativeStatus==7 || s.nativeStatus==5)?MotionOutcome::Complete:MotionOutcome::Waiting;
   case MotionKind::StopAll:return (s.nativeStatus==1 || s.nativeStatus==7 || s.nativeStatus==5)?MotionOutcome::Complete:MotionOutcome::Waiting;
   case MotionKind::Flip:
    // Coordinates alone cannot prove an opposite-side flip has completed.
    if(initialPier==0 || expectedPier==0 || initialPier==expectedPier)return MotionOutcome::Failed;
    if(s.nativeStatus==5)return MotionOutcome::Failed;
    if(s.pierSide!=expectedPier)return MotionOutcome::Waiting;
    [[fallthrough]];
   case MotionKind::Slew:{
    if(s.nativeStatus==5)return MotionOutcome::Failed;
    double dra=std::abs(s.raHours-ra);dra=std::min(dra,24-dra);
    // Within one arcminute per coordinate, including circular RA wrap.
    return s.nativeStatus==0 && dra*15<=1.0/60 && std::abs(s.decDegrees-dec)<=1.0/60?MotionOutcome::Complete:MotionOutcome::Waiting;
   }
   default:return MotionOutcome::None;
  }
 }
};
}
