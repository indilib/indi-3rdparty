#include <iostream>
#include <functional>
#include <vector>
#ifdef ORIGINAL_SKETCH
#include "../baseline/Rolloff.ino"
#else
#include "../Rolloff/Rolloff.ino"
#endif
void require(bool yes,const char*msg){if(!yes)throw std::runtime_error(msg);}
std::shared_ptr<Peer> connectPeer(){auto p=std::make_shared<Peer>();server.pending.emplace_back(p);loop();return p;}
void send(const std::shared_ptr<Peer>&p,const std::string&s){for(char c:s)p->rx.push_back(c);loop();}
void reset(){client.stop();server.pending.clear();levels.fill(LOW);modes.fill(0);fakeTime=0;simultaneous=0;otaCalls=0;otaWhileActive=false;
#ifndef ORIGINAL_SKETCH
 eventNext=0;eventUsed=0;eventSequence=0;clientSession=false;
#endif

#ifndef ORIGINAL_SKETCH
 maintenance=false;uploadSession=false;upload.abort();uploadClient.stop();uploadServer.pending.clear();roofFlash.bytes.clear();roofFlash.applies=0;
#endif
 setup();levels[2]=HIGH;levels[3]=LOW;}
bool off(){return levels[4]==LOW&&levels[7]==LOW&&levels[8]==LOW;}

#ifndef ORIGINAL_SKETCH
std::shared_ptr<Peer> connectUpload(){auto p=std::make_shared<Peer>();uploadServer.pending.emplace_back(p);loop();return p;}
void uploadBytes(const std::shared_ptr<Peer>&p,const std::string& bytes){for(auto c:bytes)p->rx.push_back(c);for(int i=0;i<100 && !p->rx.empty()&&p->connected;++i)loop();}
std::string firmware(){std::string b(512,'x');uint32_t sp=0x20008000,pc=roofFlash.sketchStart()+257;for(int i=0;i<4;++i){b[i]=sp>>(8*i);b[4+i]=pc>>(8*i);}return b;}
std::string uploadHeader(const std::string&b){uint32_t crc=~0U;for(uint8_t c:b)crc=RoofUpload<RoofStorage>::crcByte(crc,c);char h[128];snprintf(h,sizeof h,"ROOFOTA1 %s %zu %08x\n",OTA_PASSWORD,b.size(),crc^~0U);return h;}
void enableOta(const std::shared_ptr<Peer>&p){send(p,"(SET:OTA:" OTA_PASSWORD ")");require(maintenance,"Maintenance not enabled");}
#endif

int main(){
#ifdef ORIGINAL_SKETCH
 reset();auto p=connectPeer();send(p,"(SET:OPEN:ON)");require(levels[4]==HIGH,"Open did not energize");fakeTime=100;p->connected=false;loop();fakeTime=3000;loop();require(levels[4]==HIGH,"Original bug did not reproduce");std::cout<<"REPRODUCED original: OPEN relay remains ON at 3000 ms after disconnect at 100 ms\n";return 0;
#else
 std::vector<std::pair<const char*,std::function<void()>>> tests={

 {"OTA disabled on startup",[]{require(!maintenance&&!uploadSession,"OTA enabled");auto q=connectUpload();uploadBytes(q,"junk");require(roofFlash.bytes.empty(),"Wrote flash while disabled");}},
 {"OTA credentials and idle closed roof required",[]{auto p=connectPeer();send(p,"(SET:OTA:wrong)");require(!maintenance,"Wrong token enabled");levels[2]=LOW;send(p,"(SET:OTA:" OTA_PASSWORD ")");require(!maintenance,"Contradictory limits enabled");levels[2]=HIGH;levels[3]=HIGH;send(p,"(SET:OTA:" OTA_PASSWORD ")");require(!maintenance,"Open roof enabled");}},
 {"OTA refused during active relay pulse",[]{auto p=connectPeer();send(p,"(SET:OPEN:ON)");send(p,"(SET:OTA:" OTA_PASSWORD ")");require(!maintenance&&levels[4]==HIGH,"Pulse interrupted by OTA");}},
 {"maintenance locks out motor commands",[]{auto p=connectPeer();enableOta(p);for(auto op:{"OPEN","CLOSE","ABORT"})send(p,std::string("(SET:")+op+":ON)");require(off()&&maintenance,"Maintenance moved motor");}},
 {"version and maintenance state observable",[]{auto p=connectPeer();send(p,"(GET:VERSION:0)(GET:OTA:0)");require(p->tx.find(ROOF_BUILD_ID)!=std::string::npos&&p->tx.find("OTA:DISABLED")!=std::string::npos,"Identity absent");}},
 {"network transfer stages then commits through real adapter",[]{auto p=connectPeer();enableOta(p);auto q=connectUpload();auto b=firmware();uploadBytes(q,uploadHeader(b)+b);require(q->tx.find("READY\n")!=std::string::npos&&roofFlash.applies==0,"Not staged only");uploadBytes(q,"COMMIT\n");require(roofFlash.applies==1&&q->tx.find("APPLYING\n")!=std::string::npos&&off(),"No apply or active output");}},
 {"disconnect after READY never applies",[]{auto p=connectPeer();enableOta(p);auto q=connectUpload();auto b=firmware();uploadBytes(q,uploadHeader(b)+b);q->connected=false;loop();require(roofFlash.applies==0&&off(),"Disconnect applied");}},
 {"roof limit change before COMMIT cancels",[]{auto p=connectPeer();enableOta(p);auto q=connectUpload();auto b=firmware();uploadBytes(q,uploadHeader(b)+b);levels[3]=HIGH;uploadBytes(q,"COMMIT\n");require(!maintenance&&roofFlash.applies==0&&off(),"Moved roof applied");}},
 {"maintenance expiry disables listener",[]{auto p=connectPeer();enableOta(p);fakeTime=300000;loop();require(!maintenance&&off(),"No expiry");}},
 {"upload session timeout releases socket",[]{auto p=connectPeer();enableOta(p);auto q=connectUpload();uploadBytes(q,"ROOF");fakeTime=10000;loop();require(!uploadSession&&roofFlash.applies==0&&off(),"No timeout");}},
 {"explicit cancel aborts staged upload",[]{auto p=connectPeer();enableOta(p);auto q=connectUpload();auto b=firmware();uploadBytes(q,uploadHeader(b)+b);send(p,"(SET:OTA:OFF)");require(!maintenance&&!q->connected&&roofFlash.applies==0&&off(),"Cancel failed");}},
 {"bounded event ring and newest-first query",[]{for(int i=0;i<20;++i){fakeTime=static_cast<uint32_t>(i);recordEvent("TEST",i);}require(eventUsed==16,"Ring not bounded");auto p=connectPeer();send(p,"(GET:EVENT:0)");require(p->tx.find("event=CLIENT CONNECTED")!=std::string::npos,"Newest missing");send(p,"(GET:EVENT:15)");require(p->tx.find("seq=7,")!=std::string::npos,"Oldest wrong");send(p,"(GET:EVENT:16)");require(p->tx.find("EVENT:NONE")!=std::string::npos,"Out of range");}},
 {"event read has no relay side effect",[]{auto p=connectPeer();send(p,"(SET:OPEN:ON)");fakeTime=500;send(p,"(GET:EVENT:0)");require(p->tx.find("ms=0,age_ms=500,pin=4,event=RELAY ON")!=std::string::npos,"Timestamp missing");require(levels[4]==HIGH,"Event released coil");fakeTime=2000;loop();require(off(),"Event extended deadline");}},
 {"malformed event index rejected",[]{auto p=connectPeer();send(p,"(GET:EVENT:-1)(GET:EVENT:xx)");require(p->tx.find("BadEventIndex")!=std::string::npos,"Bad index accepted");require(off(),"Event moved");}},
 {"headless startup, inactive outputs",[]{require(off(),"Startup energized");require(modes[4]==OUTPUT&&modes[7]==OUTPUT&&modes[8]==OUTPUT,"Missing outputs");}},
 {"Open exact deadline",[]{auto p=connectPeer();send(p,"(SET:OPEN:ON)");fakeTime=1999;loop();require(levels[4]==HIGH,"Early release");fakeTime=2000;loop();require(off(),"Late release");}},
 {"Close exact deadline",[]{auto p=connectPeer();send(p,"(SET:CLOSE:ON)");require(levels[7]==HIGH,"Close missing");fakeTime=2000;loop();require(off(),"Close held");}},
 {"Abort exact deadline",[]{auto p=connectPeer();send(p,"(SET:ABORT:ON)");require(levels[8]==HIGH,"Abort missing");fakeTime=2000;loop();require(off(),"Abort held");}},
 {"disconnect during each pulse",[]{for(const char*op:{"OPEN","CLOSE","ABORT"}){reset();auto p=connectPeer();send(p,std::string("(SET:")+op+":ON)");fakeTime=100;p->connected=false;loop();require(off(),"Disconnect held relay");fakeTime=3000;loop();require(off(),"Relay reactivated");}}},
 {"reconnect does not replay old command",[]{auto p=connectPeer();send(p,"(SET:OPEN:ON)");p->connected=false;auto q=connectPeer();require(off(),"Replayed output");send(q,"(SET:CLOSE:ON)");require(levels[7]==HIGH&&levels[4]==LOW,"New command failed");}},
 {"duplicate ON does not extend pulse",[]{auto p=connectPeer();send(p,"(SET:OPEN:ON)");fakeTime=1500;send(p,"(SET:OPEN:ON)");fakeTime=2000;loop();require(off(),"Duplicate extended deadline");}},
 {"opposite direction rejected during pulse",[]{auto p=connectPeer();send(p,"(SET:OPEN:ON)");send(p,"(SET:CLOSE:ON)");require(levels[4]==HIGH&&levels[7]==LOW,"Opposite energized");require(p->tx.find("(NAK:ERROR:Busy)")!=std::string::npos,"No busy reply");require(simultaneous==1,"Overlapping relays");}},
 {"Abort preempts with contacts released",[]{auto p=connectPeer();send(p,"(SET:OPEN:ON)");fakeTime=100;send(p,"(SET:ABORT:ON)");require(levels[4]==LOW&&levels[8]==HIGH,"Abort failed");require(fakeTime==125&&simultaneous==1,"No release interval");fakeTime=2125;loop();require(off(),"Abort held");}},
 {"unrelated OFF cannot cancel active deadline",[]{auto p=connectPeer();send(p,"(SET:OPEN:ON)");send(p,"(SET:CLOSE:OFF)");require(levels[4]==HIGH,"Wrong relay released");fakeTime=2000;loop();require(off(),"Unrelated OFF stranded output");}},
 {"Abort OFF releases without new pulse",[]{auto p=connectPeer();send(p,"(SET:ABORT:ON)");send(p,"(SET:ABORT:OFF)");require(off(),"Abort OFF energized");}},
 {"GET and handshake retained",[]{auto p=connectPeer();send(p,"(CON:0:0)(GET:OPENED:0)(GET:CLOSED:0)(GET:LOCKED:0)(GET:AUXSTATE:0)");loop();require(p->tx.find("(ACK:0:V1.5-wifi-candidate)")!=std::string::npos,"Handshake missing");require(p->tx.find("(ACK:OPENED:OFF)")!=std::string::npos&&p->tx.find("(ACK:CLOSED:ON)")!=std::string::npos,"Limits incorrect");require(off(),"GET caused movement");}},
 {"fragmented frame",[]{auto p=connectPeer();send(p,"(SET:OP");require(off(),"Partial moved");send(p,"EN:ON)");require(levels[4]==HIGH,"Fragment failed");}},
 {"partial frame absolute timeout",[]{auto p=connectPeer();send(p,"(SET:OPEN:");fakeTime=1000;send(p,"ON)");require(off(),"Expired command executed");require(p->tx.find("CommandTimeout")!=std::string::npos,"Missing timeout");}},
 {"disconnect discards partial frame",[]{auto p=connectPeer();send(p,"(SET:OPEN:");p->connected=false;auto q=connectPeer();send(q,"ON)");require(off(),"Old fragment executed");}},
 {"oversized frame bounded and recovers",[]{auto p=connectPeer();send(p,"("+std::string(200,'X')+")");for(int i=0;i<5;++i)loop();require(off(),"Oversize moved");require(p->tx.find("BadFrame")!=std::string::npos,"Missing overflow error");send(p,"(SET:OPEN:ON)");require(levels[4]==HIGH,"Parser did not recover");}},
 {"invalid SET value rejected",[]{auto p=connectPeer();send(p,"(SET:OPEN:GARBAGE)(SET:ABORT:BOGUS)");require(off(),"Invalid moved");require(p->tx.find("BadSET")!=std::string::npos,"Missing rejection");}},
 {"busy input cannot starve pulse expiry",[]{auto p=connectPeer();send(p,"(SET:OPEN:ON)");fakeTime=2000;send(p,std::string(10000,'x'));require(off(),"Flood held output");require(p->rx.size()>=9936,"Unbounded input drain");}},
 {"32-bit clock wrap",[]{auto p=connectPeer();fakeTime=UINT32_MAX-999;send(p,"(SET:CLOSE:ON)");fakeTime=999;loop();require(levels[7]==HIGH,"Early wrap release");fakeTime=1000;loop();require(off(),"Wrap deadline missed");}},
 {"nested start discards old partial command",[]{auto p=connectPeer();send(p,"(SET:OPEN:(GET:CLOSED:0)");require(off(),"Malformed moved");require(p->tx.find("ACK:CLOSED:ON")!=std::string::npos,"Resync failed");}},
 {"nonprintable input rejected",[]{auto p=connectPeer();send(p,std::string("(SET:OPEN:")+char(0)+"ON)");require(off(),"NUL executed");require(p->tx.find("BadFrame")!=std::string::npos,"NUL not rejected");}}
 };
 int failures=0;for(const auto&t:tests){try{reset();t.second();std::cout<<"PASS "<<t.first<<'\n';}catch(const std::exception&e){++failures;std::cout<<"FAIL "<<t.first<<": "<<e.what()<<'\n';}}
 std::cout<<tests.size()-failures<<" passed, "<<failures<<" failed\n";return failures?1:0;
#endif
}
