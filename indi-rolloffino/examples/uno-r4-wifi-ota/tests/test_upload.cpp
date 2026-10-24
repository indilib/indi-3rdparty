#include "../Rolloff/RoofUpload.h"
#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>
struct Storage {
 std::vector<uint8_t> bytes;
 int opens=0,closes=0;
 bool brokenWrite=false,brokenVerify=false,brokenOpen=false;
 uint32_t sketchStart(){return 0x4000;}
 uint32_t maxSize(){return 4096;}
 bool open(uint32_t){++opens;bytes.clear();return !brokenOpen;}
 size_t write(uint8_t b){bytes.push_back(b);return brokenWrite?0:1;}
 void close(){++closes;}
 bool verify(uint32_t,uint32_t){return !brokenVerify;}
};
using Upload=RoofUpload<Storage>;
const char* token="0123456789abcdef0123456789abcdef";
void require(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
std::vector<uint8_t> image(){std::vector<uint8_t>b(512,0x55);uint32_t sp=0x20008000,pc=0x4101;for(int i=0;i<4;++i){b[i]=sp>>(i*8);b[4+i]=pc>>(i*8);}return b;}
uint32_t crc(const std::vector<uint8_t>& b){uint32_t c=~0U;for(auto v:b)c=Upload::crcByte(c,v);return c^~0U;}
std::string header(const std::vector<uint8_t>& b){char h[128];snprintf(h,sizeof h,"ROOFOTA1 %s %zu %08x\n",token,b.size(),crc(b));return h;}
void send(Upload& u,const std::string& s,uint32_t t=0){for(auto c:s)u.feed(c,t,token,true);}
void body(Upload& u,const std::vector<uint8_t>& b){for(auto c:b)u.feed(c,100,token,true);}
template<class F>void test(const char* name,F f){f();std::cout<<"PASS "<<name<<'\n';}
int main(){int n=0;
#define TEST(name, ...) test(name,[&](){__VA_ARGS__;});++n
 TEST("CRC32 independent known vector",std::vector<uint8_t>b{'1','2','3','4','5','6','7','8','9'};require(crc(b)==0xcbf43926,"CRC polynomial"));
 TEST("full image waits for explicit commit",Storage s;Upload u(s);u.start(0);auto b=image();send(u,header(b));body(u,b);require(u.phase==Upload::COMMIT&&u.ready&&s.bytes==b,"staging");send(u,"COMMIT\n",200);require(u.phase==Upload::APPLIED,"commit"));
 TEST("wrong password never opens flash",Storage s;Upload u(s);u.start(0);auto h=header(image());h[9]='x';send(u,h);require(u.phase==Upload::FAILED&&s.opens==0,"auth"));
 TEST("oversize rejected before flash",Storage s;Upload u(s);u.start(0);send(u,std::string("ROOFOTA1 ")+token+" 99999 12345678\n");require(u.phase==Upload::FAILED&&s.opens==0,"size"));
 TEST("zero and tiny images rejected",for(auto size:{0,8,255}){Storage s;Upload u(s);u.start(0);send(u,std::string("ROOFOTA1 ")+token+" "+std::to_string(size)+" 12345678\n");require(u.phase==Upload::FAILED&&s.opens==0,"tiny");});
 TEST("oversized header bounded",Storage s;Upload u(s);u.start(0);send(u,std::string(400,'x'));require(u.phase==Upload::FAILED,"header"));
 TEST("partial header times out",Storage s;Upload u(s);u.start(0);send(u,"ROOF");u.tick(10000,true);require(u.phase==Upload::FAILED&&s.opens==0,"timeout"));
 TEST("partial body times out and closes storage",Storage s;Upload u(s);u.start(0);send(u,header(image()));u.feed(1,100,token,true);u.tick(10100,true);require(u.phase==Upload::FAILED&&s.closes==1,"body timeout"));
 TEST("trickle cannot exceed absolute timeout",Storage s;Upload u(s);u.start(0);for(uint32_t t=9000;t<180000;t+=9000)u.tick(t,true),u.feed('x',t,token,true);u.tick(180000,true);require(u.phase==Upload::FAILED,"absolute"));
 TEST("changed roof limit aborts",Storage s;Upload u(s);u.start(0);send(u,header(image()));u.tick(10,false);require(u.phase==Upload::FAILED&&s.closes==1,"limit"));
 TEST("corrupted image rejected",Storage s;Upload u(s);u.start(0);auto b=image();send(u,header(b));b[100]^=1;body(u,b);require(u.phase==Upload::FAILED,"crc"));
 TEST("invalid vector table rejected",Storage s;Upload u(s);u.start(0);auto b=image();b[0]=1;send(u,header(b));body(u,b);require(u.phase==Upload::FAILED,"vector"));
 TEST("flash write failure rejected",Storage s;s.brokenWrite=true;Upload u(s);u.start(0);send(u,header(image()));body(u,image());require(u.phase==Upload::FAILED,"write"));
 TEST("flash readback failure rejected",Storage s;s.brokenVerify=true;Upload u(s);u.start(0);send(u,header(image()));body(u,image());require(u.phase==Upload::FAILED,"readback"));
 TEST("wrong commit never applies",Storage s;Upload u(s);u.start(0);send(u,header(image()));body(u,image());send(u,"WRONG\n",200);require(u.phase==Upload::FAILED,"bad commit"));
 TEST("commit timeout never applies",Storage s;Upload u(s);u.start(0);send(u,header(image()));body(u,image());u.tick(10100,true);require(u.phase==Upload::FAILED,"commit timeout"));
 TEST("clock wrap keeps timeout",Storage s;Upload u(s);u.start(0xfffffff0U);u.tick(9984,true);require(u.phase==Upload::FAILED,"wrap"));
 TEST("failed storage open never starts body",Storage s;s.brokenOpen=true;Upload u(s);u.start(0);send(u,header(image()));require(u.phase==Upload::FAILED&&!u.opened,"open"));
 std::cout<<n<<" upload receiver scenarios passed\n";
}
