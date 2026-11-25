#include <indidome.h>
#include <lilxml.h>
#include <stdexcept>
#include <string>
class RoofTest:public INDI::Dome {
 public:
 const char *getDefaultName()override{return "Native Lock Test Roof";}
 bool initProperties()override{
  INDI::Dome::initProperties();ActiveDeviceTP[ACTIVE_MOUNT].setText("10Micron Native Motion");MountPolicySP[MOUNT_IGNORED].setState(ISS_OFF);MountPolicySP[MOUNT_LOCKS].setState(ISS_ON);return true;
 }
};
void update(RoofTest &roof,const char *state,bool parked){
 std::string xml="<setSwitchVector device='10Micron Native Motion' name='TELESCOPE_PARK' state='"+std::string(state)+"'><oneSwitch name='PARK'>"+(parked?"On":"Off")+"</oneSwitch><oneSwitch name='UNPARK'>"+(parked?"Off":"On")+"</oneSwitch></setSwitchVector>";
 LilXML *parser=newLilXML();XMLEle *root=nullptr;char error[1024]{};
 for(char c:xml){root=readXMLEle(parser,c,error);if(root)break;}
 if(!root)throw std::runtime_error("cannot parse park packet");
 roof.ISSnoopDevice(root);delXMLEle(root);delLilXML(parser);
}
int main(){
 RoofTest roof;roof.ISGetProperties(nullptr);
 update(roof,"Ok",true);if(roof.isLocked())return 1;
 update(roof,"Ok",false);if(!roof.isLocked())return 2;
 update(roof,"Busy",true);if(!roof.isLocked())return 3; // Parking is not parked.
 update(roof,"Alert",true);if(!roof.isLocked())return 4; // Failure must not unlock.
 update(roof,"Ok",true);if(roof.isLocked())return 5;
 return 0;
}
