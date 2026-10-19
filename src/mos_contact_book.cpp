#include "mos_contact_book.h"

#include <cstring>

namespace mos {
namespace {

bool isZeroId(const uint8_t* nodeId) {
  if (nodeId == nullptr) return true;
  for (size_t i = 0; i < CONTACT_NODE_ID_BYTES; ++i) {
    if (nodeId[i] != 0) return false;
  }
  return true;
}

bool sameId(const uint8_t* left, const uint8_t* right) {
  return left != nullptr && right != nullptr &&
         std::memcmp(left, right, CONTACT_NODE_ID_BYTES) == 0;
}

}  // namespace

ContactBook::ContactBook() : contacts_{}, count_(0) {}

size_t ContactBook::count() const {
  return count_;
}

const Contact* ContactBook::find(const uint8_t* nodeId) const {
  if (isZeroId(nodeId)) return nullptr;
  for (size_t i = 0; i < CONTACT_BOOK_CAPACITY; ++i) {
    if (contacts_[i].occupied && sameId(contacts_[i].nodeId, nodeId)) {
      return &contacts_[i];
    }
  }
  return nullptr;
}

bool isValidContactLabel(const char* label) {
  if (label == nullptr || label[0] == '\0') return false;
  size_t length = 0;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(label);
       *p != '\0'; ++p) {
    if (++length > CONTACT_LABEL_MAX_BYTES) return false;
    // Labels are plain printable ASCII for now; reject controls and DEL.
    if (*p < 0x20 || *p > 0x7e) return false;
  }
  return true;
}

ContactResult ContactBook::upsert(const uint8_t* nodeId, const char* label) {
  if (isZeroId(nodeId)) return ContactResult::InvalidId;
  if (!isValidContactLabel(label)) return ContactResult::InvalidLabel;

  size_t freeSlot = CONTACT_BOOK_CAPACITY;
  for (size_t i = 0; i < CONTACT_BOOK_CAPACITY; ++i) {
    if (contacts_[i].occupied && sameId(contacts_[i].nodeId, nodeId)) {
      std::memcpy(contacts_[i].label, label, std::strlen(label) + 1);
      return ContactResult::Ok;
    }
    if (!contacts_[i].occupied && freeSlot == CONTACT_BOOK_CAPACITY) {
      freeSlot = i;
    }
  }
  if (freeSlot == CONTACT_BOOK_CAPACITY) return ContactResult::Full;

  Contact& contact = contacts_[freeSlot];
  std::memcpy(contact.nodeId, nodeId, CONTACT_NODE_ID_BYTES);
  std::memcpy(contact.label, label, std::strlen(label) + 1);
  contact.occupied = true;
  ++count_;
  return ContactResult::Ok;
}

ContactResult ContactBook::erase(const uint8_t* nodeId) {
  if (isZeroId(nodeId)) return ContactResult::InvalidId;
  for (size_t i = 0; i < CONTACT_BOOK_CAPACITY; ++i) {
    if (contacts_[i].occupied && sameId(contacts_[i].nodeId, nodeId)) {
      contacts_[i] = Contact{};
      --count_;
      return ContactResult::Ok;
    }
  }
  return ContactResult::NotFound;
}

}  // namespace mos
