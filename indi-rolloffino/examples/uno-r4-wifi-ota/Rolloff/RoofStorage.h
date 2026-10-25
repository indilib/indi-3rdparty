#pragma once
// ArduinoOTA 1.1.1 supplies the Renesas flash backend only. Its network poll
// and mDNS implementation are NOT used by this candidate.
// The upstream geometry constants are protected MEMBERS, not global macros.
// Own a derived backend to expose its geometry without guessing flash offsets.
class RoofFlashBackend : public InternalStorageRenesasClass {
 public:
  uint32_t sketchStart() const { return SKETCH_START_ADDRESS; }
  uint32_t pageSize() const { return PAGE_SIZE; }
};
RoofFlashBackend roofFlash;

class RoofStorage {
 public:
  uint32_t sketchStart() const { return roofFlash.sketchStart(); }
  uint32_t maxSize() {
    // The backend erases one extra page; reserve it inside the staging half.
    return uint32_t(roofFlash.maxSize())-roofFlash.pageSize();
  }
  bool open(uint32_t n) {
    if(roofFlash.open(int(n))==1) return true;
    roofFlash.close(); return false;
  }
  size_t write(uint8_t b) { return roofFlash.write(b); }
  void close() { roofFlash.close(); }
  bool verify(uint32_t n,uint32_t expected) {
#ifdef ROOF_HOST_TEST
    const uint8_t* bytes=roofFlash.bytes.data();
#else
    const uint8_t* bytes=reinterpret_cast<const uint8_t*>(
        roofFlash.sketchStart()+roofFlash.maxSize());
#endif
    uint32_t crc=0xffffffffUL;
    for(uint32_t i=0;i<n;++i) crc=RoofUpload<RoofStorage>::crcByte(crc,bytes[i]);
    return (crc^0xffffffffUL)==expected;
  }
  void apply() { roofFlash.apply(); }
};
