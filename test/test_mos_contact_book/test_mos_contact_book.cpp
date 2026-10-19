#include <unity.h>
#include "mos_contact_book.h"
#include <cstring>

namespace {
const uint8_t kNodeA[mos::CONTACT_NODE_ID_BYTES] = {1, 2, 3, 4, 5, 6, 7, 8};
const uint8_t kNodeB[mos::CONTACT_NODE_ID_BYTES] = {8, 7, 6, 5, 4, 3, 2, 1};
const uint8_t kZeroId[mos::CONTACT_NODE_ID_BYTES] = {};
}

void test_contact_book_add_find_update_and_delete() {
  mos::ContactBook book;
  TEST_ASSERT_EQUAL_UINT32(0, book.count());
  TEST_ASSERT_EQUAL((int)mos::ContactResult::Ok, (int)book.upsert(kNodeA, "Relay One"));
  TEST_ASSERT_EQUAL_UINT32(1, book.count());
  const mos::Contact* found = book.find(kNodeA);
  TEST_ASSERT_NOT_NULL(found);
  TEST_ASSERT_EQUAL_STRING("Relay One", found->label);

  TEST_ASSERT_EQUAL((int)mos::ContactResult::Ok, (int)book.upsert(kNodeA, "Relay Alpha"));
  TEST_ASSERT_EQUAL_UINT32(1, book.count());
  TEST_ASSERT_EQUAL_STRING("Relay Alpha", book.find(kNodeA)->label);

  TEST_ASSERT_EQUAL((int)mos::ContactResult::Ok, (int)book.erase(kNodeA));
  TEST_ASSERT_EQUAL_UINT32(0, book.count());
  TEST_ASSERT_NULL(book.find(kNodeA));
  TEST_ASSERT_EQUAL((int)mos::ContactResult::NotFound, (int)book.erase(kNodeA));
}

void test_contact_book_rejects_invalid_ids_and_labels() {
  mos::ContactBook book;
  TEST_ASSERT_EQUAL((int)mos::ContactResult::InvalidId, (int)book.upsert(nullptr, "Valid"));
  TEST_ASSERT_EQUAL((int)mos::ContactResult::InvalidId, (int)book.upsert(kZeroId, "Valid"));
  TEST_ASSERT_EQUAL((int)mos::ContactResult::InvalidLabel, (int)book.upsert(kNodeA, nullptr));
  TEST_ASSERT_EQUAL((int)mos::ContactResult::InvalidLabel, (int)book.upsert(kNodeA, ""));
  TEST_ASSERT_EQUAL((int)mos::ContactResult::InvalidLabel, (int)book.upsert(kNodeA, "Bad\nLabel"));
  char tooLong[mos::CONTACT_LABEL_MAX_BYTES + 2];
  std::memset(tooLong, 'x', sizeof(tooLong) - 1);
  tooLong[sizeof(tooLong) - 1] = '\0';
  TEST_ASSERT_EQUAL((int)mos::ContactResult::InvalidLabel, (int)book.upsert(kNodeA, tooLong));
  TEST_ASSERT_EQUAL_UINT32(0, book.count());
}

void test_contact_book_capacity_is_bounded() {
  mos::ContactBook book;
  for (size_t i = 0; i < mos::CONTACT_BOOK_CAPACITY; ++i) {
    uint8_t id[mos::CONTACT_NODE_ID_BYTES] = {0};
    id[0] = static_cast<uint8_t>((i / 255) + 1);
    id[1] = static_cast<uint8_t>(i % 255 + 1);
    TEST_ASSERT_EQUAL((int)mos::ContactResult::Ok, (int)book.upsert(id, "Contact"));
  }
  TEST_ASSERT_EQUAL_UINT32(mos::CONTACT_BOOK_CAPACITY, book.count());
  uint8_t extra[mos::CONTACT_NODE_ID_BYTES] = {0xfe, 0xfe, 1};
  TEST_ASSERT_EQUAL((int)mos::ContactResult::Full, (int)book.upsert(extra, "Extra"));
  TEST_ASSERT_EQUAL_UINT32(mos::CONTACT_BOOK_CAPACITY, book.count());
}

void test_contact_labels_accept_printable_ascii_only() {
  TEST_ASSERT_TRUE(mos::isValidContactLabel("Node-01"));
  TEST_ASSERT_FALSE(mos::isValidContactLabel(nullptr));
  TEST_ASSERT_FALSE(mos::isValidContactLabel(""));
  TEST_ASSERT_FALSE(mos::isValidContactLabel("line\tbreak"));
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_contact_book_add_find_update_and_delete);
  RUN_TEST(test_contact_book_rejects_invalid_ids_and_labels);
  RUN_TEST(test_contact_book_capacity_is_bounded);
  RUN_TEST(test_contact_labels_accept_printable_ascii_only);
  return UNITY_END();
}
