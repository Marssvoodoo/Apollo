/**
 * @file tests/unit/test_confighttp.cpp
 * @brief Web authentication regressions exercised through loopback HTTPS.
 */
#include "../tests_common.h"
#include "../tests_log_checker.h"

#include <filesystem>
#include <future>
#include <thread>
#include <nlohmann/json.hpp>
#include <Simple-Web-Server/client_https.hpp>
#include <Simple-Web-Server/server_https.hpp>
#include <src/config.h>
#include <src/file_handler.h>
#include <src/httpcommon.h>
#include <src/network.h>
#include <src/uuid.h>

namespace confighttp {
  using response_t = std::shared_ptr<SimpleWeb::ServerBase<SimpleWeb::HTTPS>::Response>;
  using request_t = std::shared_ptr<SimpleWeb::ServerBase<SimpleWeb::HTTPS>::Request>;
  void login(response_t, request_t);
  void savePassword(response_t, request_t);
  void getCsrfToken(response_t, request_t);
  void print_req(const request_t &);
  std::string getCookieValue(const std::string &, const std::string &);
}

TEST(WebCookieTest, MatchesOnlyWholeCookieNames) {
  EXPECT_EQ(confighttp::getCookieValue("oauth=wrong; auth=correct", "auth"), "correct");
  EXPECT_EQ(confighttp::getCookieValue("xauth=wrong", "auth"), "");
  EXPECT_EQ(confighttp::getCookieValue("other=auth=wrong; auth=correct", "auth"), "correct");
  EXPECT_EQ(confighttp::getCookieValue("other=value;\t auth=correct; theme=dark", "auth"), "correct");
  EXPECT_EQ(confighttp::getCookieValue("auth=abc==; theme=dark", "auth"), "abc==");
  EXPECT_EQ(confighttp::getCookieValue("auth=; theme=dark", "auth"), "");
}

class WebAuthTest: public testing::Test {
protected:
  using server_t = SimpleWeb::Server<SimpleWeb::HTTPS>;
  using client_t = SimpleWeb::Client<SimpleWeb::HTTPS>;
  config::sunshine_t original_config;
  net::net_e original_origin;
  std::filesystem::path directory;
  std::unique_ptr<server_t> server;
  std::unique_ptr<client_t> client;
  std::thread worker;

  void SetUp() override {
    original_config = config::sunshine;
    original_origin = http::origin_web_ui_allowed;
    directory = std::filesystem::temp_directory_path() / ("apollo-web-test-" + uuid_util::uuid_t::generate().string());
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    config::sunshine.username.clear();
    config::sunshine.password.clear();
    config::sunshine.salt.clear();
    config::sunshine.credentials_file = (directory / "state.json").string();
    http::origin_web_ui_allowed = net::PC;
    const auto key = (directory / "key.pem").string();
    const auto cert = (directory / "cert.pem").string();
    ASSERT_EQ(http::create_creds(key, cert), 0);
    server = std::make_unique<server_t>(cert, key);
    server->config.address = "127.0.0.1";
    server->config.port = 0;
    server->resource["^/api/login$"]["POST"] = confighttp::login;
    server->resource["^/api/password$"]["POST"] = confighttp::savePassword;
    server->resource["^/api/csrf-token$"]["GET"] = confighttp::getCsrfToken;
    server->resource["^/log$"]["GET"] = [](auto response, auto request) {
      confighttp::print_req(request);
      response->write("logged");
    };
    auto ready = std::make_shared<std::promise<unsigned short>>();
    auto started = ready->get_future();
    worker = std::thread([this, ready] {
      try {
        server->start([ready](auto port) { ready->set_value(port); });
      } catch (...) {
        ready->set_exception(std::current_exception());
      }
    });
    ASSERT_EQ(started.wait_for(5s), std::future_status::ready);
    client = std::make_unique<client_t>("127.0.0.1:" + std::to_string(started.get()), false);
    client->config.timeout = 5;
  }

  void TearDown() override {
    if (server) server->stop();
    if (worker.joinable()) worker.join();
    config::sunshine = original_config;
    http::origin_web_ui_allowed = original_origin;
    std::filesystem::remove_all(directory);
  }

  auto password_request(const nlohmann::json &body, SimpleWeb::CaseInsensitiveMultimap headers = {}) {
    headers.emplace("Content-Type", "application/json");
    return client->request("POST", "/api/password", body.dump(), headers);
  }

  void create_account() {
    auto response = password_request({{"newUsername", "test-admin"}, {"newPassword", "old-password"}, {"confirmNewPassword", "old-password"}});
    ASSERT_EQ(response->status_code, "200 OK");
    ASSERT_TRUE(nlohmann::json::parse(response->content.string()).at("status"));
  }

  SimpleWeb::CaseInsensitiveMultimap login(const std::string &password) {
    auto response = client->request("POST", "/api/login", nlohmann::json({{"username", "test-admin"}, {"password", password}}).dump(), {{"Content-Type", "application/json"}});
    EXPECT_EQ(response->status_code, "200 OK");
    auto cookie = response->header.find("Set-Cookie");
    if (cookie == response->header.end()) return {};
    SimpleWeb::CaseInsensitiveMultimap headers {{"Cookie", cookie->second.substr(0, cookie->second.find(';'))}};
    auto token = client->request("GET", "/api/csrf-token", "", headers);
    EXPECT_EQ(token->status_code, "200 OK");
    headers.emplace("X-CSRF-Token", nlohmann::json::parse(token->content.string()).at("csrf_token").get<std::string>());
    return headers;
  }
};

TEST_F(WebAuthTest, InitialSetupReportsWriteFailure) {
  config::sunshine.credentials_file = (directory / "missing-parent" / "state.json").string();
  auto response = password_request({{"newUsername", "test-admin"}, {"newPassword", "new-password"}, {"confirmNewPassword", "new-password"}});
  EXPECT_EQ(response->status_code, "500 Internal Server Error");
  EXPECT_FALSE(nlohmann::json::parse(response->content.string()).at("status"));
  EXPECT_TRUE(config::sunshine.username.empty());
}

TEST_F(WebAuthTest, FailedPasswordChangePreservesLoginAndPairings) {
  create_account();
  auto headers = login("old-password");
  ASSERT_FALSE(headers.empty());
  const auto state = config::sunshine.credentials_file;
  auto contents = nlohmann::json::parse(file_handler::read_file(state.c_str()));
  contents["root"] = {{"uniqueid", "paired-device"}};
  ASSERT_EQ(file_handler::write_private_file(state.c_str(), contents.dump()), 0);
  const auto original_bytes = file_handler::read_file(state.c_str());
  config::sunshine.credentials_file = (directory / "missing-parent" / "state.json").string();
  auto response = password_request({{"currentUsername", "test-admin"}, {"currentPassword", "old-password"}, {"newPassword", "new-password"}, {"confirmNewPassword", "new-password"}}, headers);
  EXPECT_EQ(response->status_code, "500 Internal Server Error");
  EXPECT_FALSE(nlohmann::json::parse(response->content.string()).at("status"));
  config::sunshine.credentials_file = state;
  EXPECT_EQ(file_handler::read_file(state.c_str()), original_bytes);
  EXPECT_EQ(client->request("GET", "/api/csrf-token", "", headers)->status_code, "200 OK");
  EXPECT_FALSE(login("old-password").empty());
}

TEST_F(WebAuthTest, SuccessfulPasswordChangeInvalidatesSessionAndPreservesPairings) {
  create_account();
  auto headers = login("old-password");
  ASSERT_FALSE(headers.empty());
  auto state = nlohmann::json::parse(file_handler::read_file(config::sunshine.credentials_file.c_str()));
  state["root"] = {{"uniqueid", "paired-device"}};
  ASSERT_EQ(file_handler::write_private_file(config::sunshine.credentials_file.c_str(), state.dump()), 0);
  auto response = password_request({{"currentUsername", "test-admin"}, {"currentPassword", "old-password"}, {"newPassword", "new-password"}, {"confirmNewPassword", "new-password"}}, headers);
  EXPECT_EQ(response->status_code, "200 OK");
  EXPECT_TRUE(nlohmann::json::parse(response->content.string()).at("status"));
  EXPECT_EQ(client->request("GET", "/api/csrf-token", "", headers)->status_code, "401 Unauthorized");
  EXPECT_FALSE(login("new-password").empty());
  const auto saved = nlohmann::json::parse(file_handler::read_file(config::sunshine.credentials_file.c_str()));
  EXPECT_EQ(saved.at("root").at("uniqueid"), "paired-device");
}

TEST_F(WebAuthTest, RequestLogsRedactSessionHeadersInAnyCase) {
  auto response = client->request("GET", "/log", "", {
    {"aUtHoRiZaTiOn", "Bearer apollo-auth-secret-20261021"},
    {"cOoKiE", "auth=apollo-cookie-secret-20261021"},
    {"X-cSrF-ToKeN", "apollo-csrf-secret-20261021"},
    {"sEt-CoOkIe", "apollo-set-cookie-secret-20261021"},
    {"X-Test-Trace", "apollo-visible-trace-20261021"},
  });
  ASSERT_EQ(response->status_code, "200 OK");
  EXPECT_TRUE(log_checker::line_contains("test_sunshine.log", "apollo-visible-trace-20261021"));
  EXPECT_FALSE(log_checker::line_contains("test_sunshine.log", "apollo-auth-secret-20261021"));
  EXPECT_FALSE(log_checker::line_contains("test_sunshine.log", "apollo-cookie-secret-20261021"));
  EXPECT_FALSE(log_checker::line_contains("test_sunshine.log", "apollo-csrf-secret-20261021"));
  EXPECT_FALSE(log_checker::line_contains("test_sunshine.log", "apollo-set-cookie-secret-20261021"));
}
