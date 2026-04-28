# Unit Tests

Unit tests are written using the [ThrowTheSwitch/Unity](https://www.throwtheswitch.org/unity). The [github repo](https://github.com/ThrowTheSwitch/Unity) has a good quick reference for assert statements.

## Adding a new tests

Make a copy of `./template` directory naming it such that it matches the source files that are being tested.

### Write individual tests

See the [github repo](https://github.com/ThrowTheSwitch/Unity) for a full list of ASSERT statements.

```c
void test_template(void) {
  int two = 1+1;
  TEST_ASSERT_EQUAL_INT(2, two);
}
```

### Add the test function to `main()`

```c
int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_template);

  return UNITY_END();
}
```

## Running tests

From a tests folder (ie. `./template/`):

1. Clear existing application

```
tockloader erase-apps
```

2. Build and install the test

```
make install
```

3. Open the serial monitor

```
tockloader listen
```

4. Observe serial output

```
Initialization complete. Entering main loop...
main.c:43:test_template:PASS

-----------------------
1 Tests 0 Failures 0 Ignored 
OK
```
