/**
 * @file file_handler.cpp
 * @brief Definitions for file handling functions.
 */

// standard includes
#include <atomic>
#include <cerrno>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
  // Windows.h pulls in CreateFileW / FlushFileBuffers / ReplaceFileW.
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <sddl.h>
#else
  #include <fcntl.h>
  #include <sys/stat.h>
  #include <unistd.h>
#endif

// local includes
#include "file_handler.h"
#include "logging.h"
#include "utility.h"

namespace file_handler {
  namespace {
    std::atomic_uint64_t private_temp_counter {0};
#ifdef _WIN32
    constexpr wchar_t PRIVATE_FILE_SDDL[] = L"D:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;OW)";
#endif

    int harden_private_file_permissions_impl(const char *path) {
#ifdef _WIN32
      // DACL is protected from inheritance and grants full control only to
      // SYSTEM, built-in Administrators, and the file owner. The owner ACE
      // keeps standalone/non-service builds usable without granting the local
      // Users group read access to credentials or private keys.
      PSECURITY_DESCRIPTOR security_descriptor = nullptr;
      if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            PRIVATE_FILE_SDDL,
            SDDL_REVISION_1,
            &security_descriptor,
            nullptr)) {
        BOOST_LOG(error) << "ConvertStringSecurityDescriptorToSecurityDescriptorW() failed for private file: " << GetLastError();
        return -1;
      }

      auto free_descriptor = util::fail_guard([security_descriptor]() {
        LocalFree(security_descriptor);
      });

      const auto wide_path = std::filesystem::path(path).wstring();
      if (!SetFileSecurityW(
            wide_path.c_str(),
            DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
            security_descriptor)) {
        BOOST_LOG(error) << "SetFileSecurityW() failed for private file: " << GetLastError();
        return -1;
      }
#else
      std::error_code ec;
      std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace,
        ec);
      if (ec) {
        BOOST_LOG(error) << "Couldn't restrict private file permissions: " << ec.message();
        return -1;
      }
#endif
      return 0;
    }

    int create_private_temp_file(const char *path, std::string &tmp_path) {
      for (int attempt = 0; attempt < 16; ++attempt) {
        const auto sequence = private_temp_counter.fetch_add(1, std::memory_order_relaxed);
#ifdef _WIN32
        tmp_path = std::string(path) + ".tmp." + std::to_string(GetCurrentProcessId()) + "." + std::to_string(sequence);
        const auto wide_tmp_path = std::filesystem::path(tmp_path).wstring();
        PSECURITY_DESCRIPTOR security_descriptor = nullptr;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
              PRIVATE_FILE_SDDL,
              SDDL_REVISION_1,
              &security_descriptor,
              nullptr)) {
          BOOST_LOG(error) << "ConvertStringSecurityDescriptorToSecurityDescriptorW() failed for private temporary file: " << GetLastError();
          return -1;
        }
        auto free_descriptor = util::fail_guard([security_descriptor]() {
          LocalFree(security_descriptor);
        });
        SECURITY_ATTRIBUTES security_attributes {
          sizeof(SECURITY_ATTRIBUTES),
          security_descriptor,
          FALSE,
        };
        HANDLE handle = CreateFileW(
          wide_tmp_path.c_str(),
          GENERIC_WRITE,
          0,
          &security_attributes,
          CREATE_NEW,
          FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
          nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
          const auto win_error = GetLastError();
          if (win_error == ERROR_FILE_EXISTS || win_error == ERROR_ALREADY_EXISTS) {
            continue;
          }
          BOOST_LOG(error) << "CreateFileW() failed for private temporary file: " << win_error;
          return -1;
        }

        CloseHandle(handle);
#else
        tmp_path = std::string(path) + ".tmp." + std::to_string(getpid()) + "." + std::to_string(sequence);
        const int descriptor = ::open(tmp_path.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        if (descriptor < 0) {
          if (errno == EEXIST) {
            continue;
          }
          BOOST_LOG(error) << "open() failed for private temporary file: " << errno;
          return -1;
        }
        ::close(descriptor);
#endif
        return 0;
      }

      BOOST_LOG(error) << "Unable to allocate a collision-free private temporary file";
      return -1;
    }

    int write_file_impl(const char *path, const std::string_view &contents, bool private_file) {
      if (private_file && std::filesystem::exists(path) && harden_private_file_permissions_impl(path)) {
        // Refuse to replace a currently-permissive secret file unless its ACL
        // can first be restricted. This closes the migration-time exposure.
        return -1;
      }

      std::string tmp_path;
      if (private_file) {
        if (create_private_temp_file(path, tmp_path)) {
          return -1;
        }
      } else {
        tmp_path = std::string(path) + ".tmp";
      }

      std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);

      if (!out.is_open()) {
        std::error_code ec;
        std::filesystem::remove(tmp_path, ec);
        return -1;
      }

      out.write(contents.data(), static_cast<std::streamsize>(contents.size()));

      if (out.fail()) {
        std::error_code ec;
        std::filesystem::remove(tmp_path, ec);
        return -1;
      }

      out.flush();
      out.close();

      if (out.fail()) {
        std::error_code ec;
        std::filesystem::remove(tmp_path, ec);
        return -1;
      }

      if (private_file && harden_private_file_permissions_impl(tmp_path.c_str())) {
        std::error_code ec;
        std::filesystem::remove(tmp_path, ec);
        return -1;
      }

#ifdef _WIN32
      std::wstring wtmp = std::filesystem::path(tmp_path).wstring();
      std::wstring wpath = std::filesystem::path(path).wstring();

      HANDLE h = CreateFileW(
        wtmp.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        NULL);
      if (h == INVALID_HANDLE_VALUE) {
        std::error_code ec;
        std::filesystem::remove(tmp_path, ec);
        return -1;
      }
      if (!FlushFileBuffers(h)) {
        CloseHandle(h);
        std::error_code ec;
        std::filesystem::remove(tmp_path, ec);
        return -1;
      }
      CloseHandle(h);

      if (std::filesystem::exists(path)) {
        if (!ReplaceFileW(
              wpath.c_str(),
              wtmp.c_str(),
              NULL,
              REPLACEFILE_WRITE_THROUGH | REPLACEFILE_IGNORE_MERGE_ERRORS,
              NULL,
              NULL)) {
          std::error_code ec;
          std::filesystem::remove(tmp_path, ec);
          return -1;
        }
      } else if (!MoveFileExW(
                   wtmp.c_str(),
                   wpath.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::error_code ec;
        std::filesystem::remove(tmp_path, ec);
        return -1;
      }
#else
      const int descriptor = ::open(tmp_path.c_str(), O_RDONLY);
      if (descriptor < 0 || ::fsync(descriptor) != 0) {
        if (descriptor >= 0) {
          ::close(descriptor);
        }
        std::error_code ec;
        std::filesystem::remove(tmp_path, ec);
        return -1;
      }
      ::close(descriptor);

      std::error_code ec;
      std::filesystem::rename(tmp_path, path, ec);
      if (ec) {
        std::filesystem::remove(tmp_path, ec);
        return -1;
      }
#endif

      return private_file ? harden_private_file_permissions_impl(path) : 0;
    }
  }  // namespace

  std::string get_parent_directory(const std::string &path) {
    // remove any trailing path separators
    std::string trimmed_path = path;
    while (!trimmed_path.empty() && trimmed_path.back() == '/') {
      trimmed_path.pop_back();
    }

    std::filesystem::path p(trimmed_path);
    return p.parent_path().string();
  }

  bool make_directory(const std::string &path) {
    // first, check if the directory already exists
    if (std::filesystem::exists(path)) {
      return true;
    }

    return std::filesystem::create_directories(path);
  }

  std::string read_file(const char *path) {
    if (!std::filesystem::exists(path)) {
      BOOST_LOG(debug) << "Missing file: " << path;
      return {};
    }

    std::ifstream in(path, std::ios::binary);
    return std::string {(std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()};
  }

  int write_file(const char *path, const std::string_view &contents) {
    return write_file_impl(path, contents, false);
  }

  int write_private_file(const char *path, const std::string_view &contents) {
    return write_file_impl(path, contents, true);
  }

  int harden_private_file_permissions(const char *path) {
    return harden_private_file_permissions_impl(path);
  }
}  // namespace file_handler
