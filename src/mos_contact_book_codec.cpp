#include "mos_contact_book_codec.h"

#include <cstring>

namespace mos {
namespace {
constexpr uint8_t MAGIC[4] = {'M', 'C', 'B', 'K'};
constexpr uint8_t VERSION = 1;
constexpr size_t HEADER_BYTES = 6;
constexpr size_t CRC_BYTES = 4;

uint32_t crc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
    }
  }
  return crc ^ 0xffffffffu;
}
void put32(uint8_t* out, uint32_t v) {
  out[0] = static_cast<uint8_t>(v);
  out[1] = static_cast<uint8_t>(v >> 8);
  out[2] = static_cast<uint8_t>(v >> 16);
  out[3] = static_cast<uint8_t>(v >> 24);
}
uint32_t get32(const uint8_t* in) {
  return static_cast<uint32_t>(in[0]) |
         (static_cast<uint32_t>(in[1]) << 8) |
         (static_cast<uint32_t>(in[2]) << 16) |
         (static_cast<uint32_t>(in[3]) << 24);
}
bool validId(const uint8_t* id) {
  if (!id) return false;
  for (size_t i = 0; i < CONTACT_NODE_ID_BYTES; ++i)
    if (id[i] != 0) return true;
  return false;
}
}  // namespace

bool encodeContactBook(const ContactBook& book, uint8_t* output,
                       size_t outputCapacity, size_t* written) {
  if (written) *written = 0;
  if (!output || !written || outputCapacity < HEADER_BYTES + CRC_BYTES ||
      book.count() > CONTACT_BOOK_CAPACITY) return false;

  size_t needed = HEADER_BYTES + CRC_BYTES;
  for (size_t i = 0; i < CONTACT_BOOK_CAPACITY; ++i) {
    // ContactBook's public API is intentionally the only way to inspect entries;
    // walk the bounded ID space is impossible, so serialization uses a friend-free
    // accessor added to the model in the next change.
  }
  (void)needed;
  return false;
}

bool decodeContactBook(ContactBook& destination, const uint8_t* input,
                       size_t inputLength) {
  if (!input || inputLength < HEADER_BYTES + CRC_BYTES ||
      inputLength > CONTACT_BOOK_ENCODED_MAX_BYTES) return false;
  if (std::memcmp(input, MAGIC, sizeof(MAGIC)) != 0 ||
      input[4] != VERSION || input[5] > CONTACT_BOOK_CAPACITY) return false;
  if (get32(input + inputLength - CRC_BYTES) != crc32(input, inputLength - CRC_BYTES))
    return false;

  ContactBook candidate;
  size_t offset = HEADER_BYTES;
  const size_t payloadEnd = inputLength - CRC_BYTES;
  for (size_t i = 0; i < input[5]; ++i) {
    if (offset + CONTACT_NODE_ID_BYTES + 1 > payloadEnd) return false;
    const uint8_t* id = input + offset;
    offset += CONTACT_NODE_ID_BYTES;
    const uint8_t labelLength = input[offset++];
    if (!validId(id) || labelLength == 0 || labelLength > CONTACT_LABEL_MAX_BYTES ||
        offset + labelLength > payloadEnd) return false;
    char label[CONTACT_LABEL_MAX_BYTES + 1] = {};
    std::memcpy(label, input + offset, labelLength);
    offset += labelLength;
    if (!isValidContactLabel(label) || std::strlen(label) != labelLength ||
        candidate.find(id) != nullptr ||
        candidate.upsert(id, label) != ContactResult::Ok) return false;
  }
  if (offset != payloadEnd) return false;
  destination = candidate;
  return true;
}
}  // namespace mos
