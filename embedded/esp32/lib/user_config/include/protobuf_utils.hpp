#ifndef PROTOBUF_UTILS_H
#define PROTOBUF_UTILS_H

#include <libents/proto/controller.pb.h>
#include <libents/proto/transcoder.h>
#include <stdint.h>

void printEncodedData(const uint8_t* data, size_t len);
void printDecodedConfig(const UserConfiguration* config);

#endif
