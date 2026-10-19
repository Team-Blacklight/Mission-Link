#include <unity.h>
#include "mos_storage.h"
#include <string>

void test_missing_card_is_absent_not_error() {
  TEST_ASSERT_EQUAL((int)mos::StorageState::Absent,
                    (int)mos::removableStorageState(false, false));
  TEST_ASSERT_EQUAL((int)mos::StorageState::Absent,
                    (int)mos::removableStorageState(false, true));
}

void test_card_mount_outcomes_are_distinct() {
  TEST_ASSERT_EQUAL((int)mos::StorageState::Ready,
                    (int)mos::removableStorageState(true, true));
  TEST_ASSERT_EQUAL((int)mos::StorageState::Error,
                    (int)mos::removableStorageState(true, false));
}

void test_safe_relative_paths() {
  TEST_ASSERT_TRUE(mos::isSafeRelativePath("contacts/contacts.json"));
  TEST_ASSERT_TRUE(mos::isSafeRelativePath("logs/events.log"));
  TEST_ASSERT_TRUE(mos::isSafeRelativePath("assets/..backup"));
  TEST_ASSERT_FALSE(mos::isSafeRelativePath(nullptr));
  TEST_ASSERT_FALSE(mos::isSafeRelativePath(""));
  TEST_ASSERT_FALSE(mos::isSafeRelativePath("/mos/contacts.json"));
  TEST_ASSERT_FALSE(mos::isSafeRelativePath("."));
  TEST_ASSERT_FALSE(mos::isSafeRelativePath("assets/."));
  TEST_ASSERT_FALSE(mos::isSafeRelativePath("../contacts.json"));
  TEST_ASSERT_FALSE(mos::isSafeRelativePath("contacts/../secrets.json"));
  TEST_ASSERT_FALSE(mos::isSafeRelativePath("contacts//contacts.json"));
  TEST_ASSERT_FALSE(mos::isSafeRelativePath("contacts/"));
  TEST_ASSERT_FALSE(mos::isSafeRelativePath("contacts\\secrets.json"));
  TEST_ASSERT_FALSE(mos::isSafeRelativePath("C:/secrets.json"));
  TEST_ASSERT_FALSE(mos::isSafeRelativePath("logs/line\nfeed"));
}

void test_path_length_is_bounded() {
  std::string valid(mos::MAX_RELATIVE_PATH_BYTES, 'a');
  TEST_ASSERT_TRUE(mos::isSafeRelativePath(valid.c_str()));
  valid.push_back('a');
  TEST_ASSERT_FALSE(mos::isSafeRelativePath(valid.c_str()));
}

void test_file_size_limit_is_inclusive_and_bounded() {
  TEST_ASSERT_TRUE(mos::isWithinSizeLimit(0, 0));
  TEST_ASSERT_TRUE(mos::isWithinSizeLimit(512, 512));
  TEST_ASSERT_FALSE(mos::isWithinSizeLimit(513, 512));
  TEST_ASSERT_FALSE(mos::isWithinSizeLimit(1, 0));
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_missing_card_is_absent_not_error);
  RUN_TEST(test_card_mount_outcomes_are_distinct);
  RUN_TEST(test_safe_relative_paths);
  RUN_TEST(test_path_length_is_bounded);
  RUN_TEST(test_file_size_limit_is_inclusive_and_bounded);
  return UNITY_END();
}
