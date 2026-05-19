#include "irrigation.h"

#include "../communication.h"

#include "solenoid.h"
#include "transcoder.h"

extern unsigned int g_controller_i2c_timeout;

ControllerStatus IrrigationCommandTransaction(const IrrigationCommand *input,
                                              IrrigationCommand *output) {
  // get reference to tx and rx buffers
  Buffer *tx = ControllerTx();
  Buffer *rx = ControllerRx();

  // encode command
  tx->len = EncodeIrrigationCommand(input, tx->data, tx->size);

  // send transaction
  ControllerStatus status = CONTROLLER_SUCCESS;
  status = ControllerTransaction(g_controller_i2c_timeout);
  if (status != CONTROLLER_SUCCESS) {
    return status;
  }

  // check for errors
  if (rx->len == 0) {
    return CONTROLLER_ERROR;
  }

  // decode command
  Esp32Command cmd = Esp32Command_init_default;
  cmd = DecodeEsp32Command(rx->data, rx->len);

  // copy output data
  memcpy(output, &cmd.command.irrigation_command, sizeof(IrrigationCommand));

  return CONTROLLER_SUCCESS;
}

IrrigationCommand_State ControllerIrrigationCheck(void) {
  IrrigationCommand irrigation_cmd = IrrigationCommand_init_zero;
  irrigation_cmd.type = IrrigationCommand_Type_CHECK;

  IrrigationCommand resp = IrrigationCommand_init_zero;

  if (IrrigationCommandTransaction(&irrigation_cmd, &resp) !=
      CONTROLLER_SUCCESS) {
    return IrrigationCommand_State_OPEN;
  }

  // Add verbose output
  const char *state_name = "UNKNOWN";
  switch (resp.state) {
    case IrrigationCommand_State_OPEN:
      state_name = "OPEN";
      break;
    case IrrigationCommand_State_CLOSE:
      state_name = "CLOSE";
      break;
    default:
      state_name = "ERROR";
      break;
  }

  // Control solenoid based on the received state
  switch (resp.state) {
    case IrrigationCommand_State_OPEN:
      SolenoidOpen();
      break;
    case IrrigationCommand_State_CLOSE:
      SolenoidClose();
      break;
    default:
      break;
  }

  return resp.state;
}
