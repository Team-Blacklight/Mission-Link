#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mos {

static constexpr size_t CONTACT_BOOK_CAPACITY = 64;
static constexpr size_t CONTACT_NODE_ID_BYTES = 8;
static constexpr size_t CONTACT_LABEL_MAX_BYTES = 32;

enum class ContactResult {
  Ok,
  NotFound,
  Full,
  InvalidId,
  InvalidLabel
};

struct Contact {
  uint8_t nodeId[CONTACT_NODE_ID_BYTES];
  char label[CONTACT_LABEL_MAX_BYTES + 1];
  bool occupied;
};

class ContactBook {
 public:
  ContactBook();

  size_t count() const;
  const Contact* find(const uint8_t* nodeId) const;
  ContactResult upsert(const uint8_t* nodeId, const char* label);
  ContactResult erase(const uint8_t* nodeId);

 private:
  Contact contacts_[CONTACT_BOOK_CAPACITY];
  size_t count_;
};

bool isValidContactLabel(const char* label);

}  // namespace mos
