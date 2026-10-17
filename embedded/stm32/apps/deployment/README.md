# Deployment App

Single app for field deployments that combines the [core](../core) and
[sensors](../sensors) apps. Rather than passing measurements between two apps
over IPC, this app measures the enabled sensors and uploads the data itself.

Implements the following:
- user config webserver
- measurement of sensors enabled in the user config
- storage of measurements in the FRAM buffer
- upload over LoRaWAN (or a heartbeat when there is nothing to upload)

Each loop measures all enabled sensors, uploads the buffered measurements, then
waits `Upload_interval` seconds from the user config.

## Makefile Options

Use the options when calling make as follows

```
make LORAWAN_DEV=0
```

### List of Options

| Options | Values | Default | Description |
| --- | --- | --- | --- |
| `LORAWAN_DEV` | bool | `0` | Hardcodes LoRaWAN MAC parameters to pre-setup device for testing. |
| `TEST_USER_CONFIG` | bool | `0` | Hardcodes the user config for testing |
