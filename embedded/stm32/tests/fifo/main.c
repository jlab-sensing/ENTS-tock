/**
 * @file main.c
 * @brief Test application for the MB85RC1MT FRAM driver on TockOS
 *
 * Exercises basic FRAM read/write operations and boundary checks
 * using the Unity testing framework.
 *
 * Based on stm32/test/test_fram/test_fram.c from ENTS-node-firmware.
 *
 * @author Pritish Mahato
 * @date 2026-02-25
 *
 * Copyright (c) 2026 jLab, UCSC
 */

#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wmissing-declarations"

#include <stdio.h>
#include <string.h>

#include <unity.h>

#include <storage/fifo.h>

void setUp(void) { fifo_buffer_clear(); }

void tearDown(void) {}

void test_fifo_put_ValidData(void) {
  uint8_t data[] = {1, 2, 3, 4, 5};

  fram_status status = fifo_put(data, sizeof(data));

  TEST_ASSERT_EQUAL(FRAM_OK, status);
  TEST_ASSERT_EQUAL(1, fifo_buffer_len());
}

void test_fifo_put_Sequential(void) {
  const int niters = 20;

  // starting values
  uint8_t data[10] = {0, 1, 2, 3, 4};

  // write 100 times, therefore 1100 bytes (data + len)
  for (int i = 0; i < niters; i++) {
    fram_status status = fifo_put(data, sizeof(data));
    TEST_ASSERT_EQUAL(FRAM_OK, status);

    // increment index of data
    for (unsigned int j = 0; j < sizeof(data); j++) {
      data[j]++;
    }
  }
}

void test_fifo_put_Sequential_BufferFull(void) {
  // starting values
  uint8_t data[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};

  const int niters = fifo_size() / (sizeof(data) + 1);

  // write 100 times, therefore 1100 bytes (data + len)
  for (int i = 0; i < niters; i++) {
    fram_status status_zero = fifo_put(data, sizeof(data));
    TEST_ASSERT_EQUAL(FRAM_OK, status_zero);
  }

  fram_status status_full = fifo_put(data, sizeof(data));
  TEST_ASSERT_EQUAL(FRAM_BUFFER_FULL, status_full);
}

void test_fifo_BufferEmpty(void) {
  uint8_t data[fifo_size()];
  uint8_t data_len;

  fram_status status = fifo_get(data, &data_len);

  TEST_ASSERT_EQUAL(FRAM_BUFFER_EMPTY, status);
}

void test_fifo_ValidData(void) {
  unsigned int items = 5;
  uint8_t put_data[] = {1, 2, 3, 4, 5};
  fifo_put(put_data, items);
  
  uint8_t get_data[items];
  uint8_t get_data_len;
  fram_status status = fifo_get(get_data, &get_data_len);

  TEST_ASSERT_EQUAL(FRAM_OK, status);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(put_data, get_data, items);
}

void test_fifo_Sequential(void) {
  const int niters = 20;

  // starting values
  uint8_t put_data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  // write 100 times, therefore 1100 bytes (data+len)
  for (int i = 0; i < niters; i++) {
    fifo_put(put_data, sizeof(put_data));

    // increment index of data
    for (unsigned int j = 0; j < sizeof(put_data); j++) {
      put_data[j]++;
    }
  }

  // reset put_data to starting values
  // was unsure about the behavior of memcpy
  for (unsigned int i = 0; i < sizeof(put_data); i++) {
    put_data[i] = i;
  }

  uint8_t get_data[10] = {0};

  // read back all the data
  for (int i = 0; i < niters; i++) {
    uint8_t get_data_len = 0;
    fram_status status_get = fifo_get(get_data, &get_data_len);

    TEST_ASSERT_EQUAL(FRAM_OK, status_get);
    TEST_ASSERT_EQUAL_INT(10, get_data_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(put_data, get_data, sizeof(put_data));

    // increment index of data
    for (unsigned int j = 0; j < sizeof(put_data); j++) {
      put_data[j]++;
    }
  }
}

void test_fifo_Sequential_BufferFull(void) {
  fram_status status = FRAM_OK;

  // starting values
  uint8_t data[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};

  const int niters = (fifo_size() / (sizeof(data) + 1));

  // write 100 times, therefore 1100 bytes (data + len)
  for (int i = 0; i < niters; i++) {
    status = fifo_put(data, sizeof(data));
    TEST_ASSERT_EQUAL(FRAM_OK, status);

    // increment data
    for (unsigned int j = 0; j < sizeof(data); j++) {
      data[j] += sizeof(data);
    }
  }

  // try writing data
  status = fifo_put(data, sizeof(data));
  TEST_ASSERT_EQUAL(FRAM_BUFFER_FULL, status);
  // reset data
  for (unsigned int i = 0; i < sizeof(data); i++) {
    data[i] = i;
  }

  uint8_t get_data[sizeof(data)] = {0};

  for (int i = 0; i < niters; i++) {
    uint8_t get_data_len = 0;
    status = fifo_get(get_data, &get_data_len);

    TEST_ASSERT_EQUAL(FRAM_OK, status);
    TEST_ASSERT_EQUAL_INT(sizeof(data), get_data_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, get_data, sizeof(data));

    // increment data
    for (unsigned int j = 0; j < sizeof(data); j++) {
      data[j] += sizeof(data);
    }
  }
}

void test_fifo_buffer_len(void) {
  // Assuming that the buffer length is initially 0
  TEST_ASSERT_EQUAL(0, fifo_buffer_len());

  // Put some data into the buffer
  uint8_t data[] = {1, 2, 3, 4, 5};
  fifo_put(data, sizeof(data));

  // Now the buffer length should be equal to the size of the data
  TEST_ASSERT_EQUAL(1, fifo_buffer_len());
}

void test_fifo_buffer_clear(void) {
  // Put some data into the buffer
  uint8_t data[] = {1, 2, 3, 4, 5};
  fram_status status = fifo_put(data, sizeof(data));
  TEST_ASSERT_EQUAL(FRAM_OK, status);

  TEST_ASSERT_EQUAL(1, fifo_buffer_len());

  // Clear the buffer
  fifo_buffer_clear();

  // Now the buffer length should be 0
  TEST_ASSERT_EQUAL(0, fifo_buffer_len());
}

void test_fifo_wraparound(void) {
  // checks for errors when read addr > write addr
  fram_status status;

  // Set the memory after the FIFO buffer to a unique character to check OOB
  // memory write
  uint8_t oob_before = 0;
  uint8_t oob_after = 0;
  const uint8_t oob_check = 0xFF;
  status = fram_read(FRAM_BUFFER_END + 1, 1,
                    &oob_before);  // to be restored afterwards
  TEST_ASSERT_EQUAL(FRAM_OK, status);
  status = fram_write(FRAM_BUFFER_END + 1, &oob_check, 1);
  TEST_ASSERT_EQUAL(FRAM_OK, status);

  // write block size to handle length
  // block_size+1 must not be a factor of the FIFO's space
  uint8_t block_size = 7;
  while (fifo_size() % (block_size + 1) == 0) {
    block_size += 1;
  }

  // oob_check is reserved as a special character for determining
  // if data was written out of bounds
  TEST_ASSERT_NOT_EQUAL(block_size, oob_check);
  TEST_ASSERT_NOT_EQUAL(block_size + 1, oob_check);

  uint8_t junk_data[256];
  for (int i = 0; i < 256; i++) {
    junk_data[i] = i;
  }

  uint8_t buffer[256];
  uint8_t buffer_length;

  // move write to before the end of physical memory in FRAM
  // if the assigned FRAM memory is [0, 1769], then a block_size of 16 (+1
  // length byte) means that the 104th block starts at 1768 and must wrap
  // around. read 0, write 1768
  status = FRAM_OK;
  while (status != FRAM_BUFFER_FULL) {
    TEST_ASSERT_EQUAL(FRAM_OK, status);
    status = fifo_put(junk_data, block_size);
  }

  // advance read all the way to the end to make room for the wraparound
  // read 1768, write 1768
  while (fifo_buffer_len() != 0) {
    status = fifo_get(buffer, &buffer_length);
    TEST_ASSERT_EQUAL(FRAM_OK, status);
  }

  // write one block for the wraparound
  // read 1768, write 1768 + 17 = 14
  // [{68} 69 70 0 1 2 3 4 5 6 7 8 9 10 11 12 13] 14
  status = fifo_put(junk_data, block_size);
  TEST_ASSERT_EQUAL(FRAM_OK, status);

  // observe the wraparound
  // read 14, write 14
  status = fifo_get(buffer, &buffer_length);
  TEST_ASSERT_EQUAL(FRAM_OK, status);

  // test that the data was successfully retrieved across the wraparound
  TEST_ASSERT_EQUAL_UINT8_ARRAY(buffer, junk_data, block_size);

  // test that no data was written out of bounds
  status = fram_read(FRAM_BUFFER_END + 1, 1, &oob_after);
  TEST_ASSERT_EQUAL(FRAM_OK, status);
  TEST_ASSERT_EQUAL(oob_after, oob_check);
  status = fram_write(FRAM_BUFFER_END + 1, &oob_before, 1);  // restore
  TEST_ASSERT_EQUAL(FRAM_OK, status);

  // status = fifo_put(zeros, block_size);
  // TEST_ASSERT_EQUAL(FRAM_BUFFER_FULL, status);
}

void test_fifo_load_buffer_state(void) {
  // Clear the buffer and check initial state
  fram_status status = fifo_buffer_clear();
  TEST_ASSERT_EQUAL(FRAM_OK, status);

  // Add some data
  const uint8_t test_data[] = {0x11, 0x22, 0x33};
  for (int i = 0; i < 10; i++) {
    // note that fifo_put() calls fram_save_buffer_state()
    status = fifo_put(test_data, sizeof(test_data));
    TEST_ASSERT_EQUAL(FRAM_OK, status);
  }

  // Load the new buffer state
  status = fifo_load_buffer_state();
  TEST_ASSERT_EQUAL(FRAM_OK, status);

  // 
  uint8_t retrieved_data[sizeof(test_data)];
  uint8_t retrieved_len;
  for (int i = 0; i < 10; i++) {
    status = fifo_get(retrieved_data, &retrieved_len);
    TEST_ASSERT_EQUAL(FRAM_OK, status);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(test_data, retrieved_data, sizeof(test_data));
  }
}

void test_fifo_peek(void) {
  // Clear the buffer and check initial state
  fram_status status = fifo_buffer_clear();
  TEST_ASSERT_EQUAL(FRAM_OK, status);

  // Add some data
  const uint8_t test_data[] = {0xDE, 0xAD, 0xBE, 0xEF};
  for (int i = 0; i < 5; i++) {
    status = fifo_put(test_data, sizeof(test_data));
    TEST_ASSERT_EQUAL(FRAM_OK, status);
  }

  // Peek at each measurement without removing them
  uint8_t retrieved_data[sizeof(test_data)];
  uint8_t retrieved_len;
  for (int i = 0; i < 5; i++) {
    status = fifo_peek(i, retrieved_data, &retrieved_len);
    TEST_ASSERT_EQUAL(FRAM_OK, status);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(test_data, retrieved_data, sizeof(test_data));
  }

  // Ensure buffer length remains unchanged
  TEST_ASSERT_EQUAL(5, fifo_buffer_len());
}

void test_fifo_drop(void) {
  // Clear the buffer and check initial state
  fram_status status = fifo_buffer_clear();
  TEST_ASSERT_EQUAL(FRAM_OK, status);

  // Add some data
  const uint8_t test_data[] = {0xAA, 0xBB, 0xCC};
  for (int i = 0; i < 5; i++) {
    status = fifo_put(test_data, sizeof(test_data));
    TEST_ASSERT_EQUAL(FRAM_OK, status);
  }

  // Drop two measurements
  for (int i = 0; i < 2; i++) {
    status = fifo_drop();
    TEST_ASSERT_EQUAL(FRAM_OK, status);
  }

  // Check remaining buffer length
  TEST_ASSERT_EQUAL(3, fifo_buffer_len());

  // Retrieve remaining data and verify correctness
  uint8_t retrieved_data[sizeof(test_data)];
  uint8_t retrieved_len;
  for (int i = 0; i < 3; i++) {
    status = fifo_get(retrieved_data, &retrieved_len);
    TEST_ASSERT_EQUAL(FRAM_OK, status);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(test_data, retrieved_data, sizeof(test_data));
  }

  // Attempt to drop from an empty buffer
  status = fifo_drop();
  TEST_ASSERT_EQUAL(FRAM_BUFFER_EMPTY, status);
}


int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_fifo_buffer_clear);
  RUN_TEST(test_fifo_buffer_len);

  // test buffer functions
  RUN_TEST(test_fifo_put_ValidData);
  RUN_TEST(test_fifo_put_Sequential);
  RUN_TEST(test_fifo_put_Sequential_BufferFull);
  RUN_TEST(test_fifo_BufferEmpty);
  RUN_TEST(test_fifo_ValidData);
  RUN_TEST(test_fifo_Sequential);
  RUN_TEST(test_fifo_Sequential_BufferFull);
  RUN_TEST(test_fifo_wraparound);
  RUN_TEST(test_fifo_load_buffer_state);
  RUN_TEST(test_fifo_peek);
  RUN_TEST(test_fifo_drop);

  return UNITY_END();
}
