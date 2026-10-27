#pragma once
#include <algorithm>
#include <cmath>
#include <string>
namespace tenmicron {
// Pure, read-only scheduling policy. No command, arming or settings writes.
// Gmte estimates horizon OR meridian end and is never flip permission.
struct TrackingBudgetInput {
 bool connected=false, tracking=false, settled=false;
 bool nativeValid=false;
 double timeToEndMinutes=0;
 double sampleAgeSeconds=0;
 double exposureSeconds=0;
 double overheadSeconds=0;
 double marginSeconds=0;
 // Integer-minute rounding is undocumented; reserve a whole minute.
 double estimateUncertaintySeconds=60;
 double maximumSampleAgeSeconds=15;
};
enum class ExposureDecision {Unknown,Hold,Allow};
struct TrackingBudget {
 ExposureDecision decision=ExposureDecision::Unknown;
 double remainingSeconds=0;
 std::string reason;
};
inline TrackingBudget trackingBudget(const TrackingBudgetInput &i){
 auto valid=[](double x){return std::isfinite(x) && x>=0;};
 if(!i.connected || !i.nativeValid)return {ExposureDecision::Unknown,0,"Native tracking-end telemetry unavailable"};
 if(!valid(i.timeToEndMinutes) || !valid(i.sampleAgeSeconds) ||
    !valid(i.exposureSeconds) || !valid(i.overheadSeconds) || !valid(i.marginSeconds) ||
    !valid(i.estimateUncertaintySeconds) || !valid(i.maximumSampleAgeSeconds) ||
    i.maximumSampleAgeSeconds==0 || i.exposureSeconds==0)
  return {ExposureDecision::Unknown,0,"Invalid budget input"};
 if(i.sampleAgeSeconds>=i.maximumSampleAgeSeconds)
  return {ExposureDecision::Unknown,0,"Native tracking-end telemetry stale"};
 if(!i.tracking || !i.settled)return {ExposureDecision::Hold,0,"Mount is not settled and tracking"};
 const double raw=i.timeToEndMinutes*60;
 const double need=i.exposureSeconds+i.overheadSeconds+i.marginSeconds;
 if(!std::isfinite(raw) || !std::isfinite(need))return {ExposureDecision::Unknown,0,"Budget arithmetic overflow"};
 const double remaining=std::max(0.0,raw-i.sampleAgeSeconds-i.estimateUncertaintySeconds);
 if(need>=remaining)return {ExposureDecision::Hold,remaining,"Next exposure does not fit native tracking budget"};
 return {ExposureDecision::Allow,remaining,"Next exposure fits native tracking budget; flip availability remains separate"};
}
}
