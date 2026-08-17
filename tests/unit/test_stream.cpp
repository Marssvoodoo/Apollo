/**
 * @file tests/unit/test_stream.cpp
 * @brief Test src/stream.*
 */

#include <cstdint>
#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace stream {
  std::vector<uint8_t> concat_and_insert(uint64_t insert_size, uint64_t slice_size, const std::string_view &data1, const std::string_view &data2);
  bool has_suspension_capacity(std::size_t suspended_sessions, int max_suspended_sessions);
  bool reconnect_deadline_expired(
    std::chrono::steady_clock::time_point now,
    std::chrono::steady_clock::time_point suspended_at,
    int timeout_seconds
  );
}

#include "../tests_common.h"

TEST(ConcatAndInsertTests, ConcatNoInsertionTest) {
  char b1[] = {'a', 'b'};
  char b2[] = {'c', 'd', 'e'};
  auto res = stream::concat_and_insert(0, 2, std::string_view {b1, sizeof(b1)}, std::string_view {b2, sizeof(b2)});
  auto expected = std::vector<uint8_t> {'a', 'b', 'c', 'd', 'e'};
  ASSERT_EQ(res, expected);
}

TEST(ConcatAndInsertTests, ConcatLargeStrideTest) {
  char b1[] = {'a', 'b'};
  char b2[] = {'c', 'd', 'e'};
  auto res = stream::concat_and_insert(1, sizeof(b1) + sizeof(b2) + 1, std::string_view {b1, sizeof(b1)}, std::string_view {b2, sizeof(b2)});
  auto expected = std::vector<uint8_t> {0, 'a', 'b', 'c', 'd', 'e'};
  ASSERT_EQ(res, expected);
}

TEST(ConcatAndInsertTests, ConcatSmallStrideTest) {
  char b1[] = {'a', 'b'};
  char b2[] = {'c', 'd', 'e'};
  auto res = stream::concat_and_insert(1, 1, std::string_view {b1, sizeof(b1)}, std::string_view {b2, sizeof(b2)});
  auto expected = std::vector<uint8_t> {0, 'a', 0, 'b', 0, 'c', 0, 'd', 0, 'e'};
  ASSERT_EQ(res, expected);
}

TEST(SmartReconnectTests, EnforcesSuspendedSessionCapacity) {
  EXPECT_FALSE(stream::has_suspension_capacity(0, 0));
  EXPECT_TRUE(stream::has_suspension_capacity(0, 2));
  EXPECT_TRUE(stream::has_suspension_capacity(1, 2));
  EXPECT_FALSE(stream::has_suspension_capacity(2, 2));
}

TEST(SmartReconnectTests, UsesConfiguredReconnectDeadline) {
  const auto suspended_at = std::chrono::steady_clock::time_point {};
  EXPECT_FALSE(stream::reconnect_deadline_expired(suspended_at + std::chrono::seconds(10), suspended_at, 30));
  EXPECT_FALSE(stream::reconnect_deadline_expired(suspended_at + std::chrono::seconds(30), suspended_at, 30));
  EXPECT_TRUE(stream::reconnect_deadline_expired(suspended_at + std::chrono::seconds(31), suspended_at, 30));
}
