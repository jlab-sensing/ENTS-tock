# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/2.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Added a clear-buffer option to the ESP32 web configuration flow so users can request that stored measurements be cleared when saving a new configuration.
- Propagated the new clear-buffer flag through the user-config protobuf schema and configuration handling path between the ESP32 and STM32.
- Updated the STM32 configuration startup flow to honor the clear-buffer request, clear the FRAM/FIFO measurement buffer, and reset the flag so it does not repeat on the next boot.
- STM32 core now initializes the FRAM-backed FIFO state on startup with fifo_init() so previously persisted buffer state is restored correctly.
- Added a new STM32 test app for inserting sample payloads into the FIFO buffer for manual verification.

### Changed
- Improved the web UI and configuration save flow to make the buffer-clearing behavior explicit to the user.
- Clarified the STM32-side behavior and logging around clearing the measurement buffer.

## [3.0.0] - 2026-06-19

[3.0.0](https://github.com/jlab-sensing/ENTS-tock/compare/v1.1.2...v2.0.0)

Minimum viable product for measuring power and bme280 with data sent over LoRaWAN.

