# FIFO Insert Test App

This helper app writes a small sample payload into the STM32 FIFO buffer so you can test that the buffer is initialized correctly and that clearing the buffer works as expected.

## What it does

When the app starts, it:

- initializes the FIFO storage layer,
- inserts a fixed test payload into the FIFO,
- prints the FIFO length so you can verify that the data was stored.

## How to use it

1. Build and install the app:

   ```bash
   cd embedded/stm32/apps/fifo_insert
   make install
   ```

2. Listen for serial output:

   ```bash
   tockloader listen
   ```

3. Reset the board and watch the serial output for the FIFO length.

4. To test buffer clearing, use the user configuration flow and enable the clear-buffer option, then verify that the FIFO length resets to zero after the configuration is applied.

## Related documentation

For the user configuration flow and the clear-buffer behavior, see the configuration app README:

- [embedded/esp32/docs/testing-webui.md](../../../esp32/docs/testing-webui.md)
