#include <unity.h>
#include "mos_contact_book.h"
#include "mos_contact_book_codec.h"
#include <cstring>

namespace {
const uint8_t kA[8] = {1,2,3,4,5,6,7,8};
const uint8_t kB[8] = {8,7,6,5,4,3,2,1};
uint8_t snapshot[mos::CONTACT_BOOK_ENCODED_MAX_BYTES];
}

void test_snapshot_round_trip() {
  mos::ContactBook original;
  TEST_ASSERT_EQUAL((int)mos::ContactResult::Ok, (int)original.upsert(kA, "Mission"));
  TEST_ASSERT_EQUAL((int)mos::ContactResult::Ok, (int)original.upsert(kB, "Vancouver"));
  size_t written = 0;
  TEST_ASSERT_TRUE(mos::encodeContactBook(original, snapshot, sizeof(snapshot), &written));
  TEST_ASSERT_GREATER_THAN_UINT32(10, written);
  mos::ContactBook restored;
  TEST_ASSERT_TRUE(mos::decodeContactBook(restored, snapshot, written));
  TEST_ASSERT_EQUAL_UINT32(2, restored.count());
  TEST_ASSERT_EQUAL_STRING("Mission", restored.find(kA)->label);
  TEST_ASSERT_EQUAL_STRING("Vancouver", restored.find(kB)->label);
}

void test_snapshot_rejects_corruption_and_truncation() {
  mos::ContactBook original;
  original.upsert(kA, "Relay");
  size_t written = 0;
  TEST_ASSERT_TRUE(mos::encodeContactBook(original, snapshot, sizeof(snapshot), &written));
  snapshot[written - 5] ^= 0x40;
  mos::ContactBook destination;
  destination.upsert(kB, "Preserve me");
  TEST_ASSERT_FALSE(mos::decodeContactBook(destination, snapshot, written));
  TEST_ASSERT_EQUAL_STRING("Preserve me", destination.find(kB)->label);
  TEST_ASSERT_FALSE(mos::decodeContactBook(destination, snapshot, written - 1));
}

void test_snapshot_rejects_bad_arguments_and_capacity() {
  mos::ContactBook book;
  size_t written = 77;
  TEST_ASSERT_FALSE(mos::encodeContactBook(book, nullptr, sizeof(snapshot), &written));
  TEST_ASSERT_EQUAL_UINT32(0, written);
  TEST_ASSERT_FALSE(mos::encodeContactBook(book, snapshot, 2, &written));
  TEST_ASSERT_FALSE(mos::decodeContactBook(book, nullptr, 0));
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_snapshot_round_trip);
  RUN_TEST(test_snapshot_rejects_corruption_and_truncation);
  RUN_TEST(test_snapshot_rejects_bad_arguments_and_capacity);
  return UNITY_END();
}
