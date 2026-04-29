/**
 * @file lorawan.cc
 *
 * @author Tyler Potyondy
 * @author John Madden (jmadden173@pm.me)
 */

#include "lorawan.h"

#include <ulog.h>
// include the library
// #include <RadioLib.h>

// include the hardware abstraction layer
//
#include "config.h"
// Include some libtock-c helpers



static TockRadioLibHal* hal;
static Module* mod;
static SX1262* tock_module;
static LoRaWANNode* node;

// the entry point for the program
int lorawan_init(void) {
  ulog_trace("lorawan_setup");

  hal = new TockRadioLibHal();
  mod = new Module(hal, RADIOLIB_RADIO_NSS, RADIOLIB_RADIO_DIO_1, RADIOLIB_RADIO_RESET,
                                 RADIOLIB_RADIO_BUSY);
  tock_module = new SX1262(mod);

  // print const LoRaWANBand_t US915
  ulog_info("Region: %d\t Sub Band: %d", Region->txSpans->numChannels, Region->freqMin);
 
  // create the LoRaWAN node
  node = new LoRaWANNode(tock_module, Region, subBand);
  // now we can create the radio module
  // pinout corresponds to the SparkFun LoRa Thing Plus - expLoRaBLE
  // NSS pin:   0
  // DIO1 pin:  2
  // NRST pin:  4
  // BUSY pin:  1

  // Setup the radio
  if (tock_module == nullptr) {
    ulog_fatal("Error when initializing tock module.");
    return -1;
  }
  ulog_trace("a");
  int state = tock_module->begin();
  ulog_trace("b");

  // check node?
  if (node == nullptr) {
    ulog_fatal("Error when initializing node.");
    return -1;
  }

  // check final state
  if (state != RADIOLIB_ERR_NONE) {
    ulog_fatal("Initialization failed.");
    return -1;
  }

  return 0;
}

int lorawan_join(void) {
  ulog_trace("lorawan_join");

  // status codes 
  int state = 0;

  static const uint8_t devEUI_bytes[8] = FORMAT32_KEY(RADIOLIB_LORAWAN_DEV_EUI);
  static const uint64_t devEUI  = bytes_to_u64_be(devEUI_bytes);
  uint8_t appKey[16] = FORMAT_KEY(RADIOLIB_LORAWAN_APP_KEY);
 
  static const uint8_t joinEUI_bytes[8] = FORMAT32_KEY(RADIOLIB_LORAWAN_JOIN_EUI);
  static const uint64_t joinEUI = bytes_to_u64_be(joinEUI_bytes);

  static const uint8_t nwkKey[16] = FORMAT_KEY(RADIOLIB_LORAWAN_NWK_KEY);

  node->scanGuard = 100;

  // Setup the OTAA session info
  state = node->beginOTAA(joinEUI, devEUI, nwkKey, appKey);

  if (state != RADIOLIB_ERR_NONE) {
    ulog_fatal("LoRaWAN OTAA setup failed, code %d", state);
    return 1;
  }
  ulog_debug("LoRaWAN OTAA setup success!");

  state = node->activateOTAA();
  while (state != RADIOLIB_ERR_NONE && state != RADIOLIB_LORAWAN_NEW_SESSION) {
    ulog_error("LoRaWAN OTAA activation failed, code %d. Delaying 5 seconds, then retrying.", state);
    hal->delay(5000);
  }

  ulog_debug("LoRaWAN OTAA activation success!");

  return 0;
}

int lorawan_timesync(void) {
  ulog_trace("lorawan_timesync");

  ulog_warn("Timesync not implemented.");

  return 0;
}

int lorawan_upload(uint8_t* buffer, int length) {
  ulog_trace("lorawan_upload");

  int state = 0;
  
  static int counter = 0; 

  // Ensure there are no pending callbacks
  yield_no_wait();

  // Form payload to send
  ulog_debug("Sending uplink of %lu bytes: %s\r\n", length, buffer);

  state = node->sendReceive(buffer, length);
  ulog_debug("LoRaWAN send/receive code %d.", state);
  if (state != RADIOLIB_ERR_NONE) {
    ulog_error("Upload failed.");
    return -1;
  }

  // Wait until next uplink - observing legal & TTN FUP constraints
  hal->delay(uplinkIntervalSeconds * 1000UL);

  counter++;

  return state;
}
