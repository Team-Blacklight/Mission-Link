#include <unity.h>
#include "mos_contact_book.h"
#include "mos_contact_book_codec.h"
#include <cstring>

namespace {
const uint8_t kA[8] = {1,2,3,4,5,6,7,8};
const uint8_t kB[8] = {8,7,6,5,4,3,2,1};
uint8_t snapshot[mos::CONTACT_BOOK_ENCODED_MAX_BYTES];
uint8_t secondSnapshot[mos::CONTACT_BOOK_ENCODED_MAX_BYTES];
}

void test_snapshot_round_trip_and_determinism() {
  mos::ContactBook original;
  TEST_ASSERT_EQUAL((int)mos::ContactResult::Ok, (int)original.upsert(kA, "Mission"));
  TEST_ASSERT_EQUAL((int)mos::ContactResult::Ok, (int)original.upsert(kB, "Vancouver"));

  size_t written = 0;
  size_t secondWritten = 0;
  TEST_ASSERT_TRUE(mos::encodeContactBook(original, snapshot, sizeof(snapshot), &written));
  TEST_ASSERT_TRUE(written > 10);
  TEST_ASSERT_TRUE(mos::encodeContactBook(original, secondSnapshot,
                                          sizeof(secondSnapshot), &secondWritten));
  TEST_ASSERT_EQUAL_UINT32(written, secondWritten);
  TEST_ASSERT_EQUAL_MEMORY(snapshot, secondSnapshot, written);

  mos::ContactBook restored;
  TEST_ASSERT_TRUE(mos::decodeContactBook(restored, snapshot, written));
  TEST_ASSERT_EQUAL_UINT32(2, restored.count());
  TEST_ASSERT_EQUAL_STRING("Mission", restored.find(kA)->label);
  TEST_ASSERT_EQUAL_STRING("Vancouver", restored.find(kB)->label);
}

void test_empty_snapshot_round_trip() {
  mos::ContactBook empty;
  size_t written = 0;
  TEST_ASSERT_TRUE(mos::encodeContactBook(empty, snapshot, sizeof(snapshot), &written));
  TEST_ASSERT_EQUAL_UINT32(10, written);

  mos::ContactBook restored;
  restored.upsert(kA, "Old entry");
  TEST_ASSERT_TRUE(mos::decodeContactBook(restored, snapshot, written));
  TEST_ASSERT_EQUAL_UINT32(0, restored.count());
  TEST_ASSERT_NULL(restored.find(kA));
}

void test_snapshot_rejects_corruption_and_truncation_without_mutation() {
  mos::ContactBook original;
  original.upsert(kA, "Relay");
  size_t written = 0;
  TEST_ASSERT_TRUE(mos::encodeContactBook(original, snapshot, sizeof(snapshot), &written));

  mos::ContactBook destination;
  destination.upsert(kB, "Preserve me");

  snapshot[0] ^= 0x01;  // Invalid magic and stale checksum.
  TEST_ASSERT_FALSE(mos::decodeContactBook(destination, snapshot, written));
  TEST_ASSERT_EQUAL_STRING("Preserve me", destination.find(kB)->label);

  TEST_ASSERT_TRUE(mos::encodeContactBook(original, snapshot, sizeof(snapshot), &written));
  TEST_ASSERT_FALSE(mos::decodeContactBook(destination, snapshot, written - 1));
  TEST_ASSERT_EQUAL_STRING("Preserve me", destination.find(kB)->label);

  snapshot[written - 1] ^= 0x80;  // Corrupt checksum.
  TEST_ASSERT_FALSE(mos::decodeContactBook(destination, snapshot, written));
  TEST_ASSERT_EQUAL_STRING("Preserve me", destination.find(kB)->label);
}

void test_snapshot_rejects_bad_arguments_and_capacity() {
  mos::ContactBook book;
  size_t written = 77;
  TEST_ASSERT_FALSE(mos::encodeContactBook(book, nullptr, sizeof(snapshot), &written));
  TEST_ASSERT_EQUAL_UINT32(0, written);
  TEST_ASSERT_FALSE(mos::encodeContactBook(book, snapshot, 2, &written));
  TEST_ASSERT_EQUAL_UINT32(0, written);
  TEST_ASSERT_FALSE(mos::decodeContactBook(book, nullptr, 0));
  TEST_ASSERT_FALSE(mos::decodeContactBook(book, snapshot, 0));
}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_snapshot_round_trip_and_determinism);
  RUN_TEST(test_empty_snapshot_round_trip);
  RUN_TEST(test_snapshot_rejects_corruption_and_truncation_without_mutation);
  RUN_TEST(test_snapshot_rejects_bad_arguments_and_capacity);
  return UNITY_END();
}
