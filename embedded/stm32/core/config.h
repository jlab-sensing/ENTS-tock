#pragma once

#include <RadioLib.h>
#include "libtockHal.h"

// first you have to set your radio model and pin configuration
// this is provided just as a default example

// create a new instance of the HAL class

// if you have RadioBoards (https://github.com/radiolib-org/RadioBoards)
// and are using one of the supported boards, you can do the following:
/*
#define RADIO_BOARD_AUTO
#include <RadioBoards.h>

Radio radio = new RadioModule();
*/

// how often to send an uplink - consider legal & FUP constraints - see notes
const uint32_t uplinkIntervalSeconds = 5UL * 60UL;    // minutes x seconds

// joinEUI - previous versions of LoRaWAN called this AppEUI
// for development purposes you can use all zeros - see wiki for details
#define RADIOLIB_LORAWAN_JOIN_EUI 01,01,01,01,01,01,01,01

// the Device EUI & two keys can be generated on the TTN console 
#ifndef RADIOLIB_LORAWAN_DEV_EUI   // Replace with your Device EUI
#define RADIOLIB_LORAWAN_DEV_EUI   05,38,04,D9,00,80,E1,15
#endif
#ifndef RADIOLIB_LORAWAN_APP_KEY   // Replace with your App Key 
#define RADIOLIB_LORAWAN_APP_KEY  02,B0,7E,01,50,16,02,80,AE,0D,20,A6,0A,B0,F7,01
#endif
#ifndef RADIOLIB_LORAWAN_NWK_KEY   // Put your Nwk Key here
#define RADIOLIB_LORAWAN_NWK_KEY B8,2D,91,3F,AF,B0,99,EE,2E,3B,BE,9B,31,96,C1,09 
#endif

// for the curious, the #ifndef blocks allow for automated testing &/or you can
// put your EUI & keys in to your platformio.ini - see wiki for more tips

// regional choices: EU868, US915, AU915, AS923, AS923_2, AS923_3, AS923_4, IN865, KR920, CN470
const LoRaWANBand_t* Region = &US915;

// subband choice: for US915/AU915 set to 2, for CN470 set to 1, otherwise leave on 0
const uint8_t subBand = 2;

#define RAW_TO_INT8A(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) {0x##a,0x##b,0x##c,0x##d,\
                                                       0x##e,0x##f,0x##g,0x##h,\
                                                       0x##i,0x##j,0x##k,0x##l,\
                                                       0x##m,0x##n,0x##o,0x##p}

#define RAW8_TO_INT8A(a,b,c,d) 0x##a##b##c##d
#define RAW32_TO_INT8A(a,b,c,d,e,f,g,h) {0x##a,0x##b,0x##c,0x##d,\
                                         0x##e,0x##f,0x##g,0x##h}

#define FORMAT_KEY(...) RAW_TO_INT8A(__VA_ARGS__)
#define FORMAT8_KEY(...) RAW8_TO_INT8A(__VA_ARGS__)
#define FORMAT32_KEY(...) RAW32_TO_INT8A(__VA_ARGS__)

// Big-endian: b[0] is MSB, b[7] is LSB
static inline uint64_t bytes_to_u64_be(const uint8_t b[8]) {
  return ((uint64_t)b[0] << 56) |
         ((uint64_t)b[1] << 48) |
         ((uint64_t)b[2] << 40) |
         ((uint64_t)b[3] << 32) |
         ((uint64_t)b[4] << 24) |
         ((uint64_t)b[5] << 16) |
         ((uint64_t)b[6] << 8)  |
         ((uint64_t)b[7]);
}

static inline uint64_t bytes_to_u64_le(const uint8_t b[8]) {
  return ((uint64_t)b[7] << 56) |
         ((uint64_t)b[6] << 48) |
         ((uint64_t)b[5] << 40) |
         ((uint64_t)b[4] << 32) |
         ((uint64_t)b[3] << 24) |
         ((uint64_t)b[2] << 16) |
         ((uint64_t)b[1] << 8)  |
         ((uint64_t)b[0]);
}

// ============================================================================
// Below is to support the sketch - only make changes if the notes say so ...

// copy over the EUI's & keys in to the something that will not compile if incorrectly formatted
// build a byte array first (can't pass a braced-init-list directly to the function)

// devEUI: if you want to provide 8 raw bytes, define RADIOLIB_LORAWAN_DEV_EUI as 8 comma-separated bytes.
// If RADIOLIB_LORAWAN_DEV_EUI is a single integer, keep the cast below.
/* Example for 8-byte DEV EUI:
   #define RADIOLIB_LORAWAN_DEV_EUI 01,02,03,04,05,06,07,08
   then use:
     static const uint8_t devEUI_bytes[8] = FORMAT32_KEY(RADIOLIB_LORAWAN_DEV_EUI);
     static const uint64_t devEUI = bytes_to_u64_be(devEUI_bytes);
*/
