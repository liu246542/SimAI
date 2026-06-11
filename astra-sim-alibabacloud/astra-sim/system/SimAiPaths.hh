/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __SIMAI_PATHS_HH__
#define __SIMAI_PATHS_HH__

#include <cstdlib>
#include <string>

namespace AstraSim {

inline std::string getSimAiBasePath() {
  const char* env = std::getenv("SIMAI_BASE_DIR");
  if (env && env[0] != '\0') return std::string(env);
  return "/etc/astra-sim";
}

inline std::string getSimAiLogPath() {
  return getSimAiBasePath() + "/";
}

inline std::string getSimAiResultPath() {
  return getSimAiBasePath() + "/results/ncclFlowModel_";
}

}  // namespace AstraSim
#endif
