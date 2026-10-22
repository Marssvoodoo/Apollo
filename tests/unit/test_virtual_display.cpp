#ifdef _WIN32
#include "../tests_common.h"
#include <windows.h>
#include <vector>

namespace VDISPLAY {
  bool validate_topology_restore(std::vector<DISPLAYCONFIG_PATH_INFO> &, std::vector<DISPLAYCONFIG_MODE_INFO> &);
}

TEST(DisplayTopologyTest, WindowsAcceptsRestoreFlagsWithoutChangingDisplays) {
  UINT32 path_count = 0;
  UINT32 mode_count = 0;
  LONG status;
  std::vector<DISPLAYCONFIG_PATH_INFO> paths;
  std::vector<DISPLAYCONFIG_MODE_INFO> modes;
  do {
    status = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count);
    if (status == ERROR_ACCESS_DENIED || status == ERROR_NOT_SUPPORTED) {
      GTEST_SKIP() << "No interactive Windows display session: " << status;
    }
    ASSERT_EQ(status, ERROR_SUCCESS);
    paths.resize(path_count);
    modes.resize(mode_count);
    status = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_count, paths.data(), &mode_count, modes.data(), nullptr);
  } while (status == ERROR_INSUFFICIENT_BUFFER);
  ASSERT_EQ(status, ERROR_SUCCESS);
  paths.resize(path_count);
  modes.resize(mode_count);
  ASSERT_GT(path_count, 0);
  EXPECT_TRUE(VDISPLAY::validate_topology_restore(paths, modes));
}
#endif
