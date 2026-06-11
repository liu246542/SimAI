/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __ALGORITHM_REGISTRY_HH__
#define __ALGORITHM_REGISTRY_HH__

#include <functional>
#include <string>
#include <unordered_map>
#include "AlgorithmParams.hh"

namespace AstraSim {

class Algorithm;

class AlgorithmRegistry {
 public:
  using FactoryFn = std::function<Algorithm*(const AlgorithmParams&)>;

  static AlgorithmRegistry& instance();

  bool registerAlgorithm(const std::string& name, FactoryFn factory);
  Algorithm* create(const std::string& name,
                    const AlgorithmParams& params) const;
  bool hasAlgorithm(const std::string& name) const;

 private:
  AlgorithmRegistry() = default;
  std::unordered_map<std::string, FactoryFn> factories_;
};

}  // namespace AstraSim

#define _REGISTER_ALGO_CONCAT2(a, b) a##b
#define _REGISTER_ALGO_CONCAT(a, b) _REGISTER_ALGO_CONCAT2(a, b)

#define REGISTER_ALGORITHM(NAME, FACTORY_EXPR)                          \
  namespace {                                                           \
  static bool _REGISTER_ALGO_CONCAT(_reg_algo_, __COUNTER__) =          \
      AstraSim::AlgorithmRegistry::instance().registerAlgorithm(        \
          NAME, FACTORY_EXPR);                                          \
  }

#endif
