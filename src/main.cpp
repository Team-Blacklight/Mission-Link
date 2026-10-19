#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <cstring>
#include <esp_mac.h>
#include "missionlink_config.h"
#include "mesh_router.h"
#include "missionlink_radio.h"

static const char* BLE_NAME = "MissionLink";
static const char* SERVICE_UUID = "3f4a0001-7c2e-4a5b-9d11-5b7d6e100001";
static const char* RX_UUID = "3f4a0002-7c2e-4a5b-9d11-5b7d6e100001";
static const char* TX_UUID = "3f4a0003-7c2e-4a5b-9d11-5b7d6e100001";
static constexpr size_t MAX_COMMAND_BYTES = 256;
static NimBLECharacteristic* txChar = nullptr;
static String commandBuffer;
static bool discardUntilNewline = false;
static ml::NodePreset activePreset = ml::NodePreset::Client;
static ml::MeshRouter meshRouter(activePreset);
static ml::MissionLinkRadio radioLink;
static Preferences settingsStore;
static bool settingsStoreReady = false;
static uint8_t localNodeId[8] = {0};

static void report(const String& jsonLine) {
  Serial.println(jsonLine);
  if (txChar) {
    txChar->setValue(jsonLine.c_str());
    // NimBLE-Arduino 2.x handles the no-subscriber case in notify().
    txChar->notify();
  }
}

static void reportError(const char* code, const char* message) {
  String out = "{\"type\":\"error\",\"code\":\"";
  out += code;
  out += "\",\"message\":\"";
  out += message;
  out += "\"}";
  report(out);
}

static const char* radioStateName(ml::RadioState state) {
  switch (state) {
    case ml::RadioState::Disabled: return "disabled";
    case ml::RadioState::PinmapUnverified: return "pinmap_unverified";
    case ml::RadioState::Ready: return "ready";
    case ml::RadioState::ReceiveError: return "receive_error";
    case ml::RadioState::DecodeError: return "decode_error";
    case ml::RadioState::TransmitBlocked: return "transmit_blocked";
    case ml::RadioState::TransmitError: return "transmit_error";
  }
  return "unknown";
}

static const char* meshActionName(ml::MeshAction action) {
  switch (action) {
    case ml::MeshAction::DropInvalid: return "drop_invalid";
    case ml::MeshAction::DropDuplicate: return "drop_duplicate";
    case ml::MeshAction::DeliverLocal: return "deliver_local";
    case ml::MeshAction::DeliverAndForward: return "deliver_and_forward";
    case ml::MeshAction::ForwardOnly: return "forward_only";
  }
  return "unknown";
}

static const char* packetTypeName(ml::PacketType type) {
  switch (type) {
    case ml::PacketType::Hello: return "hello";
    case ml::PacketType::Data: return "data";
    case ml::PacketType::Ack: return "ack";
    case ml::PacketType::Status: return "status";
    case ml::PacketType::Join: return "join";
    case ml::PacketType::Leave: return "leave";
  }
  return "unknown";
}

static const char* addressModeName(ml::AddressMode mode) {
  switch (mode) {
    case ml::AddressMode::Direct: return "direct";
    case ml::AddressMode::Group: return "group";
    case ml::AddressMode::Broadcast: return "broadcast";
  }
  return "unknown";
}

static void dispatchCommand(String cmd) {
  cmd.trim();
  if (cmd == "help") {
    report("{\"type\":\"help\",\"commands\":[\"help\",\"status\",\"nodes\",\"presets\",\"preset <client|repeater|gateway>\",\"send <node-id> <text>\",\"group <channel-id> <text>\",\"join <channel-id>\",\"leave <channel-id>\"]}");
  } else if (cmd == "presets") {
    report("{\"type\":\"presets\",\"items\":[{\"id\":\"client\",\"description\":\"End device; never relays packets.\"},{\"id\":\"repeater\",\"description\":\"Relays traffic without presenting group traffic locally.\"},{\"id\":\"gateway\",\"description\":\"Relays traffic and delivers group traffic locally.\"}]}");
  } else if (cmd.startsWith("preset ")) {
    String requested = cmd.substring(7);
    requested.trim();
    ml::NodePreset parsed;
    if (!ml::parsePreset(requested.c_str(), parsed)) {
      reportError("invalid_preset", "Use preset client, preset repeater, or preset gateway.");
      return;
    }
    // Persist this small preference in ESP32 NVS (internal flash), not the TF card.
    // If NVS is unavailable, allow a runtime-only change and report that explicitly.
    bool persisted = false;
    if (settingsStoreReady) {
      const char* requestedPreset = ml::presetName(parsed);
      const String savedPreset = settingsStore.getString("preset", "client");
      // Avoid unnecessary NVS writes (flash wear) when the selection is unchanged.
      if (!settingsStore.isKey("preset") || savedPreset != requestedPreset) {
        const size_t written = settingsStore.putString("preset", requestedPreset);
        if (written != strlen(requestedPreset)) {
          reportError("settings_write_failed", "Preset was not changed because internal-flash persistence failed.");
          return;
        }
      }
      persisted = true;
    }
    activePreset = parsed;
    meshRouter.setPreset(activePreset);
    String out = "{\"type\":\"preset\",\"active\":\"";
    out += ml::presetName(activePreset);
    out += "\",\"persisted\":";
    out += persisted ? "true" : "false";
    out += "}";
    report(out);
  } else if (cmd == "status") {
    const ml::RadioStatus radioStatus = radioLink.status();
    String out = "{\"type\":\"status\",\"firmware\":\"0.2.1\",\"protocolVersion\":";
    out += String(ML_PROTOCOL_VERSION);
    out += ",\"rfEnabled\":";
    out += ML_RF_ENABLED ? "true" : "false";
    out += ",\"bleReady\":true,\"serialReady\":true,\"radioReady\":";
    out += (radioStatus.initialized && radioStatus.state == ml::RadioState::Ready) ? "true" : "false";
    out += ",\"radioInitialized\":";
    out += radioStatus.initialized ? "true" : "false";
    out += ",\"radioReceiving\":";
    out += radioStatus.receiving ? "true" : "false";
    out += ",\"radioState\":\"";
    out += radioStateName(radioStatus.state);
    out += "\",\"settingsReady\":";
    out += settingsStoreReady ? "true" : "false";
    const bool presetPersisted = settingsStoreReady && settingsStore.isKey("preset") &&
        settingsStore.getString("preset", "client") == ml::presetName(activePreset);
    out += ",\"presetPersisted\":";
    out += presetPersisted ? "true" : "false";
    out += ",\"preset\":\"";
    out += ml::presetName(activePreset);
    out += "\"}";
    report(out);
  } else if (cmd == "nodes") {
    report("{\"type\":\"nodes\",\"items\":[],\"ready\":false,\"reason\":\"discovery_not_implemented\"}");
  } else if (cmd.startsWith("send ")) {
    reportError("not_ready", "Direct messaging is unavailable until radio and encryption are implemented.");
  } else if (cmd.startsWith("group ")) {
    reportError("not_ready", "Group messaging is unavailable until group keys and radio are implemented.");
  } else if (cmd.startsWith("join ")) {
    reportError("not_ready", "Channel membership and key provisioning are not implemented.");
  } else if (cmd.startsWith("leave ")) {
    reportError("not_ready", "Channel membership is not implemented.");
  } else if (cmd.length() == 0) {
    return;
  } else {
    reportError("unknown_command", "Unknown command. Send help for available commands.");
  }
}

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) override {
    std::string value = characteristic->getValue();
    if (value.empty()) return;
    if (value.size() > MAX_COMMAND_BYTES) {
      reportError("command_too_long", "Commands must be 256 bytes or fewer.");
      return;
    }
    dispatchCommand(String(value.c_str()));
  }
};

void setup() {
  Serial.begin(115200);
  delay(300);
  commandBuffer.reserve(MAX_COMMAND_BYTES);
  Serial.println();
  Serial.println("{\"type\":\"boot\",\"firmware\":\"0.2.1\",\"rfEnabled\":false}");
  Serial.println("{\"type\":\"hint\",\"message\":\"Send help for commands. RF is deliberately disabled.\"}");

  // Preferences uses internal ESP32 NVS flash and does not depend on TF/microSD.
  settingsStoreReady = settingsStore.begin("missionlink", false);
  if (settingsStoreReady) {
    const String savedPreset = settingsStore.getString("preset", "client");
    ml::NodePreset restored;
    if (ml::parsePreset(savedPreset.c_str(), restored)) {
      activePreset = restored;
      meshRouter.setPreset(activePreset);
    } else {
      Serial.println("{\"type\":\"warning\",\"code\":\"invalid_saved_preset\",\"message\":\"Using client preset; saved value is invalid.\"}");
    }
  } else {
    Serial.println("{\"type\":\"warning\",\"code\":\"settings_unavailable\",\"message\":\"Internal-flash settings unavailable; preset changes will be runtime-only.\"}");
  }

  uint8_t mac[6] = {0};
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
    localNodeId[0] = 'M'; localNodeId[1] = 'L';
    for (size_t i = 0; i < 6; ++i) localNodeId[i + 2] = mac[i];
  }
  // Safe no-op while the verified pin map and radio feature flags are disabled.
  radioLink.beginReceive();

  NimBLEDevice::init(BLE_NAME);
  auto* server = NimBLEDevice::createServer();
  auto* service = server->createService(SERVICE_UUID);
  auto* rx = service->createCharacteristic(
      RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  txChar = service->createCharacteristic(
      TX_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  rx->setCallbacks(new RxCallbacks());
  txChar->setValue("{\"type\":\"ready\",\"firmware\":\"0.2.1\",\"rfEnabled\":false}");
  NimBLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
  NimBLEDevice::getAdvertising()->start();
  Serial.println("{\"type\":\"ble\",\"advertising\":true}");
}

void loop() {
  while (Serial.available()) {
    char ch = static_cast<char>(Serial.read());
    if (ch == '\n' || ch == '\r') {
      if (discardUntilNewline) {
        discardUntilNewline = false;
        commandBuffer = "";
        reportError("command_too_long", "Commands must be 256 bytes or fewer.");
      } else if (commandBuffer.length()) {
        dispatchCommand(commandBuffer);
        commandBuffer = "";
      }
    } else if (!discardUntilNewline) {
      if (commandBuffer.length() < MAX_COMMAND_BYTES) {
        commandBuffer += ch;
      } else {
        // Drop the entire oversized line instead of dispatching a truncated command.
        commandBuffer = "";
        discardUntilNewline = true;
      }
    }
  }
  // Receive -> validate/deduplicate -> route. The adapter remains inert unless
  // its verified board config and feature flags are enabled at compile time.
  ml::Packet incoming;
  if (radioLink.poll(incoming)) {
    ml::Packet forwarded;
    const ml::MeshAction action = meshRouter.process(incoming, localNodeId, millis(), forwarded);
    if (action == ml::MeshAction::ForwardOnly ||
        action == ml::MeshAction::DeliverAndForward) {
      radioLink.transmit(forwarded);
    }
    const ml::RadioStatus rs = radioLink.status();
    String event = R"({"type":"mesh","action":)";
    event += String(static_cast<uint8_t>(action));
    event += R"(,"actionName":")";
    event += meshActionName(action);
    event += R"(","packetType":")";
    event += packetTypeName(incoming.type);
    event += R"(","addressMode":")";
    event += addressModeName(incoming.mode);
    event += R"(","source":")";
    event += ml::hexId(incoming.source, sizeof(incoming.source)).c_str();
    event += R"(","destination":")";
    event += ml::hexId(incoming.destination, sizeof(incoming.destination)).c_str();
    event += R"(","messageId":")";
    event += ml::hexId(incoming.messageId, sizeof(incoming.messageId)).c_str();
    event += R"(","hopCount":)";
    event += String(incoming.hopCount);
    event += R"(,"hopLimit":)";
    event += String(incoming.hopLimit);
    event += R"(,"payloadBytes":)";
    event += String(incoming.payload.size());
    event += R"(,"rssi":)";
    event += String(rs.lastRssi);
    event += R"(,"snr":)";
    event += String(rs.lastSnr);
    event += "}";
    report(event);
  }
  delay(5);
}
