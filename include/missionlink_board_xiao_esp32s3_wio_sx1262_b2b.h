#pragma once

// Mission Link V1 target:
// Seeed Studio XIAO ESP32-S3 + top-mounted Wio-SX1262 B2B kit.
//
// Candidate GPIO mapping transcribed from the user-supplied P2P sketch.
// These are recorded for review only and are NOT active radio pin macros.
// Verify every assignment against the exact board revision's official
// schematic and the physical module before promoting to ML_RADIO_*.
//
// Candidate mapping (user sketch):
//   SPI SCK   -> GPIO 7
//   SPI MISO  -> GPIO 8
//   SPI MOSI  -> GPIO 9
//   SX1262 NSS/CS -> GPIO 41
//   SX1262 RESET  -> GPIO 42
//   SX1262 BUSY   -> GPIO 40
//   SX1262 DIO1   -> GPIO 39
//
// The sketch also calls setDio2AsRfSwitch(true); this is a radio-internal
// DIO2 RF-switch configuration, not evidence for a separate GPIO 38 switch.
// Do not add an external RF-switch GPIO unless the exact schematic requires it.
//
// The sample's 915 MHz, 125 kHz, SF9, CR7, 22 dBm and sync-word settings are
// intentionally NOT adopted. Regional authorization and operating limits
// must be reviewed independently.
//
// This profile is not the nRF52840 through-hole Wio-SX1262 variant.
#define ML_BOARD_XIAO_ESP32S3_WIO_SX1262_B2B 1
#define ML_BOARD_RADIO_CONNECTOR_B2B 1
#define ML_BOARD_RADIO_CHIP_SX1262 1

// Candidate values are documentation-only constants; driver code must not
// consume them until verification is complete.
namespace ml {
namespace board_candidate {
static constexpr int kRadioSck = 7;
static constexpr int kRadioMiso = 8;
static constexpr int kRadioMosi = 9;
static constexpr int kRadioNss = 41;
static constexpr int kRadioReset = 42;
static constexpr int kRadioBusy = 40;
static constexpr int kRadioDio1 = 39;
}  // namespace board_candidate
}  // namespace ml

#if defined(ML_RADIO_PINMAP_VERIFIED) && ML_RADIO_PINMAP_VERIFIED
#error "Candidate B2B GPIO mapping is not yet verified against the physical board; radio initialization must remain disabled"
#endif
