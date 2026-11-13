#pragma once
namespace tenmicron {
// Standard INDI properties are event-driven. Do not impose a vendor-specific
// 15-second light-vector refresh contract on other dome drivers.
struct EnclosureState {
 bool connected=false,parkOpen=false,shutterSeen=false,shutterOpen=false;
 void reset(){connected=false;parkOpen=false;shutterOpen=false;}
 void connection(bool on){if(!on || !connected)reset();connected=on;}
 bool permits(bool ignored,bool named)const {
  return ignored || (named && connected && parkOpen && (!shutterSeen || shutterOpen));
 }
};
}
