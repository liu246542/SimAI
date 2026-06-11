/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "AlgorithmRegistry.hh"
#include <iostream>

namespace AstraSim {

AlgorithmRegistry& AlgorithmRegistry::instance() {
  static AlgorithmRegistry reg;
  return reg;
}

bool AlgorithmRegistry::registerAlgorithm(const std::string& name,
                                          FactoryFn factory) {
  if (factories_.count(name)) {
    std::cerr << "Warning: AlgorithmRegistry overwriting '" << name << "'"
              << std::endl;
  }
  factories_[name] = std::move(factory);
  return true;
}

Algorithm* AlgorithmRegistry::create(const std::string& name,
                                     const AlgorithmParams& params) const {
  auto it = factories_.find(name);
  if (it == factories_.end()) return nullptr;
  return it->second(params);
}

bool AlgorithmRegistry::hasAlgorithm(const std::string& name) const {
  return factories_.count(name) > 0;
}

}  // namespace AstraSim
