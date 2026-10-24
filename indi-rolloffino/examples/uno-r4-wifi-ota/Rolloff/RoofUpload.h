#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Bounded, maintenance-only receiver. Storage and network effects are supplied
// by the board adapter. CRC32 detects corruption; it is NOT a signature or TLS.
template<class Storage> class RoofUpload {
 public:
  enum Phase { HEADER, BODY, COMMIT, APPLIED, FAILED };
  explicit RoofUpload(Storage& s): storage(s) {}
  static uint32_t crcByte(uint32_t crc, uint8_t b) {
    crc ^= b;
    for (int i=0;i<8;++i) crc=(crc>>1)^((crc&1)?0xedb88320UL:0);
    return crc;
  }
  Phase phase=HEADER;
  const char* error="";
  bool opened=false;
  bool ready=false;
  uint32_t count=0, length=0;
  void start(uint32_t now) {
    abort(); phase=HEADER; error=""; ready=false; count=length=0;
    lineUsed=0; crc=0xffffffffUL; started=last=now;
  }
  void abort() { if(opened) storage.close(); opened=false; }
  void fail(const char* why) { abort(); error=why; phase=FAILED; }
  void tick(uint32_t now, bool closed) {
    if(phase==FAILED || phase==APPLIED) return;
    if(!closed) { fail("roof-not-closed"); return; }
    if(uint32_t(now-started)>=180000UL || uint32_t(now-last)>=10000UL)
      fail("timeout");
  }
  void feed(uint8_t b, uint32_t now, const char* token, bool closed) {
    tick(now,closed);
    if(phase==FAILED || phase==APPLIED) return;
    last=now;
    if(phase==BODY) {
      if(count<8) vectors[count]=b;
      if(storage.write(b)!=1) { fail("flash-write"); return; }
      crc=crcByte(crc,b); ++count;
      if(count==length) {
        storage.close(); opened=false;
        const uint32_t sp=le32(vectors), pc=le32(vectors+4);
        if((crc^0xffffffffUL)!=expectedCrc) { fail("crc"); return; }
        if(sp<0x20000000UL || sp>0x20008000UL || (sp&7) || !(pc&1) ||
           (pc&~1UL)<storage.sketchStart() ||
           (pc&~1UL)>=storage.sketchStart()+length) {
          fail("vectors"); return;
        }
        if(!storage.verify(length,expectedCrc)) { fail("flash-verify"); return; }
        phase=COMMIT; lineUsed=0; ready=true;
      }
      return;
    }
    if(b=='\n') {
      line[lineUsed]='\0';
      if(phase==COMMIT) {
        if(strcmp(line,"COMMIT")!=0) { fail("commit"); return; }
        // Caller must send APPLYING before invoking apply(). No apply on a
        // disconnected or incomplete transfer, wrong CRC or expired session.
        phase=APPLIED;
      } else parseHeader(token);
      lineUsed=0;
    } else if(b<32 || b>126 || lineUsed>=sizeof(line)-1) fail("header");
    else line[lineUsed++]=char(b);
  }
 private:
  Storage& storage;
  char line[128]{};
  uint8_t vectors[8]{};
  size_t lineUsed=0;
  uint32_t crc=0xffffffffUL,expectedCrc=0,started=0,last=0;
  static uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
  }
  void parseHeader(const char* token) {
    // ROOFOTA1 <32 hex password> <decimal size> <8 hex CRC32>
    char auth[33]{}, sizeText[12]{}, crcText[9]{}, extra=0;
    if(sscanf(line,"ROOFOTA1 %32s %11s %8s %c",auth,sizeText,crcText,&extra)!=3 ||
       strlen(auth)!=32 || strlen(token)!=32 || strcmp(auth,token)!=0 ||
       strlen(crcText)!=8) { fail("authentication-or-header"); return; }
    for(const char* p=sizeText;*p;++p) if(*p<'0'||*p>'9') { fail("size"); return; }
    for(const char* p=crcText;*p;++p)
      if(!((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')||(*p>='A'&&*p<='F'))) {
        fail("crc-header"); return;
      }
    char* end=nullptr;
    const unsigned long n=strtoul(sizeText,&end,10);
    if(!end || *end || n<256 || n>storage.maxSize()) { fail("size"); return; }
    length=uint32_t(n); expectedCrc=uint32_t(strtoul(crcText,nullptr,16));
    if(!storage.open(length)) { fail("flash-open"); return; }
    opened=true; phase=BODY;
  }
};
