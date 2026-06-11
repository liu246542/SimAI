/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __ALGORITHM_PARAMS_HH__
#define __ALGORITHM_PARAMS_HH__

#include <any>
#include <memory>
#include <string>
#include <unordered_map>
#include "astra-sim/system/Common.hh"
#include "astra-sim/system/topology/RingTopology.hh"

namespace AstraSim {

class Sys;
class BasicLogicalTopology;

struct AlgorithmParams {
  ComType collective_type;
  int id;
  int layer_num;
  BasicLogicalTopology* topology;
  uint64_t data_size;
  bool boost_mode;

  RingTopology::Direction direction;
  InjectionPolicy injection_policy;

  int direct_collective_window = -1;

  std::shared_ptr<void> flow_models;
  int num_channels = 0;

  Sys* sys = nullptr;

  std::unordered_map<std::string, std::any> extras;
};

}  // namespace AstraSim
#endif
