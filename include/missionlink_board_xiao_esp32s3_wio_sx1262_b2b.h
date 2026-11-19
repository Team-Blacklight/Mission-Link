#pragma once

// Mission Link V1 board identity:
// Seeed Studio XIAO ESP32-S3 + Wio-SX1262 top-mounted B2B kit.
//
// This profile deliberately records the connector variant without assigning
// GPIO numbers. The B2B signal-to-GPIO mapping must be transcribed from the
// matching official schematic and checked against the physical board revision
// before ML_RADIO_PINMAP_VERIFIED may be set to 1.
//
// Radio-side signal names documented by Seeed:
//   LORA_SPI_SCK, LORA_SPI_MISO, LORA_SPI_MOSI
//   LORA_SPI_NSS, LORA_RST, LORA_BUSY, LORA_DIO1, LORA_RF_SW1
//
// Do not substitute the XIAO nRF52840 through-hole Wio-SX1262 variant.

#define ML_BOARD_XIAO_ESP32S3_WIO_SX1262_B2B 1
#define ML_BOARD_RADIO_CONNECTOR_B2B 1
#define ML_BOARD_RADIO_CHIP_SX1262 1

#if defined(ML_RADIO_PINMAP_VERIFIED) && ML_RADIO_PINMAP_VERIFIED
#error "B2B board profile has no verified GPIO map yet; do not enable radio initialization"
#endif
