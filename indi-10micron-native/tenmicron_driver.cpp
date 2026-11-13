#include "tenmicron_protocol.h"
#include <inditelescope.h>
#include <indicom.h>
#include <indilogger.h>
#include <cstring>
#include <memory>
#include <cstdio>
#include <stdexcept>

class TenMicronReadOnly : public INDI::Telescope {
 public:
 TenMicronReadOnly(){setVersion(0,4);SetTelescopeCapability(TELESCOPE_HAS_PIER_SIDE,0);setTelescopeConnection(CONNECTION_NONE);}
 const char *getDefaultName() override {return "10Micron Native ReadOnly";}
 bool initProperties() override {
  INDI::Telescope::initProperties();
  EqNP.setPermission(IP_RO);
  IUFillText(&EndpointT[0],"HOST","Mount IP","192.168.0.1");
  IUFillText(&EndpointT[1],"PORT","TCP Port","3490");
  IUFillTextVector(&EndpointTP,EndpointT,2,getDeviceName(),"NATIVE_ENDPOINT","Mount Network",CONNECTION_TAB,IP_RW,0,IPS_IDLE);
  IUFillText(&IdentityT[0],"PRODUCT","Product","");
  IUFillText(&IdentityT[1],"FIRMWARE","Firmware","");
  IUFillText(&IdentityT[2],"CONTROLLER","Controller","");
  IUFillTextVector(&IdentityTP,IdentityT,3,getDeviceName(),"NATIVE_IDENTITY","Identification","10Micron Status",IP_RO,0,IPS_IDLE);
  IUFillText(&StatusT[0],"DESCRIPTION","Native state","");
  IUFillText(&StatusT[1],"COMMUNICATION","Communication","");
  IUFillTextVector(&StatusTP,StatusT,2,getDeviceName(),"NATIVE_STATUS_TEXT","Diagnostics","10Micron Status",IP_RO,0,IPS_IDLE);
  const char *names[]={"STATUS_CODE","SLEW_OR_SETTLING","PARKED","TRACKING","AZIMUTH","ALTITUDE","JULIAN_DATE","LEAP_SECOND"};
  const char *labels[]={"Native status code","Slew or settling","Parked","Tracking","Azimuth (deg)","Altitude (deg)","UTC Julian date","Leap second"};
  for(int i=0;i<8;i++) IUFillNumber(&StateN[i],names[i],labels[i],"%.8f",-1e8,1e8,0,0);
  IUFillNumberVector(&StateNP,StateN,8,getDeviceName(),"NATIVE_STATE","Mount State","10Micron Status",IP_RO,0,IPS_IDLE);
  const char *mn[]={"SLEW_LIMIT_DEG","TRACK_LIMIT_DEG","TIME_TO_END_MIN","ALLOWED_OBJECT_SIDE","UNATTENDED_FLIP","STORED_TARGET_DESTINATION_SIDE"};
  const char *ml[]={"Slew meridian limit (deg)","Tracking meridian limit (deg)","Estimated tracking end (min)","Allowed object side (1=both,2=west,3=east)","Unattended flip enabled","Stored target side (0=none/unreachable,2=west,3=east)"};
  for(int i=0;i<6;i++) IUFillNumber(&MeridianN[i],mn[i],ml[i],"%.0f",0,1e8,0,0);
  IUFillNumberVector(&MeridianNP,MeridianN,6,getDeviceName(),"NATIVE_MERIDIAN","Meridian Settings","10Micron Meridian",IP_RO,0,IPS_IDLE);
  setDefaultPollingPeriod(1000);addDebugControl();return true;
 }
 void ISGetProperties(const char *dev) override {
  INDI::Telescope::ISGetProperties(dev);defineProperty(&EndpointTP);
 }
 bool updateProperties() override {
  INDI::Telescope::updateProperties();
  if(isConnected()) {defineProperty(&IdentityTP);defineProperty(&StatusTP);defineProperty(&StateNP);defineProperty(&MeridianNP);publish(lastState);MeridianNP.s=IPS_IDLE;IDSetNumber(&MeridianNP,"Waiting for first meridian query.");}
  else {deleteProperty(IdentityTP.name);deleteProperty(StatusTP.name);deleteProperty(StateNP.name);deleteProperty(MeridianNP.name);}
  return true;
 }
 bool ISNewText(const char *dev,const char *name,char *texts[],char *names[],int n) override {
  if(!std::strcmp(dev,getDeviceName()) && !std::strcmp(name,EndpointTP.name)) {
   if(isConnected()) {EndpointTP.s=IPS_ALERT;IDSetText(&EndpointTP,"Disconnect before changing the endpoint.");return false;}
   if(IUUpdateText(&EndpointTP,texts,names,n)<0) return false;
   EndpointTP.s=IPS_OK;IDSetText(&EndpointTP,nullptr);return true;
  }
  return INDI::Telescope::ISNewText(dev,name,texts,names,n);
 }
 bool ISNewNumber(const char *dev,const char *name,double values[],char *names[],int n) override {
  if(!std::strcmp(dev,getDeviceName())) {
   if(!std::strcmp(name,"POLLING_PERIOD"))
    return INDI::DefaultDevice::ISNewNumber(dev,name,values,names,n);
   LOGF_WARN("Read-only driver rejects numeric write: %s",name);return false;
  }
  return INDI::Telescope::ISNewNumber(dev,name,values,names,n);
 }
 protected:
 bool Connect() override {
  try {
   size_t used=0;std::string portText=EndpointT[1].text;
   int port=std::stoi(portText,&used);
   if(used!=portText.size() || port<1 || port>65535)throw std::runtime_error("Invalid TCP port");
   protocol.connect(EndpointT[0].text,static_cast<unsigned short>(port));
   protocol.ultraPrecision();
   auto product=protocol.product(), firmware=protocol.firmware(), controller=protocol.controller();
   // Require documented combined-state format (firmware 2.14.9 or later).
   int major=0,minor=0,patch=0;
   int parts=std::sscanf(firmware.c_str(),"%d.%d.%d",&major,&minor,&patch);
   if(parts<2 || major<2 || (major==2 && (minor<14 || (minor==14 && patch<9))))
    throw std::runtime_error("Requires firmware 2.14.9 or newer with documented Ginfo format");
   lastState=protocol.poll();
   IUSaveText(&IdentityT[0],product.c_str());IUSaveText(&IdentityT[1],firmware.c_str());IUSaveText(&IdentityT[2],controller.c_str());
   IdentityTP.s=IPS_OK;healthy=true;lastMeridianPoll=std::chrono::steady_clock::time_point{};SetTimer(getCurrentPollingPeriod());
   LOG_INFO("Read-only native connection established.");return true;
  }catch(const std::exception &e){protocol.disconnect();LOGF_ERROR("Connection failed: %s",e.what());return false;}
 }
 bool Disconnect() override {protocol.disconnect();healthy=false;return true;}
 bool ReadScopeStatus() override {
  if(!healthy) return false;
  try {
   lastState=protocol.poll();publish(lastState);
   auto now=std::chrono::steady_clock::now();
   if(now-lastMeridianPoll>=std::chrono::seconds(10)) {
    auto m=protocol.meridian();
    double data[]={static_cast<double>(m.slewLimitDegrees),static_cast<double>(m.trackLimitDegrees),static_cast<double>(m.timeToEndMinutes),static_cast<double>(m.allowedSide),static_cast<double>(m.unattendedFlip),static_cast<double>(m.targetDestinationSide)};
    for(int i=0;i<6;i++) MeridianN[i].value=data[i];
    MeridianNP.s=IPS_OK;IDSetNumber(&MeridianNP,nullptr);lastMeridianPoll=now;
   }
   return true;
  }
  catch(const std::exception &e){
   protocol.disconnect();healthy=false;
   MeridianNP.s=IPS_ALERT;IDSetNumber(&MeridianNP,nullptr);
   StateNP.s=IPS_ALERT;IDSetNumber(&StateNP,nullptr);
   StatusTP.s=IPS_ALERT;IUSaveText(&StatusT[1],e.what());IDSetText(&StatusTP,nullptr);
   PierSideSP.setState(IPS_ALERT);PierSideSP.apply();
   LOGF_ERROR("Polling stopped; disconnect/reconnect required: %s",e.what());return false;
  }
 }
 bool saveConfigItems(FILE *fp) override {INDI::Telescope::saveConfigItems(fp);IUSaveConfigText(fp,&EndpointTP);return true;}
 private:
 void publish(const tenmicron::State &s) {
  bool tracking=s.nativeStatus==0 || s.nativeStatus==9 || s.nativeStatus==10;
  bool unknown=s.nativeStatus==98 || s.nativeStatus==99 || s.nativeStatus>11;
  bool fault=s.nativeStatus==8 || s.nativeStatus==9 || s.nativeStatus==11 || s.nativeStatus>=98 || s.nativeStatus>11;
  TrackState=SCOPE_IDLE;
  if(tracking) TrackState=SCOPE_TRACKING;
  else if(s.nativeStatus==5) TrackState=SCOPE_PARKED;
  else if(s.nativeStatus==2) TrackState=SCOPE_PARKING;
  else if(s.nativeStatus==3 || s.nativeStatus==4 || s.nativeStatus==6 || s.slewOrSettling) TrackState=SCOPE_SLEWING;
  setPierSide(s.pierSide=='E'?PIER_EAST:PIER_WEST);
  PierSideSP.setState(IPS_OK);PierSideSP.apply();
  NewRaDec(s.raHours,s.decDegrees);
  if(fault){EqNP.setState(IPS_ALERT);EqNP.apply();}
  double data[]={static_cast<double>(s.nativeStatus),static_cast<double>(s.slewOrSettling),unknown?-1.0:static_cast<double>(s.nativeStatus==5),unknown?-1.0:static_cast<double>(tracking),s.azDegrees,s.altDegrees,s.julianDate,static_cast<double>(s.leapSecond)};
  for(int i=0;i<8;i++) StateN[i].value=data[i];
  StateNP.s=fault?IPS_ALERT:IPS_OK;IDSetNumber(&StateNP,nullptr);
  auto description=tenmicron::statusName(s.nativeStatus);
  IUSaveText(&StatusT[0],description.c_str());IUSaveText(&StatusT[1],"Connected; last poll valid");
  StatusTP.s=fault?IPS_ALERT:IPS_OK;IDSetText(&StatusTP,nullptr);
 }
 tenmicron::Protocol protocol;
 tenmicron::State lastState{};
 bool healthy=false;
 std::chrono::steady_clock::time_point lastMeridianPoll{};
 INumber MeridianN[6]{};INumberVectorProperty MeridianNP{};
 IText EndpointT[2]{},IdentityT[3]{},StatusT[2]{};
 ITextVectorProperty EndpointTP{},IdentityTP{},StatusTP{};
 INumber StateN[8]{};INumberVectorProperty StateNP{};
};
static std::unique_ptr<TenMicronReadOnly> driver(new TenMicronReadOnly());
