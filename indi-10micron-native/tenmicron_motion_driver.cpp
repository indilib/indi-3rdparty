#include "tenmicron_protocol.h"
#include "motion_state.h"
#include "enclosure_state.h"
#include <algorithm>
#include <inditelescope.h>
#include <indiguiderinterface.h>
#include <cmath>
#include <libnova/julian_day.h>
#include <indicom.h>
#include <indidriver.h>
#include <indilogger.h>
#include <cstring>
#include <memory>
#include <cstdio>
#include <stdexcept>

class TenMicronMotion : public INDI::Telescope, public INDI::GuiderInterface {
 public:
 TenMicronMotion(): INDI::GuiderInterface(this){setVersion(0,28);SetTelescopeCapability(TELESCOPE_HAS_PIER_SIDE | TELESCOPE_CAN_FLIP | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_PARK | TELESCOPE_CAN_ABORT | TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_TRACK_MODE | TELESCOPE_HAS_TRACK_RATE | TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION,4);setTelescopeConnection(CONNECTION_NONE);}
 const char *getDefaultName() override {return "10Micron Native Motion";}
 bool initProperties() override {
  INDI::Telescope::initProperties();
  setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);
  INDI::GuiderInterface::initProperties(MOTION_TAB);
  for(auto &p:GuideNSNP)p.setMax(9999);
  for(auto &p:GuideWENP)p.setMax(9999);
  AddTrackMode("TRACK_SIDEREAL","Sidereal",true);
  AddTrackMode("TRACK_SOLAR","Solar");AddTrackMode("TRACK_LUNAR","Lunar");AddTrackMode("TRACK_CUSTOM","Custom");
  TrackModeSP.setState(IPS_IDLE); // Selected default is not a native mode readback.
  TrackRateNP[0].setMin(-150);TrackRateNP[0].setMax(150);
  TrackRateNP[1].setMin(-150);TrackRateNP[1].setMax(150);
  GuideRateNP[0].fill("RATE_ARCSEC_S","Both axes (arcsec/s)","%.4f",0.1,15.0417,0.1,7.5);
  GuideRateNP.fill(getDeviceName(),"NATIVE_GUIDE_RATE","Guide Rate",MOTION_TAB,IP_RW,0,IPS_IDLE);
  RefractionNP[0].fill("TEMPERATURE_C","Temperature (C)","%.1f",-100,100,0.1,0);
  RefractionNP[1].fill("PRESSURE_HPA","Station pressure (hPa)","%.1f",0,1200,0.1,0);
  RefractionNP.fill(getDeviceName(),"NATIVE_REFRACTION","Refraction Inputs","10Micron Status",IP_RW,0,IPS_IDLE);
  WeatherModeSP[0].fill("MANUAL","Manual inputs",ISS_OFF);
  WeatherModeSP[1].fill("AUTO_NOT_TRACKING","Auto when not tracking",ISS_OFF);
  WeatherModeSP[2].fill("AUTO_CONTINUOUS","Auto continuously",ISS_OFF);
  WeatherModeSP.fill(getDeviceName(),"NATIVE_WEATHER_UPDATE","Direct Weather Updates","10Micron Status",IP_RW,ISR_1OFMANY,0,IPS_IDLE);
  RefreshSP[0].fill("REFRESH","Read mount values",ISS_OFF);
  RefreshSP.fill(getDeviceName(),"NATIVE_REFRESH_CONTROLS","Refresh Controls","10Micron Status",IP_RW,ISR_ATMOST1,0,IPS_IDLE);
  RecoverySP[0].fill("MANUAL","Manual reconnect",ISS_ON);RecoverySP[1].fill("TELEMETRY","Reconnect telemetry only",ISS_OFF);
  RecoverySP.fill(getDeviceName(),"NATIVE_RECOVERY","Communication Recovery",OPTIONS_TAB,IP_RW,ISR_1OFMANY,0,IPS_OK);
  char savedLatch[32]{};
  parkUnsafe=IUGetConfigText(getDeviceName(),"NATIVE_PARK_SAFETY","STOP_INTERRUPTED",savedLatch,sizeof(savedLatch))==0 && !std::strcmp(savedLatch,"1");
  // The legacy INTERRUPTED flag was set during every normal unpark. It is
  // diagnostic compatibility only; only the explicit STOP record is restored.
  IUFillText(&ParkSafetyT[0],"INTERRUPTED","Known interrupted park",parkUnsafe?"1":"0");
  IUFillText(&ParkSafetyT[1],"STOP_INTERRUPTED","Park/unpark explicitly halted",parkUnsafe?"1":"0");
  IUFillTextVector(&ParkSafetyTP,ParkSafetyT,2,getDeviceName(),"NATIVE_PARK_SAFETY","Interrupted Motion Warning","Motion Safety",IP_RO,0,IPS_IDLE);
  SetParkDataType(PARK_NONE); // Native controller owns park position; no INDI file is authoritative.
  // Standard INDI policy: portable mounts default to ignoring an enclosure.
  // A named enclosure becomes mandatory only when the user enables Dome locks.
  // Telescope::initProperties() has already restored ACTIVE_DEVICES. Only
  // replace its simulator default when no saved Active Dome member exists.
  // A saved empty name is intentional and must remain empty too.
  char savedDome[MAXINDINAME]{};
  if(IUGetConfigText(getDeviceName(),"ACTIVE_DEVICES","ACTIVE_DOME",savedDome,sizeof(savedDome))!=0)
   ActiveDeviceTP[ACTIVE_DOME].setText("");
  IUFillText(&SafetyT[0],"INTERLOCK","Enclosure policy","Waiting for enclosure policy");
  IUFillText(&SafetyT[1],"COMMAND","Last motion command","None");
  IUFillTextVector(&SafetyTP,SafetyT,2,getDeviceName(),"NATIVE_MOTION_STATUS","Motion Diagnostics","Motion Safety",IP_RO,0,IPS_IDLE);
  IUFillText(&RoofStatusT[0],"INTERLOCK","Enclosure policy","Waiting for enclosure policy");
  IUFillTextVector(&RoofStatusTP,RoofStatusT,1,getDeviceName(),"NATIVE_ROOF_INTERLOCK","Roof Interlock","Motion Safety",IP_RO,0,IPS_IDLE);
  IUFillText(&CommandStatusT[0],"COMMAND","Last motion command","None");
  IUFillTextVector(&CommandStatusTP,CommandStatusT,1,getDeviceName(),"NATIVE_COMMAND_STATUS","Command Status","Motion Safety",IP_RO,0,IPS_IDLE);
  IUFillSwitch(&StopS[0],"STOP","Stop all",ISS_OFF);
  IUFillSwitchVector(&StopSP,StopS,1,getDeviceName(),"NATIVE_STOP_ALL","Stop Including Tracking","Motion Safety",IP_RW,ISR_ATMOST1,0,IPS_IDLE);
  IUFillSwitch(&SideS[0],"AUTO","Automatic",ISS_ON);
  IUFillSwitch(&SideS[1],"WEST","West",ISS_OFF);
  IUFillSwitch(&SideS[2],"EAST","East",ISS_OFF);
  IUFillSwitchVector(&SideSP,SideS,3,getDeviceName(),"NATIVE_SLEW_SIDE","Next Slew Side","10Micron Meridian",IP_RW,ISR_1OFMANY,0,IPS_IDLE);
  IUFillText(&EndpointT[0],"HOST","Mount IP","192.168.0.1");
  IUFillText(&EndpointT[1],"PORT","TCP Port","3490");
  IUFillTextVector(&EndpointTP,EndpointT,2,getDeviceName(),"NATIVE_ENDPOINT","Mount Network",CONNECTION_TAB,IP_RW,0,IPS_IDLE);
  IUFillSwitch(&TransportS[0],"TCP","TCP",ISS_ON);
  IUFillSwitch(&TransportS[1],"SERIAL","Serial",ISS_OFF);
  IUFillSwitchVector(&TransportSP,TransportS,2,getDeviceName(),"NATIVE_TRANSPORT","Connection Type",CONNECTION_TAB,IP_RW,ISR_1OFMANY,0,IPS_IDLE);
  IUFillText(&SerialT[0],"DEVICE","Serial device","/dev/ttyUSB0");
  IUFillText(&SerialT[1],"BAUD","Baud rate","9600");
  IUFillTextVector(&SerialTP,SerialT,2,getDeviceName(),"NATIVE_SERIAL","Serial Settings",CONNECTION_TAB,IP_RW,0,IPS_IDLE);
  ISState savedSerial=ISS_OFF;
  if(IUGetConfigSwitch(getDeviceName(),"NATIVE_TRANSPORT","SERIAL",&savedSerial)==0 && savedSerial==ISS_ON){TransportS[0].s=ISS_OFF;TransportS[1].s=ISS_ON;}
  for(auto &t:SerialT){char value[1024]{};if(IUGetConfigText(getDeviceName(),"NATIVE_SERIAL",t.name,value,sizeof(value))==0 && value[0])IUSaveText(&t,value);}
  // Restore before the client can request CONNECTION. This legacy property is
  // defined after Telescope::ISGetProperties(), too late for base auto-load.
  char savedHost[1024]{},savedPort[32]{};
  if(IUGetConfigText(getDeviceName(),"NATIVE_ENDPOINT","HOST",savedHost,sizeof(savedHost))==0 && savedHost[0])
   IUSaveText(&EndpointT[0],savedHost);
  if(IUGetConfigText(getDeviceName(),"NATIVE_ENDPOINT","PORT",savedPort,sizeof(savedPort))==0 && savedPort[0])
   IUSaveText(&EndpointT[1],savedPort);
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
  const char *sn[]={"VALID","SAMPLE_AGE_SEC","TRACK_END_ESTIMATE_SEC","ESTIMATE_UNCERTAINTY_SEC"};
  const char *sl[]={"Fresh native estimate (1=yes)","Estimate age (seconds)","Native tracking-end estimate (seconds)","Proposed rounding reserve (seconds)"};
  for(int i=0;i<4;i++)IUFillNumber(&ScheduleN[i],sn[i],sl[i],"%.3f",-1,1e12,0,i==3?60:-1);
  IUFillNumberVector(&ScheduleNP,ScheduleN,4,getDeviceName(),"NATIVE_SCHEDULING_DATA","Tracking Budget Telemetry","10Micron Meridian",IP_RO,0,IPS_IDLE);
  IUFillText(&ScheduleT[0],"VERSION","Contract version","1");
  IUFillText(&ScheduleT[1],"END_REASON","Estimate scope","Horizon or meridian; reason not supplied by Gmte");
  IUFillText(&ScheduleT[2],"FLIP_READY","Flip readiness","Not provided; FLIP acceptance is mount-authoritative");
  IUFillText(&ScheduleT[3],"CAPTURE_GATE","Capture admission","Not integrated; telemetry only");
  IUFillTextVector(&ScheduleTP,ScheduleT,4,getDeviceName(),"NATIVE_SCHEDULING_CONTRACT","Scheduling Contract","10Micron Meridian",IP_RO,0,IPS_OK);
  IUFillNumber(&FlipResultN[0],"CODE","0=idle 1=accepted 2=confirmed rejection 3=blocked/error", "%.0f",0,3,0,0);
  IUFillNumber(&FlipResultN[1],"SEQUENCE","Response sequence","%.0f",0,1e12,0,0);
  IUFillNumberVector(&FlipResultNP,FlipResultN,2,getDeviceName(),"NATIVE_FLIP_RESULT","Flip Response","10Micron Meridian",IP_RO,0,IPS_IDLE);
  setDefaultPollingPeriod(1000);setPollingPeriodRange(100,1000);addDebugControl();return true;
 }
 void ISGetProperties(const char *dev) override {
  INDI::Telescope::ISGetProperties(dev);defineProperty(RecoverySP);defineProperty(&EndpointTP);defineProperty(&TransportSP);defineProperty(&SerialTP);defineProperty(&ParkSafetyTP);
 }
 bool updateProperties() override {
  INDI::Telescope::updateProperties();
  INDI::GuiderInterface::updateProperties();
  if(isConnected()) {if(weatherModeSupported)defineProperty(WeatherModeSP);defineProperty(GuideRateNP);defineProperty(RefractionNP);defineProperty(RefreshSP);defineProperty(&IdentityTP);defineProperty(&StatusTP);defineProperty(&StateNP);defineProperty(&MeridianNP);defineProperty(&ScheduleNP);defineProperty(&ScheduleTP);defineProperty(&FlipResultNP);defineProperty(&SafetyTP);defineProperty(&RoofStatusTP);defineProperty(&CommandStatusTP);defineProperty(&StopSP);defineProperty(&SideSP);publish(lastState);publishSafety();snoopRoof();MeridianNP.s=IPS_IDLE;IDSetNumber(&MeridianNP,"Waiting for first meridian query.");}
  else {deleteProperty(WeatherModeSP);deleteProperty(GuideRateNP);deleteProperty(RefractionNP);deleteProperty(RefreshSP);deleteProperty(IdentityTP.name);deleteProperty(StatusTP.name);deleteProperty(StateNP.name);deleteProperty(MeridianNP.name);deleteProperty(ScheduleNP.name);deleteProperty(ScheduleTP.name);deleteProperty(FlipResultNP.name);deleteProperty(SafetyTP.name);deleteProperty(RoofStatusTP.name);deleteProperty(CommandStatusTP.name);deleteProperty(StopSP.name);deleteProperty(SideSP.name);}
  return true;
 }
 bool ISNewText(const char *dev,const char *name,char *texts[],char *names[],int n) override {
  if(dev && !std::strcmp(dev,getDeviceName()) && !std::strcmp(name,SerialTP.name)) {
   if(isConnected()){SerialTP.s=IPS_ALERT;IDSetText(&SerialTP,"Disconnect before changing serial settings.");return false;}
   if(IUUpdateText(&SerialTP,texts,names,n)<0)return false;
   SerialTP.s=IPS_OK;IDSetText(&SerialTP,nullptr);return true;
  }
  if(!std::strcmp(dev,getDeviceName()) && !std::strcmp(name,EndpointTP.name)) {
   if(isConnected()) {EndpointTP.s=IPS_ALERT;IDSetText(&EndpointTP,"Disconnect before changing the endpoint.");return false;}
   if(IUUpdateText(&EndpointTP,texts,names,n)<0) return false;
   EndpointTP.s=IPS_OK;IDSetText(&EndpointTP,nullptr);return true;
  }
  if(dev && !std::strcmp(dev,getDeviceName()) && !std::strcmp(name,ParkSafetyTP.name))return false; // Read-only, never clear from client/config callbacks.
  if(dev && !std::strcmp(dev,getDeviceName()) && ActiveDeviceTP.isNameMatch(name)) {
   if(isConnected()){ActiveDeviceTP.setState(IPS_ALERT);ActiveDeviceTP.apply("Disconnect before changing Active Dome.");return false;}
   enclosure=tenmicron::EnclosureState{};enclosureStopIssued=false;
  }
  return INDI::Telescope::ISNewText(dev,name,texts,names,n);
 }
 bool ISNewNumber(const char *dev,const char *name,double values[],char *names[],int n) override {
  if(dev && !std::strcmp(dev,getDeviceName())) {
   if(GuideNSNP.isNameMatch(name)||GuideWENP.isNameMatch(name)) {
    auto &property=GuideNSNP.isNameMatch(name)?GuideNSNP:GuideWENP;
    unsigned positive=0;
    for(int i=0;i<n;++i)for(int j=0;j<i;++j)if(!std::strcmp(names[i],names[j])){property.setState(IPS_ALERT);property.apply();return false;}
    for(int i=0;i<n;++i) {
     if(!std::isfinite(values[i])||values[i]<0||values[i]>9999||std::floor(values[i])!=values[i]||!property.findWidgetByName(names[i])){property.setState(IPS_ALERT);property.apply();return false;}
     if(values[i]>0)++positive;
    }
    // A zero vector is a standard no-op, not an abort or a failed correction.
    if(positive==0){property.update(values,names,n);property.apply();return true;}
    if(positive!=1){property.setState(IPS_ALERT);property.apply();return false;}
    // A partial vector must not inherit a previous pulse in the opposite direction.
    for(auto &v:property)v.setValue(0);
    return INDI::GuiderInterface::processNumber(dev,name,values,names,n);
   }
   if(GuideRateNP.isNameMatch(name)||RefractionNP.isNameMatch(name)) {
    auto &property=GuideRateNP.isNameMatch(name)?GuideRateNP:RefractionNP;
    if(!allowSettingsUpdate(GuideRateNP.isNameMatch(name))){property.setState(IPS_ALERT);property.apply();return false;}
    if(n<1)return false;
    for(int i=0;i<n;++i)for(int j=0;j<i;++j)if(!std::strcmp(names[i],names[j])){property.setState(IPS_ALERT);property.apply();return false;}
    double next[2]={property[0].getValue(),property.count()>1?property[1].getValue():0};
    for(int i=0;i<n;++i){auto w=property.findWidgetByName(names[i]);if(!w||!std::isfinite(values[i])||values[i]<w->getMin()||values[i]>w->getMax()){property.setState(IPS_ALERT);property.apply();return false;}next[w==&property[0]?0:1]=values[i];}
    if(property.count()>1 && property.getState()!=IPS_OK && n!=2){property.setState(IPS_ALERT);property.apply("Read mount values or supply both inputs first.");return false;}
    try {
     if(GuideRateNP.isNameMatch(name)){protocol.setGuideRate(next[0]);double actual=protocol.guideRate();GuideRateNP[0].setValue(actual);if(std::abs(actual-next[0])>0.02)throw tenmicron::SettingsRejected("Guide rate readback mismatch");}
     else {
      if(weatherModeSupported){
       refreshWeatherMode();
       if(weatherMode!=0){property.setState(IPS_ALERT);property.apply("Automatic direct-weather updates are active. Select Manual inputs in Direct Weather Updates before editing refraction inputs.");command("Refraction write blocked: automatic direct-weather updates own these inputs; mount mode unchanged",IPS_ALERT);return false;}
      }
      const bool writeTemperature=std::abs(next[0]-property[0].getValue())>1e-6;
      const bool writePressure=std::abs(next[1]-property[1].getValue())>1e-6;
      protocol.setRefraction(next[0],next[1],writeTemperature,writePressure);
      auto v=protocol.refraction();RefractionNP[0].setValue(v[0]);RefractionNP[1].setValue(v[1]);
      if((writeTemperature && std::abs(v[0]-next[0])>0.11)||(writePressure && std::abs(v[1]-next[1])>0.11))throw tenmicron::SettingsRejected("Refraction readback mismatch");
     }
     property.setState(IPS_OK);property.apply();return true;
    }catch(const tenmicron::SettingsRejected&e){
     // The mount replied completely: keep telemetry/park state and do not replay.
     // Recover actual values, including any first half of a partial transaction.
     try {
      if(GuideRateNP.isNameMatch(name))GuideRateNP[0].setValue(protocol.guideRate());
      else {auto v=protocol.refraction();RefractionNP[0].setValue(v[0]);RefractionNP[1].setValue(v[1]);}
      property.setState(IPS_ALERT);property.apply("%s; displaying mount readback.",e.what());
      command(std::string("Settings update failed: ")+e.what()+"; connection retained, no motion command sent",IPS_ALERT);
     }catch(const std::exception&readError){communicationFault(std::string(e.what())+"; readback failed: "+readError.what());}
     return false;
    }catch(const std::invalid_argument&e){property.setState(IPS_ALERT);property.apply("%s",e.what());return false;}
    catch(const std::exception&e){communicationFault(e.what());property.setState(IPS_ALERT);property.apply();return false;}
   }
  }
  if(dev && !std::strcmp(dev,getDeviceName()) && !std::strcmp(name,EqNP.getName())) {
   if(!isConnected()){LOG_ERROR("Connect before requesting a slew.");return false;}
  }
  return INDI::Telescope::ISNewNumber(dev,name,values,names,n);
 }
 bool ISNewSwitch(const char *dev,const char *name,ISState *states,char *names[],int n) override {
  if(dev && !std::strcmp(dev,getDeviceName()) && !std::strcmp(name,TransportSP.name)) {
   if(isConnected()){TransportSP.s=IPS_ALERT;IDSetSwitch(&TransportSP,"Disconnect before changing transport.");return false;}
   if(IUUpdateSwitch(&TransportSP,states,names,n)<0)return false;
   TransportSP.s=IPS_OK;IDSetSwitch(&TransportSP,nullptr);return true;
  }
  if(dev && !std::strcmp(dev,getDeviceName())) {
   if(WeatherModeSP.isNameMatch(name)) {
    int selected=-1;
    for(int i=0;i<n;++i){
     auto w=WeatherModeSP.findWidgetByName(names[i]);
     if(!w || (states[i]!=ISS_ON && states[i]!=ISS_OFF)){WeatherModeSP.setState(IPS_ALERT);WeatherModeSP.apply();return false;}
     for(int j=0;j<i;++j)if(!std::strcmp(names[i],names[j])){WeatherModeSP.setState(IPS_ALERT);WeatherModeSP.apply();return false;}
     if(states[i]==ISS_ON){if(selected!=-1){WeatherModeSP.setState(IPS_ALERT);WeatherModeSP.apply();return false;}for(size_t j=0;j<WeatherModeSP.count();++j)if(w==&WeatherModeSP[j])selected=static_cast<int>(j);}
    }
    if(selected<0 || !weatherModeSupported || !allowSettingsUpdate(false)){WeatherModeSP.setState(IPS_ALERT);WeatherModeSP.apply();return false;}
    try{
     protocol.setWeatherUpdateMode(selected);setWeatherModeReadback(selected);
     command("Direct-weather update mode changed; mount readback verified",IPS_OK);return true;
    }catch(const tenmicron::SettingsRejected&e){
     try{refreshWeatherMode();WeatherModeSP.setState(IPS_ALERT);WeatherModeSP.apply("%s; displaying mount mode.",e.what());command(std::string("Weather mode update failed: ")+e.what()+"; connection retained",IPS_ALERT);}
     catch(const std::exception&readError){communicationFault(std::string(e.what())+"; mode readback failed: "+readError.what());}
     return false;
    }catch(const std::exception&e){communicationFault(e.what());return false;}
   }
   if(TrackModeSP.isNameMatch(name)) {
    int selected=-1;
    for(int i=0;i<n;++i){auto w=TrackModeSP.findWidgetByName(names[i]);if(!w)return false;if(states[i]==ISS_ON){if(selected!=-1)return false;for(size_t j=0;j<TrackModeSP.count();++j)if(w==&TrackModeSP[j])selected=static_cast<int>(j);}}
    if(selected<0)return false;
    if(SetTrackMode(static_cast<uint8_t>(selected))){TrackModeSP.reset();TrackModeSP[selected].setState(ISS_ON);TrackModeSP.setState(IPS_OK);}
    else TrackModeSP.setState(IPS_ALERT);
    TrackModeSP.apply();return true;
   }
   if(RecoverySP.isNameMatch(name)){RecoverySP.update(states,names,n);RecoverySP.setState(IPS_OK);RecoverySP.apply();return true;}
   if(RefreshSP.isNameMatch(name)) {
    RefreshSP.reset();
    if(!healthy){RefreshSP.setState(IPS_ALERT);RefreshSP.apply();return false;}
    try{refreshControls();RefreshSP.setState(IPS_OK);RefreshSP.apply();return true;}
    catch(const std::exception&e){communicationFault(e.what());RefreshSP.setState(IPS_ALERT);RefreshSP.apply();return false;}
   }
   if(ParkSP.isNameMatch(name)) {
    bool wantPark=false,wantUnpark=false;
    for(int i=0;i<n;i++)if(states[i]==ISS_ON){if(!std::strcmp(names[i],"PARK"))wantPark=true;if(!std::strcmp(names[i],"UNPARK"))wantUnpark=true;}
    if(wantPark==wantUnpark)return false;
    bool ok=wantPark?Park():UnPark();
    if(ok){ParkSP.reset();ParkSP[wantPark?PARK:UNPARK].setState(ISS_ON);ParkSP.setState(progress.active()?IPS_BUSY:IPS_OK);}
    else {ParkSP.setState(IPS_ALERT);}
    ParkSP.apply();return true;
   }
   if(!std::strcmp(name,SideSP.name)){
    if(progress.active()){SideSP.s=IPS_ALERT;IDSetSwitch(&SideSP,"Wait for current motion to finish.");return false;}
    IUUpdateSwitch(&SideSP,states,names,n);SideSP.s=IPS_OK;IDSetSwitch(&SideSP,nullptr);return true;
   }
   if(!std::strcmp(name,StopSP.name)){
    bool requested=false;for(int i=0;i<n;i++)if(!std::strcmp(names[i],"STOP") && states[i]==ISS_ON)requested=true;
    if(requested && healthy){try{invalidateInterruptedPark();protocol.stopAll();cancelAuxiliary();progress.begin(tenmicron::MotionKind::StopAll);stopContext.clear();motionFault=false;command("STOP sent; awaiting native telemetry",IPS_BUSY);StopSP.s=IPS_BUSY;}catch(const std::exception&e){communicationFault(e.what());StopSP.s=IPS_ALERT;}}
    else if(requested)StopSP.s=IPS_ALERT;
    StopS[0].s=ISS_OFF;IDSetSwitch(&StopSP,nullptr);return true;
   }
  }
  // Enclosure policy changes cannot quietly remove a live motion interlock.
  if(dev && !std::strcmp(dev,getDeviceName()) && DomePolicySP.isNameMatch(name) && isConnected()) {
   DomePolicySP.setState(IPS_ALERT);DomePolicySP.apply("Disconnect before changing Dome Policy.");return false;
  }
  return INDI::Telescope::ISNewSwitch(dev,name,states,names,n);
 }
 bool ISSnoopDevice(XMLEle *root) override {
  const char *device=findXMLAttValu(root,"device"),*name=findXMLAttValu(root,"name");
  const bool selected=device && ActiveDeviceTP[ACTIVE_DOME].getText()[0] &&
      !std::strcmp(device,ActiveDeviceTP[ACTIVE_DOME].getText());
  if(selected) {
   const bool deleted=!std::strcmp(tagXMLEle(root),"delProperty");
   if(deleted) {
    if(initialEnclosurePending)initialEnclosureUnsafe=true;
    if(!name || !name[0] || !std::strcmp(name,"CONNECTION"))enclosure.reset();
    else if(!std::strcmp(name,"DOME_PARK"))enclosure.parkOpen=false;
    else if(!std::strcmp(name,"DOME_SHUTTER")){enclosure.shutterSeen=true;enclosure.shutterOpen=false;}
   } else if(name) {
    const char *state=findXMLAttValu(root,"state");
    const bool ok=state && !std::strcmp(state,"Ok");
    auto member=[root](const char *key,const char *value) {
     int matches=0;bool found=false;
     for(auto e=nextXMLEle(root,1);e;e=nextXMLEle(root,0)) {
      const char *en=findXMLAttValu(e,"name");
      if(en && !std::strcmp(en,key)){++matches;found=!std::strcmp(pcdataXMLEle(e),value);}
     }
     return matches==1 && found;
    };
    if(!std::strcmp(name,"CONNECTION")){
     const bool report=ok && member("CONNECT","On") && member("DISCONNECT","Off");
     if(initialEnclosurePending && !report && state && std::strcmp(state,"Idle"))initialEnclosureUnsafe=true;
     enclosure.connection(report);
    }else if(!std::strcmp(name,"DOME_PARK")){
     const bool report=ok && member("UNPARK","On") && member("PARK","Off");
     // RollOff ino initially defines the standard vector as Ok with neither
     // selection asserted, then publishes its sensor readback. This definition
     // is uninitialized, not positive closure/fault evidence. Only initial
     // discovery may wait for it, within the existing unchanged deadline.
     const bool emptyInitialDefinition=initialEnclosurePending && ok &&
         !std::strcmp(tagXMLEle(root),"defSwitchVector") &&
         member("PARK","Off") && member("UNPARK","Off");
     if(initialEnclosurePending && !report && !emptyInitialDefinition && state && std::strcmp(state,"Idle"))initialEnclosureUnsafe=true;
     enclosure.parkOpen=enclosure.connected && report;
    }else if(!std::strcmp(name,"DOME_SHUTTER")) {
     // A newly advertised optional shutter may follow the initial Park reply.
     // Its Idle definition is still discovery, within the original deadline.
     if(!enclosure.shutterSeen && state && !std::strcmp(state,"Idle") &&
        !safetyHold && !progress.active() && !auxiliaryActive() &&
        std::chrono::steady_clock::now()<initialEnclosureDeadline){
      initialEnclosurePending=true;initialEnclosureUnsafe=false;
     }
     const bool report=ok && member("SHUTTER_OPEN","On") && member("SHUTTER_CLOSE","Off");
     if(initialEnclosurePending && !report && state && std::strcmp(state,"Idle"))initialEnclosureUnsafe=true;
     enclosure.shutterSeen=true;
     enclosure.shutterOpen=enclosure.connected && report;
    }
   }
  }
  // Preserve INDI's standard snooping and policy API. Our observed connection /
  // vector validity checks also reject Busy, Alert, missing and deleted status.
  bool result=(device && name && std::strcmp(tagXMLEle(root),"delProperty"))?INDI::Telescope::ISSnoopDevice(root):true;
  if(selected){enforceEnclosure();if(isConnected())publishSafety();}
  return result;
 }
 protected:
 std::string transportDescription() const {
  return TransportS[1].s==ISS_ON ? std::string(SerialT[0].text)+"@"+SerialT[1].text : std::string(EndpointT[0].text)+":"+EndpointT[1].text;
 }
 void openTransport() {
  const bool serial=TransportS[1].s==ISS_ON;
  std::string text=serial?SerialT[1].text:EndpointT[1].text;
  size_t used=0;int value=std::stoi(text,&used);
  if(used!=text.size() || value<1 || (!serial && value>65535))throw std::runtime_error("Invalid transport rate or port");
  if(serial)protocol.connectSerial(SerialT[0].text,static_cast<unsigned>(value));
  else protocol.connect(EndpointT[0].text,static_cast<unsigned short>(value));
  activeSerial=serial;activeTransportAddress=transportDescription();
 }
 bool Connect() override {
  try {
   openTransport();
   protocol.ultraPrecision();
   auto product=protocol.product(), firmware=protocol.firmware(), controller=protocol.controller();
   // Require documented combined-state format (firmware 2.14.9 or later).
   int major=0,minor=0,patch=0;
   int parts=std::sscanf(firmware.c_str(),"%d.%d.%d",&major,&minor,&patch);
   if(parts<2 || major<2 || (major==2 && (minor<14 || (minor==14 && patch<9))))
    throw std::runtime_error("Requires firmware 2.14.9 or newer with documented Ginfo format");
   // Conservative compatibility gate: direct-weather commands verified against
   // MW4 and this project's controller at 3.4. This is not an introduction-version claim.
   weatherModeSupported=major>3 || (major==3 && minor>=4);
   weatherMode=-1;WeatherModeSP.reset();WeatherModeSP.setState(IPS_IDLE);
   if(weatherModeSupported)setWeatherModeReadback(protocol.weatherUpdateMode(),false);
   lastState=protocol.poll();
   refreshControls(); // Read supported controller settings; never restore writes.
   activeDome=ActiveDeviceTP[ACTIVE_DOME].getText();activeGps=ActiveDeviceTP[ACTIVE_GPS].getText();
   activeDomeIgnored=DomePolicySP[DOME_IGNORED].getState()==ISS_ON;
   IUSaveText(&IdentityT[0],product.c_str());IUSaveText(&IdentityT[1],firmware.c_str());IUSaveText(&IdentityT[2],controller.c_str());
   IdentityTP.s=IPS_OK;healthy=true;cancelAuxiliary();trackingRequestFailed=false;modeSelected=false;recoveryPending=false;progress.kind=tenmicron::MotionKind::None;motionFault=false;parkPublished=false;enclosure.reset();enclosureStopIssued=false;lastMeridianPoll=std::chrono::steady_clock::time_point{};meridianValid=false;SetTimer(getCurrentPollingPeriod());
   initialEnclosurePending=true;initialEnclosureUnsafe=false;
   initialEnclosureDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
   stopContext.clear();safetyHold=false;observedParking=false;StopSP.s=IPS_IDLE;StopS[0].s=ISS_OFF;FlipResultN[0].value=0;FlipResultNP.s=IPS_IDLE;
   SideS[0].s=ISS_ON;SideS[1].s=SideS[2].s=ISS_OFF;SideSP.s=IPS_IDLE;
   command("Native connection polled; motion admission awaits enclosure policy",IPS_OK);
   LOG_INFO("Native motion connection established; using standard INDI enclosure policy.");return true;
  }catch(const std::exception &e){protocol.disconnect();transportFailure();LOGF_ERROR("Connection failed: %s",e.what());return false;}
 }
 bool Disconnect() override {if(healthy&&(manual[0]||manual[1])){try{protocol.haltSlew();}catch(const std::exception&e){LOGF_ERROR("Manual halt on disconnect failed: %s",e.what());}}cancelAuxiliary();recoveryPending=false;meridianValid=false;SyncParkStatus(false);ParkSP.reset();ParkSP.setState(IPS_ALERT);protocol.disconnect();healthy=false;progress.kind=tenmicron::MotionKind::None;
  EndpointTP.s=SerialTP.s=TransportSP.s=IPS_IDLE;IDSetText(&EndpointTP,nullptr);IDSetText(&SerialTP,nullptr);IDSetSwitch(&TransportSP,nullptr);return true;}
 bool ReadScopeStatus() override {
  if(!healthy) {
   if(recoveryPending && RecoverySP[1].getState()==ISS_ON && std::chrono::steady_clock::now()>=nextRecovery){
    nextRecovery=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    try{openTransport();protocol.ultraPrecision();if(protocol.product()!=IdentityT[0].text||protocol.firmware()!=IdentityT[1].text||protocol.controller()!=IdentityT[2].text)throw std::runtime_error("Reconnect identity changed; deliberate reconnect required");lastState=protocol.poll();healthy=true;recoveryPending=false;meridianValid=false;enclosure.reset();snoopRoof();safetyHold=true;motionFault=true;command("Telemetry reconnected; motion held. No command replay or automatic capture recovery.",IPS_ALERT);}
    catch(const std::exception&e){protocol.disconnect();LOGF_WARN("Telemetry reconnect failed: %s",e.what());return false;}
   }else return false;
  }
  const char *pollPhase="core telemetry";
  try {
   lastState=protocol.poll();
   enforceEnclosure();
   if(!healthy)return false;
   publish(lastState);finishGuides();publishSafety();
   auto snoopNow=std::chrono::steady_clock::now();
   if(snoopNow-lastRoofRequest>=std::chrono::seconds(initialEnclosurePending?1:5)){snoopRoof();lastRoofRequest=snoopNow;}
   auto now=std::chrono::steady_clock::now();
   if(now-lastMeridianPoll>=std::chrono::seconds(10)) {
    pollPhase="meridian telemetry";auto m=protocol.meridian();
    double data[]={static_cast<double>(m.slewLimitDegrees),static_cast<double>(m.trackLimitDegrees),static_cast<double>(m.timeToEndMinutes),static_cast<double>(m.allowedSide),static_cast<double>(m.unattendedFlip),static_cast<double>(m.targetDestinationSide)};
    for(int i=0;i<6;i++) MeridianN[i].value=data[i];
    MeridianNP.s=IPS_OK;IDSetNumber(&MeridianNP,nullptr);lastMeridianPoll=std::chrono::steady_clock::now();meridianValid=true;
   }
   if(now-lastControlsPoll>=std::chrono::seconds(10) && !progress.active() && !auxiliaryActive()){
    pollPhase="control readback";refreshControls();
   }
   publishScheduling();
   return true;
  }
  catch(const std::exception &e){communicationFault(std::string(pollPhase)+": "+e.what());return false;}
 }
 bool Goto(double ra,double dec) override {
  if(!allowMotion("Slew"))return false;
  // Acknowledgement can take longer than Ekos' status poll. Publish pending
  // before blocking on the protocol so a previous Alert is not reused as the
  // result of this new request. Rejection/exception still follows the normal
  // INDI failure path; Busy does not claim native acceptance or completion.
  EqNP.setState(IPS_BUSY);EqNP.apply();
  try {
   int side=SideS[1].s==ISS_ON?2:SideS[2].s==ISS_ON?3:0;
   auto result=protocol.slew(ra,dec,side);
   // Side choice is one-shot, so an ordinary future slew cannot accidentally force a flip.
   SideS[0].s=ISS_ON;SideS[1].s=SideS[2].s=ISS_OFF;SideSP.s=IPS_OK;IDSetSwitch(&SideSP,nullptr);
   if(!result.accepted()){motionFault=true;command("Slew rejected ("+std::to_string(result.code)+"): "+result.message,IPS_ALERT);return false;}
   motionFault=false;progress.begin(tenmicron::MotionKind::Slew,ra,dec);TrackState=SCOPE_SLEWING;
   command(result.message,IPS_BUSY);return true;
  }catch(const std::invalid_argument&e){command(e.what(),IPS_ALERT);return false;}
   catch(const std::exception&e){communicationFault(e.what());return false;}
 }
 bool Flip(double ra,double dec) override {
  if(!allowMotion("Flip")){flipResponse(3);return false;}
  try{
   auto result=protocol.requestFlip(ra,dec);
   if(!result.accepted){
    if(result.confirmedRejection){
     motionFault=false;flipResponse(2);command(result.message,IPS_OK);return true;
    }
    flipResponse(3);motionFault=true;EqNP.setState(IPS_ALERT);EqNP.apply();command(result.message,IPS_ALERT);return false;
   }
   flipResponse(1);
   progress.beginFlip(result.before);lastState=result.before;motionFault=false;TrackState=SCOPE_SLEWING;
   command("Flip accepted; awaiting opposite native pier side, tracking and settled target",IPS_BUSY);return true;
  }catch(const std::invalid_argument&e){command(e.what(),IPS_ALERT);return false;}
   catch(const std::exception&e){communicationFault(e.what());return false;}
 }
 bool Sync(double ra,double dec) override {
  if(!allowMotion("Offset sync"))return false;
  try {
   auto result=protocol.syncOffset(ra,dec);
   if(!result.accepted()){command("Offset sync rejected: "+result.message,IPS_ALERT);return false;}
   // Read actual coordinates immediately; no fabricated coordinate publication.
   lastState=protocol.poll();motionFault=false;publish(lastState);
   command(result.message,IPS_OK);return true;
  }catch(const std::invalid_argument&e){command(e.what(),IPS_ALERT);return false;}
   catch(const std::exception&e){communicationFault(e.what());return false;}
 }
 bool Park() override {
  if(!isConnected() || !healthy){command("Park unavailable: native telemetry is not current",IPS_ALERT);return false;}
  try{auto state=protocol.poll();
   // A tracking request is not a slew. Explicit Park may cancel it only after
   // fresh, settled native truth and the normal enclosure/auxiliary guards.
   if(progress.trackingTransition() && tenmicron::settledTrackingState(state) && roofReady() && !auxiliaryActive()){
    trackingRequested=progress.kind==tenmicron::MotionKind::TrackingOn;
    trackingRequestFailed=true;progress.kind=tenmicron::MotionKind::None;
    lastState=state;publish(state);
    command("Pending tracking request canceled by explicit Park; settled native state verified",IPS_ALERT);
   }
   if(state.nativeStatus==5 && !state.slewOrSettling && !progress.active()){
    publish(state);
    if(parkUnsafe){command("Native parked; known halted park still requires recovery before roof closure",IPS_ALERT);return false;}
    command("Native telemetry confirms already parked",IPS_OK);return true;
   }
   if(!allowMotion("Park",true))return false;
   if(state.nativeStatus!=0 && state.nativeStatus!=1 && state.nativeStatus!=7){command("Park blocked by native state",IPS_ALERT);return false;}

   protocol.park();motionFault=false;progress.begin(tenmicron::MotionKind::Park);TrackState=SCOPE_PARKING;command("Park requested; awaiting native parked state",IPS_BUSY);return true;
  }catch(const std::exception&e){communicationFault(e.what());return false;}
 }
 bool UnPark() override {
  if(!allowMotion("Unpark"))return false;
  try{auto state=protocol.poll();
   if(!state.slewOrSettling && (state.nativeStatus==0 || state.nativeStatus==1 || state.nativeStatus==7)) {
    lastState=state;publish(state);command("Native telemetry confirms already unparked and settled",IPS_OK);return true;
   }
   if(state.nativeStatus!=5 || state.slewOrSettling){command("Unpark requires native parked or settled unparked state",IPS_ALERT);return false;}

   SyncParkStatus(false); // Locks the dome before PO can initiate movement.
   protocol.unpark();motionFault=false;progress.begin(tenmicron::MotionKind::Unpark);TrackState=SCOPE_PARKING;command("Unpark requested; awaiting native state",IPS_BUSY);return true;
  }catch(const std::exception&e){communicationFault(e.what());return false;}
 }
 bool Abort() override {
  if(!healthy)return false;
  try{invalidateInterruptedPark();protocol.haltSlew();cancelAuxiliary();progress.begin(tenmicron::MotionKind::Halt);motionFault=false;command("Slew halt sent; awaiting native telemetry; tracking may remain on",IPS_BUSY);return true;}
  catch(const std::exception&e){communicationFault(e.what());return false;}
 }
 bool SetTrackEnabled(bool enabled) override {
  if(enabled && !allowMotion("Tracking on"))return false;
  if(!healthy || progress.active() || auxiliaryActive())return false;
  try{auto state=protocol.poll();if(enabled && state.nativeStatus!=0 && state.nativeStatus!=1 && state.nativeStatus!=7){command("Tracking blocked by native state",IPS_ALERT);return false;}
   if(enabled && modeSelected)protocol.trackMode(TrackModeSP.findOnSwitchIndex(),TrackRateNP[0].getValue(),TrackRateNP[1].getValue());
   protocol.setTracking(enabled);trackingRequestFailed=false;trackingRequested=enabled;motionFault=false;progress.begin(enabled?tenmicron::MotionKind::TrackingOn:tenmicron::MotionKind::TrackingOff);command("Tracking request sent; awaiting native state",IPS_BUSY);return true;
  }catch(const std::exception&e){communicationFault(e.what());return false;}
 }
 bool saveConfigItems(FILE *fp) override {INDI::Telescope::saveConfigItems(fp);IUSaveConfigText(fp,&EndpointTP);IUSaveConfigText(fp,&SerialTP);IUSaveConfigSwitch(fp,&TransportSP);IUSaveConfigText(fp,&ParkSafetyTP);RecoverySP.save(fp);return true;}
 protected:
 IPState GuideNorth(uint32_t ms) override{return guidePulse('n',ms,1);}
 IPState GuideSouth(uint32_t ms) override{return guidePulse('s',ms,1);}
 IPState GuideEast(uint32_t ms) override{return guidePulse('e',ms,0);}
 IPState GuideWest(uint32_t ms) override{return guidePulse('w',ms,0);}
 bool SetSlewRate(int index) override {
  if(!allowMotion("Slew rate"))return false;
  try{protocol.slewRate(index);return true;}catch(const std::exception&e){communicationFault(e.what());return false;}
 }
 bool MoveNS(INDI_DIR_NS d,TelescopeMotionCommand c) override{return axisMove(d==DIRECTION_NORTH?'n':'s',c==MOTION_START,1);}
 bool MoveWE(INDI_DIR_WE d,TelescopeMotionCommand c) override{return axisMove(d==DIRECTION_WEST?'w':'e',c==MOTION_START,0);}
 bool SetTrackMode(uint8_t mode) override {
  if(!allowMotion("Tracking mode"))return false;
  try{protocol.trackMode(mode,TrackRateNP[0].getValue(),TrackRateNP[1].getValue());modeSelected=true;return true;}
  catch(const std::invalid_argument&e){LOGF_ERROR("%s",e.what());return false;}catch(const std::exception&e){communicationFault(e.what());return false;}
 }
 bool SetTrackRate(double ra,double dec) override {
  if(!allowMotion("Tracking rates"))return false;
  try{protocol.trackRate(ra,dec);return true;}catch(const std::invalid_argument&e){LOGF_ERROR("%s",e.what());return false;}catch(const std::exception&e){communicationFault(e.what());return false;}
 }
 bool updateTime(ln_date *utc,double offset) override {
  if(!utc||!std::isfinite(offset)||offset<-14||offset>14||!allowConfiguration("UTC update"))return false;
  if(utc->years<2000||utc->years>2099||utc->months<1||utc->months>12||utc->days<1||utc->days>31||utc->hours<0||utc->hours>23||utc->minutes<0||utc->minutes>59||!std::isfinite(utc->seconds)||utc->seconds<0||utc->seconds>=60)return false;
  char text[32];std::snprintf(text,sizeof(text),"%04d-%02d-%02d,%02d:%02d:%02d",utc->years,utc->months,utc->days,utc->hours,utc->minutes,static_cast<int>(utc->seconds));
  try{protocol.setUTC(text,offset);lastState=protocol.poll();if(std::abs(lastState.julianDate-ln_get_julian_day(utc))*86400>3)throw std::runtime_error("UTC readback mismatch");meridianValid=false;return true;}catch(const std::invalid_argument&e){LOGF_ERROR("%s",e.what());return false;}catch(const std::exception&e){communicationFault(e.what());return false;}
 }
 bool updateLocation(double latitude,double longitude,double elevation) override {
  if(!allowConfiguration("Site update"))return false;
  try{protocol.setLocation(latitude,longitude,elevation);auto v=protocol.location();if(std::abs(v[0]-latitude)>0.0001||std::abs(std::remainder(v[1]-longitude,360))>0.0001||std::abs(v[2]-elevation)>0.11)throw std::runtime_error("Site readback mismatch");meridianValid=false;return true;}
  catch(const std::invalid_argument&e){LOGF_ERROR("%s",e.what());return false;}catch(const std::exception&e){communicationFault(e.what());return false;}
 }
 private:
 bool auxiliaryActive()const{return manual[0]||manual[1]||pulseActive[0]||pulseActive[1];}
 // Settings writes do not initiate motion and do not require enclosure permission.
 // Serialize motion conflicts; guide-rate writes also wait for active pulses.
 bool allowSettingsUpdate(bool guideRateWrite){
  finishGuides(); // Retire elapsed pulses even if a timer callback is queued behind this request.
  if(!healthy || !isConnected() || progress.active() || manual[0] || manual[1] ||
     (guideRateWrite && (pulseActive[0] || pulseActive[1])) || safetyHold){
   command("Settings update blocked: require healthy connection, no conflicting operation and no recovery hold (guide-rate changes wait for active pulses)",IPS_ALERT);
   return false;
  }
  try {
   const auto state=protocol.poll();
   const bool settledState=state.nativeStatus==0 || state.nativeStatus==1 ||
                           state.nativeStatus==5 || state.nativeStatus==7;
   if(state.slewOrSettling || !settledState){
    command("Settings update blocked: native mount must be settled and in a normal tracking, stopped or parked state",IPS_ALERT);
    return false;
   }
   return true;
  }catch(const std::exception &e){communicationFault(e.what());return false;}
 }
 bool allowConfiguration(const char *action){
  if(!healthy||!isConnected()||progress.active()||auxiliaryActive()||safetyHold)return false;
  try{auto state=protocol.poll();if(state.slewOrSettling||(state.nativeStatus!=1&&state.nativeStatus!=5&&state.nativeStatus!=7)){command(std::string(action)+" requires stationary mount with tracking off",IPS_ALERT);return false;}return true;}catch(const std::exception&e){communicationFault(e.what());return false;}
 }
 void cancelAuxiliary(){
  manual[0]=manual[1]=0;
  for(int i=0;i<2;++i){if(pulseTimer[i]){IERmTimer(pulseTimer[i]);pulseTimer[i]=0;}if(pulseActive[i]){auto &v=i?GuideNSNP:GuideWENP;v.setState(IPS_ALERT);v.apply();}pulseActive[i]=false;}
  MovementNSSP.reset();MovementNSSP.setState(IPS_IDLE);MovementNSSP.apply();
  MovementWESP.reset();MovementWESP.setState(IPS_IDLE);MovementWESP.apply();
 }
 static void guideTimerRA(void *p){static_cast<TenMicronMotion*>(p)->guideTimerExpired(0);}
 static void guideTimerDE(void *p){static_cast<TenMicronMotion*>(p)->guideTimerExpired(1);}
 void armGuideTimer(int axis,int ms){
  pulseTimer[axis]=IEAddTimer(ms,axis?guideTimerDE:guideTimerRA,this);
 }
 void guideTimerExpired(int axis){
  pulseTimer[axis]=0;
  // Event-loop timers can fire before the steady-clock deadline after rounding.
  const auto remaining=std::chrono::duration<double,std::milli>(pulseEnd[axis]-std::chrono::steady_clock::now()).count();
  if(pulseActive[axis] && remaining>0){armGuideTimer(axis,static_cast<int>(std::ceil(remaining)));return;}
  finishGuides();
 }
 void finishGuides(){
  for(int i=0;i<2;++i)if(pulseActive[i] && std::chrono::steady_clock::now()>=pulseEnd[i]){
   if(pulseTimer[i]){IERmTimer(pulseTimer[i]);pulseTimer[i]=0;}
   pulseActive[i]=false;auto &v=i?GuideNSNP:GuideWENP;for(auto &n:v)n.setValue(0);
   // Like the established LX200 pulse path, this reports elapsed command time,
   // not a plate-solve result or a claim that a global slew/settling poll completed.
   if(healthy && isConnected() && !safetyHold)GuideComplete(i?AXIS_DE:AXIS_RA);
   else {v.setState(IPS_ALERT);v.apply("Pulse interval elapsed with invalid connection or recovery hold.");}
  }
 }
 IPState guidePulse(char direction,uint32_t ms,int axis){
  finishGuides();
  if(!healthy||!isConnected()||!roofReady()||safetyHold||progress.active()||manual[0]||manual[1]||pulseActive[axis])return IPS_ALERT;
  try{protocol.guide(direction,ms);pulseActive[axis]=true;pulseEnd[axis]=std::chrono::steady_clock::now()+std::chrono::milliseconds(ms);armGuideTimer(axis,static_cast<int>(ms));return IPS_BUSY;}
  catch(const std::invalid_argument&e){LOGF_ERROR("%s",e.what());return IPS_ALERT;}catch(const std::exception&e){communicationFault(e.what());return IPS_ALERT;}
 }
 bool axisMove(char direction,bool start,int axis){
  if(!healthy||!isConnected())return false;
  if(start && (!roofReady()||safetyHold||progress.active()||pulseActive[0]||pulseActive[1]||manual[axis]))return false;
  try{
   if(start){auto state=protocol.poll();if((!manual[0]&&!manual[1]&&(state.slewOrSettling||(state.nativeStatus!=0&&state.nativeStatus!=1&&state.nativeStatus!=7))))return false;
    // Assert the selected manual speed for every move; controller state is shared.
    protocol.slewRate(SlewRateSP.findOnSwitchIndex());protocol.move(direction,true);manual[axis]=direction;
   }else {protocol.move(manual[axis]?manual[axis]:direction,false);manual[axis]=0;}
   return true;
  }catch(const std::exception&e){communicationFault(e.what());return false;}
 }
 void setWeatherModeReadback(int mode,bool emit=true){
  weatherMode=mode;WeatherModeSP.reset();WeatherModeSP[mode].setState(ISS_ON);WeatherModeSP.setState(IPS_OK);
  lastWeatherModePoll=std::chrono::steady_clock::now();
  if(emit)WeatherModeSP.apply();
 }
 void refreshWeatherMode(){setWeatherModeReadback(protocol.weatherUpdateMode());}
 void refreshControls(){
  if(weatherModeSupported)refreshWeatherMode();
  GuideRateNP[0].setValue(protocol.guideRate());GuideRateNP.setState(IPS_OK);GuideRateNP.apply();
  auto r=protocol.refraction();RefractionNP[0].setValue(r[0]);RefractionNP[1].setValue(r[1]);RefractionNP.setState(IPS_OK);RefractionNP.apply();
  auto l=protocol.location();for(int i=0;i<3;++i)LocationNP[i].setValue(l[i]);LocationNP.setState(IPS_OK);LocationNP.apply();
  siteReadbackValid=true;localUTCOffset=protocol.utcOffset();timeReadbackValid=true;
  lastControlsPoll=std::chrono::steady_clock::now();
 }
 void publishClock(const tenmicron::State &s){
  if(!healthy || !timeReadbackValid)return;
  ln_date utc{};ln_get_date(s.julianDate,&utc);
  char text[48];std::snprintf(text,sizeof(text),"%04d-%02d-%02dT%02d:%02d:%02d",utc.years,utc.months,utc.days,utc.hours,utc.minutes,static_cast<int>(utc.seconds));
  TimeTP[UTC].setText(text);std::snprintf(text,sizeof(text),"%.4f",localUTCOffset);
  TimeTP[OFFSET].setText(text);TimeTP.setState(IPS_OK);TimeTP.apply();
  if(siteReadbackValid && LocationNP.getState()==IPS_ALERT){LocationNP.setState(IPS_OK);LocationNP.apply();}
 }
 char manual[2]{};
 int pulseTimer[2]{};
 bool pulseActive[2]{},modeSelected=false,recoveryPending=false;
 bool trackingRequestFailed=false,trackingRequested=false;
 bool activeSerial=false,activeDomeIgnored=true;
 std::string activeTransportAddress,activeDome,activeGps;
 std::chrono::steady_clock::time_point pulseEnd[2]{},nextRecovery{};
 INDI::PropertyNumber GuideRateNP{1},RefractionNP{2};
 INDI::PropertySwitch RefreshSP{1},RecoverySP{2},WeatherModeSP{3};
 bool weatherModeSupported=false;
 int weatherMode=-1;
 std::chrono::steady_clock::time_point lastWeatherModePoll{},lastControlsPoll{};
 bool timeReadbackValid=false,siteReadbackValid=false;
 double localUTCOffset=0;
 void flipResponse(int code){FlipResultN[0].value=code;FlipResultN[1].value+=1;FlipResultNP.s=code==3?IPS_ALERT:IPS_OK;IDSetNumber(&FlipResultNP,nullptr);}
 void command(const std::string &text,IPState state){
  IUSaveText(&SafetyT[1],text.c_str());SafetyTP.s=state;IDSetText(&SafetyTP,nullptr);
  IUSaveText(&CommandStatusT[0],text.c_str());CommandStatusTP.s=state;IDSetText(&CommandStatusTP,nullptr);
  LOGF_INFO("%s",text.c_str());
 }
 // These properties describe configuration, not the physical roof state.
 // Rejected connected edits never replace their applied values; fresh native
 // traffic validates only the active transport. An inactive transport is Idle.
 void reconcileConfiguration(){
  if(!healthy || !isConnected())return;
  const bool sameTransport=(TransportS[1].s==ISS_ON)==activeSerial && transportDescription()==activeTransportAddress;
  const IPState transportState=sameTransport?IPS_OK:IPS_ALERT;
  if(TransportSP.s!=transportState){TransportSP.s=transportState;IDSetSwitch(&TransportSP,nullptr);}
  const IPState networkState=activeSerial?IPS_IDLE:sameTransport?IPS_OK:IPS_ALERT;
  const IPState serialState=!activeSerial?IPS_IDLE:sameTransport?IPS_OK:IPS_ALERT;
  if(EndpointTP.s!=networkState){EndpointTP.s=networkState;IDSetText(&EndpointTP,nullptr);}
  if(SerialTP.s!=serialState){SerialTP.s=serialState;IDSetText(&SerialTP,nullptr);}
  const bool sameNames=activeDome==ActiveDeviceTP[ACTIVE_DOME].getText() && activeGps==ActiveDeviceTP[ACTIVE_GPS].getText();
  const IPState namesState=sameNames && (activeDomeIgnored || !activeDome.empty())?IPS_OK:IPS_ALERT;
  if(ActiveDeviceTP.getState()!=namesState){ActiveDeviceTP.setState(namesState);ActiveDeviceTP.apply();}
  const bool ignored=DomePolicySP[DOME_IGNORED].getState()==ISS_ON;
  const bool locks=DomePolicySP[DOME_LOCKS].getState()==ISS_ON;
  const IPState policyState=ignored!=locks && ignored==activeDomeIgnored?IPS_OK:IPS_ALERT;
  if(DomePolicySP.getState()!=policyState){DomePolicySP.setState(policyState);DomePolicySP.apply();}
 }
 void transportFailure(){
  TransportSP.s=IPS_ALERT;IDSetSwitch(&TransportSP,nullptr);
  if(TransportS[1].s==ISS_ON){SerialTP.s=IPS_ALERT;IDSetText(&SerialTP,nullptr);}
  else {EndpointTP.s=IPS_ALERT;IDSetText(&EndpointTP,nullptr);}
 }
 bool awaitingInitialEnclosure()const {
  return initialEnclosurePending && !initialEnclosureUnsafe &&
         ActiveDeviceTP[ACTIVE_DOME].getText()[0] &&
         std::chrono::steady_clock::now()<initialEnclosureDeadline;
 }
 void publishSafety(){
  const bool ready=roofReady();
  const bool awaiting=!ready && awaitingInitialEnclosure();
  const char *text=DomePolicySP[DOME_IGNORED].getState()==ISS_ON?
      "Dome ignored: no enclosure interlock requested":ready?
      "Dome locks: connected enclosure reports unparked/open":
      awaiting?"Awaiting initial enclosure reports; new motion remains blocked":
      "Blocked: configured enclosure must be connected, unparked and open";
  IUSaveText(&SafetyT[0],text);IDSetText(&SafetyTP,nullptr);
  IUSaveText(&RoofStatusT[0],text);RoofStatusTP.s=ready?IPS_OK:awaiting?IPS_BUSY:IPS_ALERT;IDSetText(&RoofStatusTP,nullptr);
 }
 bool roofReady()const {
  return enclosure.permits(DomePolicySP[DOME_IGNORED].getState()==ISS_ON,
                           ActiveDeviceTP[ACTIVE_DOME].getText()[0]!=0);
 }
 void snoopRoof(){
  const char *device=ActiveDeviceTP[ACTIVE_DOME].getText();
  if(device[0])for(const char *name:{"CONNECTION","DOME_PARK","DOME_SHUTTER"})IDSnoopDevice(device,name);
 }
 bool mountActive()const{return auxiliaryActive() || progress.active() || lastState.slewOrSettling || lastState.nativeStatus==0 || lastState.nativeStatus==2 || lastState.nativeStatus==3 || lastState.nativeStatus==4 || lastState.nativeStatus==6 || lastState.nativeStatus==9 || lastState.nativeStatus==10;}
 void enforceEnclosure(){
  if(roofReady()){initialEnclosurePending=false;enclosureStopIssued=false;return;}
  // Initial silence is not a report of a closed/failed roof. Bound this wait
  // to five seconds; any explicit unsafe report bypasses it immediately.
  if(awaitingInitialEnclosure())return;
  initialEnclosurePending=false;
  if(healthy && mountActive() && !enclosureStopIssued){
   enclosureStopIssued=true;stopForSafety("Configured enclosure confirmation lost while mount active");
  }
 }
 bool allowMotion(const char *action,bool recoveryPark=false){
  if(!healthy || !isConnected() || !roofReady() || progress.active() || auxiliaryActive() || (safetyHold && !recoveryPark)) {
   command(std::string(action)+" blocked: require healthy native connection, permitted enclosure and no pending operation; safety STOP requires reconnect or completed recovery park",IPS_ALERT);return false;
  }
  return true;
 }
 void publishScheduling(){
  const auto age=meridianValid?std::chrono::duration<double>(std::chrono::steady_clock::now()-lastMeridianPoll).count():-1.0;
  const bool valid=healthy && meridianValid && age>=0 && age<15;
  ScheduleN[0].value=valid?1:0;ScheduleN[1].value=age;
  ScheduleN[2].value=valid?MeridianN[2].value*60:-1;
  ScheduleNP.s=valid?IPS_OK:IPS_ALERT;IDSetNumber(&ScheduleNP,nullptr);
 }
 void communicationFault(const std::string &reason){
  initialEnclosurePending=false;timeReadbackValid=siteReadbackValid=false;
  TimeTP.setState(IPS_ALERT);TimeTP.apply();LocationNP.setState(IPS_ALERT);LocationNP.apply();
  transportFailure();
  weatherMode=-1;WeatherModeSP.reset();WeatherModeSP.setState(IPS_ALERT);if(weatherModeSupported)WeatherModeSP.apply();
  SyncParkStatus(false);ParkSP.reset(); // Neither parked nor unparked is known. Dome stays locked.
  cancelAuxiliary();protocol.disconnect();healthy=false;recoveryPending=true;nextRecovery=std::chrono::steady_clock::now()+std::chrono::seconds(10);meridianValid=false;publishScheduling();
  GuideRateNP.setState(IPS_ALERT);GuideRateNP.apply();RefractionNP.setState(IPS_ALERT);RefractionNP.apply();progress.kind=tenmicron::MotionKind::None;motionFault=true;
  MeridianNP.s=StateNP.s=StatusTP.s=IPS_ALERT;IDSetNumber(&MeridianNP,nullptr);IDSetNumber(&StateNP,nullptr);
  StateN[2].value=StateN[3].value=-1;IDSetNumber(&StateNP,nullptr);
  IUSaveText(&StatusT[0],"Unknown; last sample stale");IUSaveText(&StatusT[1],reason.c_str());IDSetText(&StatusTP,nullptr);EqNP.setState(IPS_ALERT);EqNP.apply();ParkSP.setState(IPS_ALERT);ParkSP.apply();TrackStateSP.setState(IPS_ALERT);TrackStateSP.apply();
  command("Communication failure; motion outcome uncertain. endpoint="+transportDescription()+"; reconnect and inspect: "+reason,IPS_ALERT);
 }
 bool latchPark(bool value){
  bool old=parkUnsafe;parkUnsafe=value;for(auto &t:ParkSafetyT)IUSaveText(&t,value?"1":"0");
  if(!saveConfig(true,ParkSafetyTP.name)){parkUnsafe=old || value;for(auto &t:ParkSafetyT)IUSaveText(&t,parkUnsafe?"1":"0");ParkSafetyTP.s=IPS_ALERT;IDSetText(&ParkSafetyTP,"Cannot persist park safety state.");return false;}
  ParkSafetyTP.s=value?IPS_ALERT:IPS_OK;IDSetText(&ParkSafetyTP,nullptr);return true;
 }
 void invalidateInterruptedPark(){
  // Record an actual halt, never a normal command or a connection failure.
  if(progress.kind==tenmicron::MotionKind::Park || progress.kind==tenmicron::MotionKind::Unpark || lastState.nativeStatus==2 || lastState.nativeStatus==3){
   latchPark(true);observedParking=false;SyncParkStatus(false);
  }
 }
 void stopForSafety(const std::string &reason){
  safetyHold=true;
  try{invalidateInterruptedPark();protocol.stopAll();cancelAuxiliary();progress.begin(tenmicron::MotionKind::StopAll);stopContext=reason;motionFault=true;command(reason+"; STOP sent, awaiting native telemetry; inspect native state",IPS_ALERT);StopSP.s=IPS_BUSY;IDSetSwitch(&StopSP,nullptr);}
  catch(const std::exception&e){communicationFault(reason+"; STOP failed: "+e.what());}
 }
 void publish(const tenmicron::State &s) {
  reconcileConfiguration();publishClock(s);
  auto previous=progress.kind;
  auto outcome=progress.observe(s);
  // Polling sees other clients' actions too. A subsequent native parking
  // sequence can resolve a previously observed explicit interruption.
  if(s.nativeStatus==2){
   // An in-flight STOP/Halt may still return a pre-stop parking sample. That
   // is not evidence of a new external recovery sequence.
   observedParking=previous!=tenmicron::MotionKind::StopAll && previous!=tenmicron::MotionKind::Halt;
  }
  else if(s.nativeStatus==5 && !s.slewOrSettling){
   if(observedParking && parkUnsafe)latchPark(false);
   // A complete externally commanded recovery park is still real recovery.
   // Require its observed parking sequence, healthy roof proof and no known
   // interrupted-park latch; a static PARK sample alone never clears a hold.
   if(observedParking && previous==tenmicron::MotionKind::None && safetyHold &&
      !parkUnsafe && roofReady()){
    safetyHold=false;motionFault=false;stopContext.clear();StopSP.s=IPS_IDLE;IDSetSwitch(&StopSP,nullptr);
    command("Native recovery park observed; safety hold cleared",IPS_OK);
   }
   observedParking=false;
  }else if(s.nativeStatus!=6)observedParking=false;
  // An external park supersedes our active slew/flip without replaying or
  // stopping a completed park. The command fails; the physical state is true.
  if(s.nativeStatus==5 && !s.slewOrSettling &&
     (previous==tenmicron::MotionKind::Slew || previous==tenmicron::MotionKind::Flip ||
      previous==tenmicron::MotionKind::TrackingOn)){
   progress.kind=tenmicron::MotionKind::None;motionFault=true;outcome=tenmicron::MotionOutcome::None;
   command("Active operation superseded by native parked state; inspect native state",IPS_ALERT);
  }
  const bool unparkLimitRecovery=previous==tenmicron::MotionKind::Unpark &&
      s.nativeStatus==9 && outcome==tenmicron::MotionOutcome::Waiting;
  if(unparkLimitRecovery && !s.slewOrSettling && !progress.unparkTrackingOffRequested){
   // Do not resume capture or admit another motion command at native status 9.
   // Disable tracking once, retaining the original completion deadline.
   protocol.setTracking(false);progress.unparkTrackingOffRequested=true;
   command("Unpark outside tracking limits; tracking OFF requested; awaiting stationary telemetry",IPS_BUSY);
  }
  if(outcome==tenmicron::MotionOutcome::Complete){
   if(previous==tenmicron::MotionKind::Park){trackingRequestFailed=false;if(latchPark(false))safetyHold=false;}
   progress.kind=tenmicron::MotionKind::None;
   if(previous==tenmicron::MotionKind::StopAll){
    motionFault=!stopContext.empty();StopSP.s=motionFault?IPS_ALERT:IPS_OK;IDSetSwitch(&StopSP,nullptr);
    command((motionFault?stopContext+"; ":std::string{})+"Native telemetry confirms STOP completed; inspect native state",motionFault?IPS_ALERT:IPS_OK);
   }else if(previous==tenmicron::MotionKind::Flip){
    motionFault=false;command("Native flip complete: opposite pier side, tracking and settled target confirmed",IPS_OK);
   }else if(previous==tenmicron::MotionKind::Halt){
    motionFault=false;command("Native slew halt confirmed; tracking may remain on",IPS_OK);
   }else {
    motionFault=previous==tenmicron::MotionKind::Park && parkUnsafe;
    command(motionFault?"Native parked state confirmed, but safety latch could not be cleared; roof remains locked":"Native telemetry confirms command completed",motionFault?IPS_ALERT:IPS_OK);
   }
  }
  else if(outcome==tenmicron::MotionOutcome::Failed){
   if(previous==tenmicron::MotionKind::Flip){
    // An accepted flip has an uncertain motion outcome. Stop tracking as well
    // as slewing, retain a recovery hold and the fault after STOP confirmation.
    stopForSafety("Flip failed or timed out; required opposite-side completion not confirmed");
    if(!healthy)return; // Do not overwrite communication alerts with this old sample.
   }else if(previous==tenmicron::MotionKind::TrackingOn || previous==tenmicron::MotionKind::TrackingOff){
    progress.kind=tenmicron::MotionKind::None;
    trackingRequestFailed=true;trackingRequested=previous==tenmicron::MotionKind::TrackingOn;
    // A healthy unchanged tracking state is a failed request, not uncertain
    // mechanical motion. Native faults/movement remain faults and stay guarded.
    motionFault=!tenmicron::settledTrackingState(s);
    if(motionFault)safetyHold=true;
    command(std::string("Tracking ")+(trackingRequested?"ON":"OFF")+
            " not confirmed; native_status="+std::to_string(s.nativeStatus)+
            " ("+tenmicron::statusName(s.nativeStatus)+
            "); pending request released; no command replay",IPS_ALERT);
   }else{
    if(previous==tenmicron::MotionKind::StopAll){StopSP.s=IPS_ALERT;IDSetSwitch(&StopSP,nullptr);}
    progress.kind=tenmicron::MotionKind::None;motionFault=true;
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-progress.started).count();
    command("Motion completion failed: kind="+std::to_string(static_cast<int>(previous))+
            " native_status="+std::to_string(s.nativeStatus)+" ("+tenmicron::statusName(s.nativeStatus)+")"+
            " slew_or_settling="+std::to_string(s.slewOrSettling)+
            " elapsed_ms="+std::to_string(elapsed)+"; inspect native state",IPS_ALERT);
   }
  }
  bool tracking=s.nativeStatus==0 || s.nativeStatus==9 || s.nativeStatus==10;
  bool unknown=s.nativeStatus==98 || s.nativeStatus==99 || s.nativeStatus>11;
  bool fault=s.nativeStatus==8 || (s.nativeStatus==9 && !unparkLimitRecovery) || s.nativeStatus==11 || s.nativeStatus>=98 || s.nativeStatus>11;
  // Standard Park reports the controller, independently of command ownership.
  const bool parked=!unknown && s.nativeStatus==5 && !s.slewOrSettling;
  // INDI Dome ignores Busy/Alert packets. Engage its existing lock before
  // reporting an external unpark/unknown transition from a parked state.
  if(IsParked && !parked)SyncParkStatus(false);
  // SyncParkStatus() publishes Ok immediately. Build this update atomically
  // so a known interrupted park never emits a transient PARK/Ok roof unlock.
  IsParked=parked;parkPublished=true;
  ParkSP.reset();ParkSP[parked?PARK:UNPARK].setState(ISS_ON);
  if(unknown){ParkSP.reset();ParkSP.setState(IPS_ALERT);}
  else if(s.nativeStatus==2 || s.nativeStatus==3 ||
          progress.kind==tenmicron::MotionKind::Park || progress.kind==tenmicron::MotionKind::Unpark)
   ParkSP.setState(IPS_BUSY);
  // Truthful PARK On/Alert keeps INDI's dome lock engaged only after a known
  // explicit interruption. It never fabricates UNPARK to encode uncertainty.
  else if(parked && parkUnsafe)ParkSP.setState(IPS_ALERT);
  else ParkSP.setState(IPS_OK);
  ParkSP.apply();
  TrackState=SCOPE_IDLE;
  if(tracking) TrackState=SCOPE_TRACKING;
  else if(s.nativeStatus==5) TrackState=SCOPE_PARKED;
  else if(s.nativeStatus==2) TrackState=SCOPE_PARKING;
  else if(s.nativeStatus==3 || s.nativeStatus==4 || s.nativeStatus==6 || s.slewOrSettling) TrackState=SCOPE_SLEWING;
  if(progress.kind==tenmicron::MotionKind::Slew || progress.kind==tenmicron::MotionKind::Flip || progress.kind==tenmicron::MotionKind::Halt || progress.kind==tenmicron::MotionKind::Park || progress.kind==tenmicron::MotionKind::Unpark) {
   TrackState=(progress.kind==tenmicron::MotionKind::Park || progress.kind==tenmicron::MotionKind::Unpark)?SCOPE_PARKING:SCOPE_SLEWING;
  }
  setPierSide(s.pierSide=='E'?PIER_EAST:PIER_WEST);
  PierSideSP.setState(IPS_OK);PierSideSP.apply();
  NewRaDec(s.raHours,s.decDegrees);
  if(fault || motionFault){EqNP.setState(IPS_ALERT);EqNP.apply();}
  if(trackingRequestFailed && tenmicron::settledTrackingState(s) && tracking==trackingRequested)
   trackingRequestFailed=false; // Later real telemetry, never an assumed success.
  // A failed setter is not a persistent controller fault. Reconcile old request
  // Alerts with fresh healthy telemetry, without replaying RT/RG/RC/RM/RS.
  // Ginfo does not identify the selected rate preset: Idle is intentionally
  // unverified, not a fabricated mode/speed readback or a successful retry.
  if(healthy && !safetyHold && !fault && tenmicron::settledTrackingState(s)){
   if(TrackModeSP.getState()==IPS_ALERT){TrackModeSP.setState(IPS_IDLE);TrackModeSP.apply();}
   if(SlewRateSP.getState()==IPS_ALERT){SlewRateSP.setState(IPS_IDLE);SlewRateSP.apply();}
  }
  TrackStateSP.reset();TrackStateSP[tracking?TRACK_ON:TRACK_OFF].setState(ISS_ON);
  TrackStateSP.setState((fault || trackingRequestFailed)?IPS_ALERT:(progress.kind==tenmicron::MotionKind::TrackingOn || progress.kind==tenmicron::MotionKind::TrackingOff)?IPS_BUSY:tracking?IPS_OK:IPS_IDLE);TrackStateSP.apply();
  double data[]={static_cast<double>(s.nativeStatus),static_cast<double>(s.slewOrSettling),unknown?-1.0:static_cast<double>(s.nativeStatus==5),unknown?-1.0:static_cast<double>(tracking),s.azDegrees,s.altDegrees,s.julianDate,static_cast<double>(s.leapSecond)};
  for(int i=0;i<8;i++) StateN[i].value=data[i];
  StateNP.s=fault?IPS_ALERT:unparkLimitRecovery?IPS_BUSY:IPS_OK;IDSetNumber(&StateNP,nullptr);
  auto description=tenmicron::statusName(s.nativeStatus);
  IUSaveText(&StatusT[0],description.c_str());IUSaveText(&StatusT[1],"Connected; last poll valid");
  StatusTP.s=fault?IPS_ALERT:unparkLimitRecovery?IPS_BUSY:IPS_OK;IDSetText(&StatusTP,nullptr);
 }
 std::string stopContext;
 tenmicron::EnclosureState enclosure;
 bool enclosureStopIssued=false,safetyHold=false;
 bool initialEnclosurePending=false,initialEnclosureUnsafe=false;
 std::chrono::steady_clock::time_point initialEnclosureDeadline{};
 tenmicron::MotionProgress progress;
 bool motionFault=false,parkPublished=false,parkUnsafe=false,observedParking=false;
 std::chrono::steady_clock::time_point lastRoofRequest{};
 ISwitch StopS[1]{},SideS[3]{};ISwitchVectorProperty StopSP{},SideSP{};
 IText RoofStatusT[1]{},CommandStatusT[1]{};ITextVectorProperty RoofStatusTP{},CommandStatusTP{};
 IText SafetyT[2]{},ParkSafetyT[2]{};ITextVectorProperty SafetyTP{},ParkSafetyTP{};
 tenmicron::Protocol protocol;
 tenmicron::State lastState{};
 bool meridianValid=false;
 INumber ScheduleN[4]{};INumberVectorProperty ScheduleNP{};
 IText ScheduleT[4]{};ITextVectorProperty ScheduleTP{};
 INumber FlipResultN[2]{};INumberVectorProperty FlipResultNP{};
 bool healthy=false;
 std::chrono::steady_clock::time_point lastMeridianPoll{};
 INumber MeridianN[6]{};INumberVectorProperty MeridianNP{};
 ISwitch TransportS[2]{};
 ISwitchVectorProperty TransportSP{};
 IText SerialT[2]{};
 ITextVectorProperty SerialTP{};
 IText EndpointT[2]{},IdentityT[3]{},StatusT[2]{};
 ITextVectorProperty EndpointTP{},IdentityTP{},StatusTP{};
 INumber StateN[8]{};INumberVectorProperty StateNP{};
};
static std::unique_ptr<TenMicronMotion> driver(new TenMicronMotion());
