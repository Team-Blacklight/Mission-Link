# Mission Link V1 hardware baseline

## Selected platform

Mission Link V1 targets the Seeed Studio XIAO ESP32-S3 paired with the Seeed Wio-SX1262 extension kit.

- MCU platform: XIAO ESP32-S3.
- LoRa transceiver: SX1262 on the Wio-SX1262 extension board supplied for the XIAO ESP32-S3 kit.
- MCU-to-radio connection: board-to-board connector; the SX1262 uses SPI.
- Core mOS firmware, protocol, and LoRa software remain in internal flash.
- The optional round display's TF/microSD slot is supplemental removable storage, not a location for the boot-critical OS or radio stack.

## Hardware revision must be recorded

The kit family is now selected, but the exact Wio-SX1262 board revision fitted to each physical node must still be recorded from its board markings and checked against the matching Seeed schematic. Do not infer that every Wio-SX1262 board sold for another XIAO variant is interchangeable: Seeed documents a kit-specific module and connector arrangement.

Record for each V1 hardware build:
- XIAO ESP32-S3 board revision.
- Wio-SX1262 silkscreen revision and board identifier.
- Matching official schematic/datasheet revision.
- Antenna/connector configuration supplied with the exact kit.
- Any display/TF-card board model and its separate interface/pinout.

## Official hardware references

- Seeed Studio: XIAO ESP32S3 & Wio-SX1262 Kit Introduction: https://wiki.seeedstudio.com/wio_sx1262_with_xiao_esp32s3_kit/
- Seeed Studio: MeshCore hardware connection guide: https://wiki.seeedstudio.com/get_started_with_esp32s3_meshcore/
- Seeed Studio: XIAO ESP32-S3 getting started and pin reference: https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/

Use the official schematic and module datasheet matching the physical board revision as the source of truth. A general XIAO pinout alone is not sufficient to configure the radio adapter.

## Firmware configuration policy

The default firmware remains fail-closed while the physical revision and applicable Canadian operating conditions are not reviewed:
- `ML_RADIO_PINMAP_VERIFIED=0`.
- `ML_RADIO_RX_ENABLED=0`.
- `ML_RF_ENABLED=0`.

Do not add guessed GPIO assignments or RF operating parameters to the default build. Once the exact board revision is documented, the adapter can be reviewed against the matching official schematic. Any later radio test remains a separate, explicitly reviewed stage.

## Remaining V1 gates

1. Record the exact Wio-SX1262 silkscreen revision from the hardware in hand.
2. Verify the matching official schematic and adapter wiring.
3. Review applicable ISED Canada authorization and operating conditions for the exact module and intended use.
4. Complete authenticated encryption, nonce/replay protection, identity, and key lifecycle work before user traffic.
5. Validate firmware and native tests on the exact commit; hardware validation is separately recorded.

Selecting the kit identifies the platform; it does not by itself complete these gates or enable RF.