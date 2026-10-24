#pragma once
#include <vector>
class InternalStorageRenesasClass {
 protected:
  const uint32_t SKETCH_START_ADDRESS=0x4000UL;
  const uint32_t PAGE_SIZE=1024UL;
 public:
  std::vector<uint8_t> bytes;
  int applies=0;
  long maxSize(){return 122880;}
  int open(int){bytes.clear();return 1;}
  size_t write(uint8_t b){bytes.push_back(b);return 1;}
  void close(){}
  void apply(){++applies;}
};
