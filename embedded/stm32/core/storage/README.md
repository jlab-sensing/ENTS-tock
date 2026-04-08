FRAM Test (MB85RC1MT)
=====================

This application tests the MB85RC1MT FRAM driver over I2C on the ENTS node
running TockOS.

## Hardware Requirements

- ENTS node board with MB85RC1MT FRAM chip connected via I2C
- TockOS kernel with I2C master driver enabled

## Building

```bash
cd examples/tests/fram
make
```

## Flashing

```bash
tockloader install build/fram.tab
```

## Expected Output

```
========================================
ENTS FRAM Test (MB85RC1MT via TockOS)
========================================

FRAM size: 131072 bytes

[PASS] fram_size() == 131072
[PASS] fram_write valid data
[PASS] fram_write zero length
[PASS] fram_write out of range
[PASS] fram_read status OK
[PASS] fram_read zero length
[PASS] fram_read out of range
[PASS] fram_read matches written data
[PASS] multi-address: pattern A
[PASS] multi-address: pattern B

========================================
Results: 10 passed, 0 failed
========================================
ALL TESTS PASSED
```


## Notes

- Ported from `stm32/test/test_fram/test_fram.c` in the ENTS-node-firmware
  baremetal codebase.
- The driver uses byte-at-a-time I2C writes for correctness across address
  boundaries. Future optimization could use bulk writes within a single page.
- The `HAL_I2C_Mem_Write` / `HAL_I2C_Mem_Read` calls were replaced with
  `i2c_master_write_sync` / `i2c_master_write_read_sync` from libtock-c.
