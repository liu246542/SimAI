/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __TOPOLOGY_REGISTRY_HH__
#define __TOPOLOGY_REGISTRY_HH__

#include <functional>
#include <string>
#include <unordered_map>

namespace AstraSim {

class LogicalTopology;

struct TopologyParams {
  int id;
  int dimension_size;
  int index_in_dimension;
  int offset;
  bool is_last_dim;
  int total_npus;
};

class TopologyRegistry {
 public:
  using FactoryFn = std::function<LogicalTopology*(const TopologyParams&)>;

  static TopologyRegistry& instance();

  bool registerTopology(const std::string& name, FactoryFn factory);
  LogicalTopology* create(const std::string& name,
                          const TopologyParams& params) const;
  bool hasTopology(const std::string& name) const;

 private:
  TopologyRegistry() = default;
  std::unordered_map<std::string, FactoryFn> factories_;
};

}  // namespace AstraSim

#define _REGISTER_TOPO_CONCAT2(a, b) a##b
#define _REGISTER_TOPO_CONCAT(a, b) _REGISTER_TOPO_CONCAT2(a, b)

#define REGISTER_TOPOLOGY(NAME, FACTORY_EXPR)                           \
  namespace {                                                           \
  static bool _REGISTER_TOPO_CONCAT(_reg_topo_, __COUNTER__) =          \
      AstraSim::TopologyRegistry::instance().registerTopology(          \
          NAME, FACTORY_EXPR);                                          \
  }

#endif
