/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "GeneralComplexTopology.hh"
#include "DoubleBinaryTreeTopology.hh"
#include "RingTopology.hh"
#include "TopologyRegistry.hh"

namespace AstraSim {
BasicLogicalTopology* GeneralComplexTopology::get_basic_topology_at_dimension(
    int dimension,
    ComType type) {
  return dimension_topology[dimension]->get_basic_topology_at_dimension(
      0, type);
}
int GeneralComplexTopology::get_num_of_nodes_in_dimension(int dimension) {
  if (dimension >= dimension_topology.size()) {
    std::cout << "dim: " << dimension
              << " requested! but max dim is: " << dimension_topology.size() - 1
              << std::endl;
  }
  assert(dimension < dimension_topology.size());
  return dimension_topology[dimension]->get_num_of_nodes_in_dimension(0);
}
int GeneralComplexTopology::get_num_of_dimensions() {
  return dimension_topology.size();
}
GeneralComplexTopology::~GeneralComplexTopology() {
  for (int i = 0; i < dimension_topology.size(); i++) {
    delete dimension_topology[i];
  }
}
GeneralComplexTopology::GeneralComplexTopology(
    int id,
    std::vector<int> dimension_size,
    std::vector<CollectiveImplementation*> collective_implementation) {
  int offset = 1;
  int last_dim = collective_implementation.size() - 1;
  int total_npus = 1;
  for (int d : dimension_size) {
    total_npus *= d;
  }

  for (int dim = 0; dim < collective_implementation.size(); dim++) {
    TopologyParams tp;
    tp.id = id;
    tp.dimension_size = dimension_size[dim];
    tp.index_in_dimension = (id % (offset * dimension_size[dim])) / offset;
    tp.offset = offset;
    tp.is_last_dim = (dim == last_dim);
    tp.total_npus = total_npus;

    std::string name = collective_implementation[dim]->config_name;
    LogicalTopology* topo = TopologyRegistry::instance().create(name, tp);

    if (!topo) {
      std::cerr << "Error: No known topology for '" << name << "'" << std::endl;
      exit(1);
    }

    dimension_topology.push_back(topo);

    if (name == "oneRing" || name == "oneDirect" || name == "oneHalvingDoubling") return;

    offset *= dimension_size[dim];
  }
}
} // namespace AstraSim
