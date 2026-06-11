/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "TopologyRegistry.hh"
#include <iostream>

namespace AstraSim {

TopologyRegistry& TopologyRegistry::instance() {
  static TopologyRegistry reg;
  return reg;
}

bool TopologyRegistry::registerTopology(const std::string& name,
                                        FactoryFn factory) {
  if (factories_.count(name)) {
    std::cerr << "Warning: TopologyRegistry overwriting '" << name << "'"
              << std::endl;
  }
  factories_[name] = std::move(factory);
  return true;
}

LogicalTopology* TopologyRegistry::create(const std::string& name,
                                          const TopologyParams& params) const {
  auto it = factories_.find(name);
  if (it == factories_.end()) return nullptr;
  return it->second(params);
}

bool TopologyRegistry::hasTopology(const std::string& name) const {
  return factories_.count(name) > 0;
}

}  // namespace AstraSim
