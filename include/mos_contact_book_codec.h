#pragma once

#include <stddef.h>
#include <stdint.h>
#include "mos_contact_book.h"

namespace mos {
// Version 1: "MCBK", version byte, count byte, then count records of
// 8-byte node ID + 1-byte label length + label bytes, followed by little-endian
// CRC-32/ISO-HDLC of all preceding bytes. Maximum snapshot is 2638 bytes.
static constexpr size_t CONTACT_BOOK_ENCODED_MAX_BYTES =
    4 + 1 + 1 + CONTACT_BOOK_CAPACITY *
                  (CONTACT_NODE_ID_BYTES + 1 + CONTACT_LABEL_MAX_BYTES) + 4;

bool encodeContactBook(const ContactBook& book, uint8_t* output,
                       size_t outputCapacity, size_t* written);
bool decodeContactBook(ContactBook& destination, const uint8_t* input,
                       size_t inputLength);
}  // namespace mos
