# Stable core

Implements the following:
- storage
- communication
- watchdog for apps



## Makefile Options

Use the options when calling make as follows

```
make LORAWAN_DEV=0
```

### List of Options

| Options | Values | Default | Description |
| --- | --- | --- |
| `LORAWAN_DEV` | bool | `0` | Hardcodes LoRaWAN MAC parameters to pre-setup device for testing. |
| `TEST_USER_CONFIG` | bool | `0` | Hardcodes the user config for testing |
