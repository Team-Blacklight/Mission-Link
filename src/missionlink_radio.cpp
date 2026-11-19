#include "missionlink_radio.h"
#include "missionlink_config.h"
#include "missionlink_packet.h"
#include "missionlink_board_xiao_esp32s3_wio_sx1262_b2b.h"

#ifndef ML_RADIO_PINMAP_VERIFIED
#define ML_RADIO_PINMAP_VERIFIED 0
#endif
#ifndef ML_RADIO_RX_ENABLED
#define ML_RADIO_RX_ENABLED 0
#endif
#ifndef ML_RADIO_ANT_SW
#define ML_RADIO_ANT_SW 38
#endif

#if ML_RADIO_PINMAP_VERIFIED
#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

// These must come from a verified board-specific header. No pin or RF defaults
// are guessed here: compilation fails if verification is asserted without them.
#ifndef ML_RADIO_SCK
#error "Verified radio pin map must define ML_RADIO_SCK"
#endif
#ifndef ML_RADIO_MISO
#error "Verified radio pin map must define ML_RADIO_MISO"
#endif
#ifndef ML_RADIO_MOSI
#error "Verified radio pin map must define ML_RADIO_MOSI"
#endif
#ifndef ML_RADIO_CS
#error "Verified radio pin map must define ML_RADIO_CS"
#endif
#ifndef ML_RADIO_DIO1
#error "Verified radio pin map must define ML_RADIO_DIO1"
#endif
#ifndef ML_RADIO_RESET
#error "Verified radio pin map must define ML_RADIO_RESET"
#endif
#ifndef ML_RADIO_BUSY
#error "Verified radio pin map must define ML_RADIO_BUSY"
#endif
#ifndef ML_RADIO_FREQ_MHZ
#error "Set the legally verified regional frequency in the board config"
#endif
#ifndef ML_RADIO_BANDWIDTH_KHZ
#error "Set the verified radio bandwidth in the board config"
#endif
#ifndef ML_RADIO_SPREADING_FACTOR
#error "Set the verified spreading factor in the board config"
#endif
#ifndef ML_RADIO_CODING_RATE
#error "Set the verified coding rate in the board config"
#endif
#ifndef ML_RADIO_SYNC_WORD
#error "Set the verified network sync word in the board config"
#endif
#ifndef ML_RADIO_TX_POWER_DBM
#error "Set the verified compliant TX power in the board config"
#endif

static Module radioModule(ML_RADIO_CS, ML_RADIO_DIO1, ML_RADIO_RESET, ML_RADIO_BUSY);
static SX1262 radio(&radioModule);
#endif

namespace ml {
MissionLinkRadio::MissionLinkRadio()
    : state_(ML_RADIO_PINMAP_VERIFIED ? RadioState::Ready : RadioState::PinmapUnverified),
      initialized_(false), receiving_(false), lastRssi_(0), lastSnr_(0) {
#if !ML_RADIO_PINMAP_VERIFIED
  state_ = RadioState::PinmapUnverified;
#elif !ML_RADIO_RX_ENABLED && !ML_RF_ENABLED
  state_ = RadioState::Disabled;
#endif
}

bool MissionLinkRadio::beginReceive() {
#if !ML_RADIO_PINMAP_VERIFIED
  state_ = RadioState::PinmapUnverified;
  return false;
#elif !ML_RADIO_RX_ENABLED && !ML_RF_ENABLED
  state_ = RadioState::Disabled;
  return false;
#else
  if (!initialized_) {
    // The Wio-SX1262 RF path is switched separately from the SX1262 itself.
    pinMode(ML_RADIO_ANT_SW, OUTPUT);
    digitalWrite(ML_RADIO_ANT_SW, HIGH);
    SPI.begin(ML_RADIO_SCK, ML_RADIO_MISO, ML_RADIO_MOSI, ML_RADIO_CS);
    const int16_t rc = radio.begin(ML_RADIO_FREQ_MHZ, ML_RADIO_BANDWIDTH_KHZ,
                                   ML_RADIO_SPREADING_FACTOR, ML_RADIO_CODING_RATE,
                                   ML_RADIO_SYNC_WORD, ML_RADIO_TX_POWER_DBM,
                                   8, 1.6);
    if (rc != RADIOLIB_ERR_NONE) {
      state_ = RadioState::ReceiveError;
      initialized_ = false;
      receiving_ = false;
      return false;
    }
    initialized_ = true;
  }
#if ML_RADIO_RX_ENABLED
  const int16_t rc = radio.startReceive();
  if (rc != RADIOLIB_ERR_NONE) {
    state_ = RadioState::ReceiveError;
    receiving_ = false;
    return false;
  }
  receiving_ = true;
#else
  // TX-only builds still need the radio initialized, but do not enter RX mode.
  receiving_ = false;
#endif
  state_ = RadioState::Ready;
  return true;
#endif
}

bool MissionLinkRadio::poll(Packet& packet) {
#if !ML_RADIO_PINMAP_VERIFIED || !ML_RADIO_RX_ENABLED
  (void)packet;
  return false;
#else
  if (!initialized_ || !receiving_ || !radio.available()) return false;
  const size_t length = radio.getPacketLength();
  if (length < 32 || length > 32 + ML_MAX_PAYLOAD) {
    const int16_t restartRc = radio.startReceive();
    receiving_ = restartRc == RADIOLIB_ERR_NONE;
    state_ = receiving_ ? RadioState::DecodeError : RadioState::ReceiveError;
    return false;
  }
  uint8_t buffer[32 + ML_MAX_PAYLOAD];
  const int16_t rc = radio.readData(buffer, length);
  lastRssi_ = static_cast<int16_t>(radio.getRSSI());
  lastSnr_ = static_cast<int8_t>(radio.getSNR());
  const int16_t restartRc = radio.startReceive();
  receiving_ = restartRc == RADIOLIB_ERR_NONE;
  if (!receiving_) {
    state_ = RadioState::ReceiveError;
    return false;
  }
  if (rc != RADIOLIB_ERR_NONE || !decode(buffer, length, packet)) {
    state_ = RadioState::DecodeError;
    return false;
  }
  state_ = RadioState::Ready;
  return true;
#endif
}

bool MissionLinkRadio::transmit(const Packet& packet) {
#if !ML_RADIO_PINMAP_VERIFIED || !ML_RF_ENABLED
  (void)packet;
  state_ = RadioState::TransmitBlocked;
  return false;
#else
  if (!initialized_) {
    state_ = RadioState::TransmitBlocked;
    return false;
  }
  std::vector<uint8_t> frame;
  if (!encode(packet, frame)) {
    state_ = RadioState::TransmitError;
    return false;
  }
  receiving_ = false;
  const int16_t rc = radio.transmit(frame.data(), frame.size());
  if (rc != RADIOLIB_ERR_NONE) {
    if (ML_RADIO_RX_ENABLED) {
      receiving_ = radio.startReceive() == RADIOLIB_ERR_NONE;
      state_ = receiving_ ? RadioState::TransmitError : RadioState::ReceiveError;
    } else {
      receiving_ = false;
      state_ = RadioState::TransmitError;
    }
    return false;
  }
  if (ML_RADIO_RX_ENABLED) {
    receiving_ = radio.startReceive() == RADIOLIB_ERR_NONE;
    state_ = receiving_ ? RadioState::Ready : RadioState::ReceiveError;
  } else {
    receiving_ = false;
    state_ = RadioState::Ready;
  }
  return true;
#endif
}
RadioStatus MissionLinkRadio::status() const {
  return {state_, initialized_, receiving_, lastRssi_, lastSnr_};
}
}
