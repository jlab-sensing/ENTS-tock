Firmware for the ENTS node: the STM32 running Tock OS and the sensing
applications, plus the ESP32-C3 coprocessor that handles WiFi, the
configuration portal, and the microSD card.

**[← All ENTS-tock documentation](../index.html)** — getting started, the
python package, and the DirtViz API client live one level up.

New to the project? Start with the
[getting started guide](../../docs/getting_started.md).

## Where things are

- **Toolchain setup, flashing, configuring a board** — [Getting started](../../docs/getting_started.md)
- **Sensing applications and libs on the STM32** — @ref stm32
- **WiFi / config portal / microSD coprocessor** — @ref esp32
- **The shared library both sides build on** — @ref libents
- **Every README in the repository** — [Pages](pages.html)

## Guides

\subpage md_docs_2getting__started "Getting started"

## STM32 — Tock OS applications

\subpage md_embedded_2stm32_2README

\subpage md_embedded_2stm32_2apps_2README

\subpage md_embedded_2stm32_2apps_2config_2README

\subpage md_embedded_2stm32_2apps_2core_2README

\subpage md_embedded_2stm32_2apps_2reset_2README

\subpage md_embedded_2stm32_2apps_2sensors_2README

\subpage md_embedded_2stm32_2apps_2template_2README

\subpage md_embedded_2stm32_2examples_2README

\subpage md_embedded_2stm32_2examples_2ads1219_2README

\subpage md_embedded_2stm32_2examples_2bme280_2README

\subpage md_embedded_2stm32_2examples_2microsd__log_2README

\subpage md_embedded_2stm32_2examples_2template_2README

\subpage md_embedded_2stm32_2tests_2README

## ESP32 — coprocessor

\subpage md_embedded_2esp32_2README

\subpage md_embedded_2esp32_2lib_2module__handler_2README

\subpage md_embedded_2esp32_2lib_2user__config_2README

## libents — shared library

\subpage md_embedded_2libents_2README

\subpage md_embedded_2libents_2src_2libents_2controller_2README

\subpage md_embedded_2libents_2src_2libents_2sensors_2bme280_2README

\subpage md_embedded_2libents_2src_2libents_2storage_2README

## Building these docs

\subpage md_docs_2README

\subpage md_embedded_2README

\subpage md_embedded_2doxygen_2README
