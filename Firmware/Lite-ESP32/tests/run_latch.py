#!/usr/bin/env python3
"""Compile the actual latch adapter against a deterministic NOR flash model."""
import pathlib
import subprocess
import tempfile

project = pathlib.Path(__file__).resolve().parents[1]
esp8266 = project.name == 'Lite-ESP8266'
offset = 0x7b000 if esp8266 else 0xff000
flash = 0x80000 if esp8266 else 0x100000
with tempfile.TemporaryDirectory(prefix='lite-latch-') as tmp:
    p = pathlib.Path(tmp)
    (p/'MiningEmbed.h').write_text('''#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
namespace miningembed {struct PendingTransaction {uint64_t nonce=0;std::string hash;};}
''')
    (p/'Arduino.h').write_text('''#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <algorithm>
#define F(s) s
inline void yield() {}
struct Console {template<class T> void print(T) {} template<class T> void println(T) {}};
inline Console Serial;
inline uint8_t bytes[4096];
inline int programLimit=4096, eraseLimit=4096;
inline bool readOk=true;
inline bool modelRead(uint32_t at, void* out, size_t n) {
    if (!readOk || at + n > 4096) {
        return false;
    }
    memcpy(out, bytes + at, n);
    return true;
}
inline bool modelWrite(uint32_t at,const void* in,size_t n) {
 size_t written=std::min(n,size_t(programLimit));
 for(size_t i=0;i<written;i++) bytes[at+i]&=static_cast<const uint8_t*>(in)[i];
 return written==n;
}
inline bool modelErase() {memset(bytes,255,eraseLimit);return eraseLimit==4096;}
struct Esp {
 uint32_t getFlashChipSize(){return APLO_FLASH_BYTES;}
 uint32_t getFlashChipRealSize(){return APLO_FLASH_BYTES;}
 uint32_t getSketchSize(){return 440000;}
 bool flashRead(uint32_t a,uint32_t* p,size_t n){return modelRead(a-APLO_STORAGE_OFFSET,p,n);}
 bool flashWrite(uint32_t a,uint32_t* p,size_t n){return modelWrite(a-APLO_STORAGE_OFFSET,p,n);}
 bool flashEraseSector(uint32_t a){return a==APLO_STORAGE_OFFSET/4096 && modelErase();}
};
inline Esp ESP;
''')
    (p/'esp_partition.h').write_text('''#pragma once
#include "Arduino.h"
#define ESP_OK 0
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_TYPE_APP 0
#define ESP_PARTITION_SUBTYPE_APP_FACTORY 0
using esp_partition_subtype_t=int;
struct esp_partition_t {uint32_t address,size;};
inline esp_partition_t part{APLO_STORAGE_OFFSET,4096},app{0x10000,APLO_STORAGE_OFFSET-0x10000};
inline const esp_partition_t* esp_partition_find_first(int type,int,const char*) {return type?&part:&app;}
inline int esp_partition_read(const esp_partition_t*,uint32_t at,void* out,size_t n){return modelRead(at,out,n)?0:-1;}
inline int esp_partition_write(const esp_partition_t*,uint32_t at,const void* in,size_t n){return modelWrite(at,in,n)?0:-1;}
inline int esp_partition_erase_range(const esp_partition_t*,uint32_t at,size_t n){return at==0&&n==4096&&modelErase()?0:-1;}
''')
    (p/'esp_spi_flash.h').write_text('#pragma once\n')
    source=(project/'src/SigningLatch.cpp').read_text()
    if esp8266:
        # Model the target absolute linker symbol; all adapter I/O is unchanged.
        source=source.replace('extern "C" uint32_t _EEPROM_start;',
          '#define _EEPROM_start (*reinterpret_cast<uint32_t*>(uintptr_t(0x4027b000)))')
    (p/'adapter.cpp').write_text(source)
    (p/'test.cpp').write_text('''#include "Arduino.h"
#include "SigningLatch.h"
#include <cassert>
#include <cstdio>
int main() {
 miningembed::PendingTransaction tx;tx.nonce=9;tx.hash="0x"+std::string(64,'a');
 // Every programmed byte anywhere in the sector must block the next boot.
 for(int at=0;at<4096;at++) {
  memset(bytes,255,4096);bytes[at]=0;SigningLatch reboot;assert(!reboot.begin());
 }
 // Every prefix of a torn pre-broadcast write: erased remains safe, residue locks.
 for(int cut=0;cut<=96;cut++) {
  memset(bytes,255,4096);programLimit=cut;SigningLatch live;assert(live.begin());
  bool ok=SigningLatch::writePending(&tx,&live);assert(ok==(cut==96));
  SigningLatch reboot;assert(reboot.begin()==(cut==0));
 }
 programLimit=4096;
 // Terminal receipt clear may tear; any remaining residue still blocks reboot.
 for(int cut=0;cut<=4096;cut++) {
  memset(bytes,255,4096);eraseLimit=cut;SigningLatch live;assert(live.begin());
  assert(SigningLatch::writePending(&tx,&live));
  bool ok=SigningLatch::writePending(nullptr,&live);assert(ok==(cut==4096));
  bool blank=true;for(uint8_t byte:bytes) blank=blank&&byte==255;
  SigningLatch reboot;assert(reboot.begin()==blank);
 }
 memset(bytes,255,4096);readOk=false;SigningLatch readFailure;assert(!readFailure.begin());
 assert(!SigningLatch::writePending(&tx,nullptr));
 puts("PASS: 4096 residue positions, 97 torn writes, 4097 torn clears, read failure");
}
''')
    args=['c++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(p),'-I'+str(project/'include'),
          f'-DAPLO_STORAGE_OFFSET={offset}',f'-DAPLO_FLASH_BYTES={flash}','-DAPLO_STORAGE_SIZE=4096',
          str(p/'adapter.cpp'),str(p/'test.cpp'),'-o',str(p/'test')]
    subprocess.run(args,check=True)
    subprocess.run([str(p/'test')],check=True)
