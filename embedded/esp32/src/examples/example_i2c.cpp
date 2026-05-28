 #include <Arduino.h>
#include "Wire.h"

/** Target device address */
static const uint8_t dev_addr = 0x20;
/** Serial data pin */
static const int sda_pin = 0;
/** Serial clock pin */
static const int scl_pin = 1;


uint8_t i = 0;

uint8_t buffer[32] = "world";


void onRequest() {
  // prewrite to buffer
  Wire.write(buffer, 5);

  Serial.printf("onRequest: %02x\n", i++);
}

void onReceive(int len) {
  Serial.printf("onReceive[%d]: ", len);

  while (Wire.available()) {
    Serial.write(Wire.read());
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);

  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);

  Wire.begin(dev_addr, sda_pin, scl_pin, 400000); 
}

void loop() {}
