/**
 * @file tests/unit/test_httpcommon.cpp
 * @brief Test src/httpcommon.*.
 */
// test imports
#include "../tests_common.h"

// lib imports
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <filesystem>

// local imports
#include <src/file_handler.h>
#include <src/httpcommon.h>

struct UrlEscapeTest: testing::TestWithParam<std::tuple<std::string, std::string>> {};

TEST_P(UrlEscapeTest, Run) {
  const auto &[input, expected] = GetParam();
  ASSERT_EQ(http::url_escape(input), expected);
}

INSTANTIATE_TEST_SUITE_P(
  UrlEscapeTests,
  UrlEscapeTest,
  testing::Values(
    std::make_tuple("igdb_0123456789", "igdb_0123456789"),
    std::make_tuple("../../../", "..%2F..%2F..%2F"),
    std::make_tuple("..*\\", "..%2A%5C")
  )
);

TEST(UserCredentialsTest, SaveMigratesToV3AndPreservesPairingState) {
  const auto file = (std::filesystem::temp_directory_path() / "apollo-test-state.json").string();
  const nlohmann::json original {
    {"root", {{"uniqueid", "paired-device-state"}}},
    {"username", "legacy"},
    {"salt", "legacy-salt"},
    {"password", http::hash_password("old-password", "legacy-salt", 1)},
  };
  ASSERT_EQ(file_handler::write_private_file(file.c_str(), original.dump(2)), 0);

  ASSERT_EQ(http::save_user_creds(file, "admin", "new-password"), 0);
  const auto migrated = nlohmann::json::parse(file_handler::read_file(file.c_str()));
  EXPECT_EQ(migrated.at("hash_version").get<int>(), http::CURRENT_HASH_VERSION);
  EXPECT_EQ(migrated.at("username").get<std::string>(), "admin");
  EXPECT_EQ(migrated.at("root").at("uniqueid").get<std::string>(), "paired-device-state");
  EXPECT_EQ(
    migrated.at("password").get<std::string>(),
    http::hash_password("new-password", migrated.at("salt").get<std::string>(), http::CURRENT_HASH_VERSION));

  std::filesystem::remove(file);
}

struct UrlGetHostTest: testing::TestWithParam<std::tuple<std::string, std::string>> {};

TEST_P(UrlGetHostTest, Run) {
  const auto &[input, expected] = GetParam();
  ASSERT_EQ(http::url_get_host(input), expected);
}

INSTANTIATE_TEST_SUITE_P(
  UrlGetHostTests,
  UrlGetHostTest,
  testing::Values(
    std::make_tuple("https://images.igdb.com/example.txt", "images.igdb.com"),
    std::make_tuple("http://localhost:8080", "localhost"),
    std::make_tuple("nonsense!!}{::", "")
  )
);

struct DownloadFileTest: testing::TestWithParam<std::tuple<std::string, std::string>> {};

TEST_P(DownloadFileTest, Run) {
  const auto &[url, filename] = GetParam();
  const std::string test_dir = platf::appdata().string() + "/tests/";
  std::string path = test_dir + filename;
  ASSERT_TRUE(http::download_file(url, path, CURL_SSLVERSION_TLSv1_0));
}

#ifdef SUNSHINE_BUILD_FLATPAK
// requires running `npm run serve` prior to running the tests
constexpr const char *URL_1 = "http://0.0.0.0:3000/hello.txt";
constexpr const char *URL_2 = "http://0.0.0.0:3000/hello-redirect.txt";
#else
constexpr const char *URL_1 = "https://httpbin.org/base64/aGVsbG8h";
constexpr const char *URL_2 = "https://httpbin.org/redirect-to?url=/base64/aGVsbG8h";
#endif

INSTANTIATE_TEST_SUITE_P(
  DownloadFileTests,
  DownloadFileTest,
  testing::Values(
    std::make_tuple(URL_1, "hello.txt"),
    std::make_tuple(URL_2, "hello-redirect.txt")
  )
);
