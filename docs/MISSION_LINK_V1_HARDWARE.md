# Mission Link V1 hardware baseline

## Selected platform

Mission Link V1 specifically targets the **Seeed Studio XIAO ESP32-S3 + Wio-SX1262 kit with the top-mounted B2B (board-to-board) connector**.

- MCU: XIAO ESP32-S3.
- Radio: SX1262 on the Wio-SX1262 extension board made for the XIAO ESP32-S3 kit.
- Physical arrangement: Wio-SX1262 sits on top of the XIAO and connects through the dedicated B2B connector.
- This is **not** the Wio-SX1262 variant for XIAO nRF52840, which uses through-hole headers and a different pin configuration.
- Core mOS firmware, protocol, and LoRa software remain in internal flash.
- The optional round display's TF/microSD slot is supplemental removable storage, not a location for the boot-critical OS or radio stack.

Seeed explicitly distinguishes the ESP32-S3 B2B variant from the nRF52840 through-hole variant. Use only the ESP32-S3 kit-specific board for V1.

## Connector and signal reference

The official Seeed schematic linked below is titled **Wio-SX1262 for XIAO V1.0**. Its 30-pin B2B connector routes the SX1262 signals between the two boards. The schematic labels the radio-side signals as:

- SPI: `LORA_SPI_SCK`, `LORA_SPI_MISO`, `LORA_SPI_MOSI`.
- Chip select: `LORA_SPI_NSS`.
- Reset: `LORA_RST`.
- Busy: `LORA_BUSY`.
- Interrupt: `LORA_DIO1`.
- RF switch control: `LORA_RF_SW1`.
- Power and ground: `VIN`, `+3V3`, and `GND`.

The corresponding GPIO assignments must be checked against the XIAO-side schematic for the same hardware revision before they are entered into firmware. Do not treat the 30-pin connector positions as XIAO edge-header GPIO numbers.

## Hardware revision record

The V1 product family is selected, but the exact physical revision fitted to each node still needs to be recorded from its board markings and matched to the schematic. Record:

- XIAO ESP32-S3 board revision.
- Wio-SX1262 silkscreen revision and board identifier.
- Matching official schematic/datasheet revision.
- Antenna/connector configuration supplied with the exact kit.
- Any display/TF-card board model and its separate interface/pinout.

## Official hardware references

- Seeed Studio kit introduction and explicit B2B-vs-through-hole distinction: https://wiki.seeedstudio.com/wio_sx1262_with_xiao_esp32s3_kit/
- Official Wio-SX1262 for XIAO V1.0 schematic (PDF): https://files.seeedstudio.com/products/SenseCAP/Wio_SX1262/Schematic_Diagram_Wio-SX1262_for_XIAO.pdf
- Seeed Studio XIAO ESP32-S3 + Wio-SX1262 Meshtastic hardware guide and B2B pin mapping: https://wiki.seeedstudio.com/xiao_esp32s3_%26_wio_SX1262_kit_for_meshtastic/
- Seeed Studio XIAO ESP32-S3 getting started and pin reference: https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/

The official schematic matching the physical board revision is the source of truth. A generic SX1262 pinout, the nRF52840 Wio variant, or the XIAO edge-header pin list alone is not sufficient to configure the radio adapter.

## Firmware configuration policy

The default firmware remains fail-closed while the physical revision and applicable Canadian operating conditions are not reviewed:

- `ML_RADIO_PINMAP_VERIFIED=0`.
- `ML_RADIO_RX_ENABLED=0`.
- `ML_RF_ENABLED=0`.

Do not add guessed GPIO assignments or operating parameters to the default build. Once the physical revision is recorded, the adapter can be reviewed against the matching official schematic. Any later radio test remains a separately reviewed stage.

## Remaining V1 gates

1. Record the exact Wio-SX1262 silkscreen revision from the hardware in hand.
2. Verify the XIAO-side GPIO mapping against the matching official schematic and adapter wiring.
3. Review applicable ISED Canada authorization and operating conditions for the exact module and intended use.
4. Complete authenticated encryption, nonce/replay protection, identity, and key lifecycle work before user traffic.
5. Validate firmware and native tests on the exact commit; hardware validation is separately recorded.

Selecting the top-mounted B2B kit identifies the V1 hardware variant; it does not by itself complete these gates or enable RF.
