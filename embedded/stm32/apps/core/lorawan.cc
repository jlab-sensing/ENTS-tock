/**
 * @file lorawan.cc
 *
 * @author Tyler Potyondy
 * @author John Madden (jmadden173@pm.me)
 */

#include "lorawan.h"

#include <libents/storage/fram.h>
#include <libtock-sync/services/alarm.h>
#include <libtock/net/eui64.h>
#include <ulog.h>

#include "RadioLib.h"
#include "config.h"

// address in fram to store persistant buffer
#define LORAWAN_NONCES_ADDR 2200

// Amount of time between connection retries
static const int retry_ms = 15000;

// LoRaWAN upload port
static const uint8_t fport = 2;

static TockRadioLibHal* hal;
static Module* mod;
static SX1262* tock_module;
static LoRaWANNode* node;

// the entry point for the program
int lorawan_init(void) {
  ulog_trace("lorawan_setup");

  hal = new TockRadioLibHal();
  mod = new Module(hal, RADIOLIB_RADIO_NSS, RADIOLIB_RADIO_DIO_1,
                   RADIOLIB_RADIO_RESET, RADIOLIB_RADIO_BUSY);
  tock_module = new SX1262(mod);

  // print const LoRaWANBand_t US915
  ulog_info("Region: %d\t Sub Band: %d", Region->txSpans->numChannels,
            Region->freqMin);

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
  int state = tock_module->begin();

  // check node?
  if (node == nullptr) {
    ulog_fatal("Error when initializing node.");
    return -1;
  }

  // check final state
  if (state != RADIOLIB_ERR_NONE) {
    ulog_fatal("Initialization failed. State = %d.", state);
    return -1;
  }

  return 0;
}

int lorawan_join(void) {
  ulog_trace("lorawan_join");

  // lorawan state codes
  int state = 0;

  // status code
  int status = 0;

#ifdef STATIC_DEV_EUI
  ulog_info("Using DevEUI from #define.");

  static const uint8_t devEUI_bytes[8] = FORMAT32_KEY(RADIOLIB_LORAWAN_DEV_EUI);
  static const uint64_t devEUI = bytes_to_u64_be(devEUI_bytes);
#else
  ulog_info("Using DevEUI from flash.");

  // check driver exists
  if (!libtock_eui64_exists()) {
    ulog_error("EUI64 driver does not exist!");
  }

  // get devEUI from flash
  uint64_t devEUI = 0;
  libtock_eui64_get(&devEUI);

  uint8_t devEUI_bytes[8] = {};
  u64_to_bytes_be(devEUI, devEUI_bytes);
#endif  // RADIOLIB_LORAWAN_DEV_EUI

  ulog_info("DevEUI: %02x %02x %02x %02x %02x %02x %02x %02x", devEUI_bytes[0],
            devEUI_bytes[1], devEUI_bytes[2], devEUI_bytes[3], devEUI_bytes[4],
            devEUI_bytes[5], devEUI_bytes[6], devEUI_bytes[7]);

  uint8_t appKey[16] = FORMAT_KEY(RADIOLIB_LORAWAN_APP_KEY);
  static const uint8_t joinEUI_bytes[8] =
      FORMAT32_KEY(RADIOLIB_LORAWAN_JOIN_EUI);
  static const uint64_t joinEUI = bytes_to_u64_be(joinEUI_bytes);

  static const uint8_t nwkKey[16] = FORMAT_KEY(RADIOLIB_LORAWAN_NWK_KEY);

  node->scanGuard = 100;

  //
  // Setup the OTAA session info
  //
  state = node->beginOTAA(joinEUI, devEUI, nwkKey, appKey);

  if (state != RADIOLIB_ERR_NONE) {
    ulog_fatal("LoRaWAN OTAA setup failed, code %d", state);
    return 1;
  }
  ulog_debug("LoRaWAN OTAA setup success!");

  // get pointer to the buffer
  uint8_t persistant_nonces[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
  status = fram_read(LORAWAN_NONCES_ADDR, RADIOLIB_LORAWAN_NONCES_BUF_SIZE,
                     persistant_nonces);

  if (status < 0) {
    ulog_error("Could not read nonces data from persistant storage.");
  } else {
    state = node->setBufferNonces(persistant_nonces);
    if (state < 0) {
      if (state == RADIOLIB_ERR_NONCES_DISCARDED) {
        ulog_error("LoRaWAN nonces is invalid.");
      } else {
        ulog_error("Could not set LoRaWAN nonces buffer. RadioLib state = %d",
                   state);
      }
    }
  }

  //
  // Initiate join request
  //
  state = node->activateOTAA();
  while (state != RADIOLIB_ERR_NONE && state != RADIOLIB_LORAWAN_NEW_SESSION) {
    ulog_error(
        "LoRaWAN OTAA activation failed, code %d. Delaying %d seconds, then "
        "retrying.",
        state, retry_ms / 1000);
    hal->delay(retry_ms);
  }

  ulog_debug("LoRaWAN OTAA activation success!");

  uint8_t* current_nonces = node->getBufferNonces();
  status = fram_write(LORAWAN_NONCES_ADDR, current_nonces,
                      RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
  if (status < 0) {
    ulog_error("Could not write nonces data from persistant storage.");
  }

  // set configuration for future uploads

  // use ADR but set initial data rate
  node->setADR(true);
  node->setDatarate(2);

  // TTN fair use
  // node->setDutyCycle(true, 1250);
  // dwell time limit for US
  node->setDwellTime(true, 400);

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
  ulog_debug("Sending uplink of %lu bytes.", length, buffer);

  // state indicates there was a downlink received
  state = node->sendReceive(buffer, (size_t)length, fport);
  ulog_debug("LoRaWAN send/receive code %d.", state);
  if (state < 0) {
    ulog_error("Upload failed.");
    return -1;
  }

  // Wait until next uplink - observing legal & TTN FUP constraints
  // hal->delay(uplinkIntervalSeconds * 1000UL);

  counter++;

  return state;
}
