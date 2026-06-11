<p align="left">
    <a href="PluggableRegistry_CN.md">中文</a>&nbsp ｜ &nbspEnglish
</p>

# SimAI Pluggable Registry: Collective Algorithm & Network Topology Extension Guide

## 1. Background & Motivation

SimAI's collective communication algorithms (Ring, DoubleBinaryTree, HalvingDoubling, etc.) and network topologies (RingTopology, DoubleBinaryTreeTopology, etc.) were previously hardcoded in the dispatch logic:

- **Algorithm dispatch**: `Sys.cc` `generate_collective_phase()` used a ~220-line if/else chain on `CollectiveImplementationType` enum
- **Topology dispatch**: `GeneralComplexTopology.cc` constructor used a similar if/else chain

Adding a new algorithm or topology required modifying at least 3 core files (`Common.hh` enum, `Sys.cc` dispatch, `GeneralComplexTopology.cc` instantiation), which is error-prone and creates merge conflicts with upstream.

### What Changed

We introduced a lightweight **registry pattern** with two singleton registries:

- `AlgorithmRegistry` — maps config string names to algorithm factory functions
- `TopologyRegistry` — maps config string names to topology factory functions

All existing algorithms and topologies are now self-registered via macros in their own `.cc` files. The dispatch logic in `Sys.cc` and `GeneralComplexTopology.cc` has been replaced with registry lookups.

Additionally, the hardcoded `/etc/astra-sim` path is now configurable via the `SIMAI_BASE_DIR` environment variable.

## 2. Architecture Overview

```
Config file string (e.g., "ring")
        |
        v
generate_collective_implementation_from_input()   [Sys.cc]
        |
        |  creates CollectiveImplementation with config_name = "ring"
        v
generate_collective_phase()                        [Sys.cc]
        |
        |  builds AlgorithmParams struct
        |  calls AlgorithmRegistry::instance().create("ring", params)
        v
AlgorithmRegistry                                  [AlgorithmRegistry.cc]
        |
        |  looks up factory function registered by REGISTER_ALGORITHM("ring", ...)
        |  invokes lambda -> new Ring(...)
        v
Algorithm* instance returned
```

Topology follows the same pattern through `TopologyRegistry` in `GeneralComplexTopology.cc`.

### Key Files

| File | Role |
|---|---|
| `collective/AlgorithmRegistry.hh/.cc` | Algorithm registry singleton + `REGISTER_ALGORITHM` macro |
| `collective/AlgorithmParams.hh` | Unified parameter struct passed to all factories |
| `topology/TopologyRegistry.hh/.cc` | Topology registry singleton + `REGISTER_TOPOLOGY` macro |
| `system/SimAiPaths.hh` | Configurable base path via `SIMAI_BASE_DIR` |

## 3. How to Add a Custom Collective Algorithm

### Step 1: Create the Algorithm Class

Create your header and implementation files in `astra-sim/system/collective/`:

```cpp
// MyCustomAlgo.hh
#ifndef __MY_CUSTOM_ALGO_HH__
#define __MY_CUSTOM_ALGO_HH__

#include "astra-sim/system/collective/Algorithm.hh"
#include "astra-sim/system/topology/RingTopology.hh"

namespace AstraSim {
class MyCustomAlgo : public Algorithm {
 public:
  MyCustomAlgo(ComType type, int id, int layer_num,
               RingTopology* topology, uint64_t data_size, bool boost_mode);
  void run(EventType event, CallData* data) override;
};
}  // namespace AstraSim
#endif
```

Your algorithm must inherit from `Algorithm` and implement the pure virtual `run()` method.

### Step 2: Register with the Macro

At the top of your `.cc` file (outside any namespace), add the registration:

```cpp
// MyCustomAlgo.cc
#include "MyCustomAlgo.hh"
#include "AlgorithmRegistry.hh"

REGISTER_ALGORITHM("myCustomAlgo",
    [](const AstraSim::AlgorithmParams& p) -> AstraSim::Algorithm* {
  return new AstraSim::MyCustomAlgo(
      p.collective_type, p.id, p.layer_num,
      static_cast<AstraSim::RingTopology*>(p.topology),
      p.data_size, p.boost_mode);
})

namespace AstraSim {
// ... class implementation ...
}  // namespace AstraSim
```

The `AlgorithmParams` struct provides all commonly needed parameters:

| Field | Type | Description |
|---|---|---|
| `collective_type` | `ComType` | All_Reduce, All_Gather, Reduce_Scatter, All_to_All, etc. |
| `id` | `int` | Node/rank ID |
| `layer_num` | `int` | Layer number |
| `topology` | `BasicLogicalTopology*` | Topology for this dimension (cast to concrete type) |
| `data_size` | `uint64_t` | Data size for the operation |
| `boost_mode` | `bool` | Performance optimization flag |
| `direction` | `RingTopology::Direction` | Ring direction (Clockwise/Anticlockwise) |
| `injection_policy` | `InjectionPolicy` | Packet injection strategy |
| `direct_collective_window` | `int` | Sliding window for AllToAll-style algorithms |
| `flow_models` | `shared_ptr<void>` | NCCL flow models (for NcclFlowModel-based algorithms) |
| `num_channels` | `int` | Number of channels (for NcclFlowModel-based algorithms) |
| `sys` | `Sys*` | Access to the simulation system (use sparingly) |
| `extras` | `unordered_map<string, any>` | Extension point for custom parameters |

### Step 3: Use in Config

In the system input file, specify your algorithm name:

```
all-reduce-implementation: myCustomAlgo
```

Multi-dimensional configurations use underscores:

```
all-reduce-implementation: myCustomAlgo_doubleBinaryTree_ring
```

**No changes to `Sys.cc`, `Common.hh`, or any other core file are required.**

## 4. How to Add a Custom Network Topology

### Step 1: Create the Topology Class

Your topology must inherit from `LogicalTopology` (or `BasicLogicalTopology` / `ComplexLogicalTopology`) and implement:

```cpp
virtual int get_num_of_dimensions();
virtual int get_num_of_nodes_in_dimension(int dimension);
virtual BasicLogicalTopology* get_basic_topology_at_dimension(int dimension, ComType type);
```

### Step 2: Register with the Macro

```cpp
// MyCustomTopology.cc
#include "MyCustomTopology.hh"
#include "TopologyRegistry.hh"

REGISTER_TOPOLOGY("myCustomTopo",
    [](const AstraSim::TopologyParams& p) -> AstraSim::LogicalTopology* {
  return new AstraSim::MyCustomTopology(p.id, p.dimension_size,
                                        p.index_in_dimension, p.offset);
})

namespace AstraSim {
// ... class implementation ...
}  // namespace AstraSim
```

The `TopologyParams` struct:

| Field | Type | Description |
|---|---|---|
| `id` | `int` | Node ID |
| `dimension_size` | `int` | Number of nodes in this dimension |
| `index_in_dimension` | `int` | This node's index within the dimension |
| `offset` | `int` | Stride for node addressing |
| `is_last_dim` | `bool` | Whether this is the last dimension |
| `total_npus` | `int` | Total NPU count across all dimensions |

### Step 3: Register the Corresponding Algorithm

A topology usually pairs with an algorithm. Register both under the same config name:

```cpp
// In MyCustomAlgo.cc
REGISTER_ALGORITHM("myCustom", [...factory...])

// In MyCustomTopology.cc
REGISTER_TOPOLOGY("myCustom", [...factory...])
```

Then in the config file:

```
all-reduce-implementation: myCustom
```

The system will use `TopologyRegistry` to create the topology and `AlgorithmRegistry` to create the algorithm, both resolved by the same `"myCustom"` name.

## 5. SIMAI_BASE_DIR Environment Variable

The log and result paths previously hardcoded to `/etc/astra-sim` are now configurable:

```bash
# Use a local directory (no sudo needed)
export SIMAI_BASE_DIR=./simai_data
mkdir -p $SIMAI_BASE_DIR

# Or keep the default (/etc/astra-sim, requires write permission)
# unset SIMAI_BASE_DIR
```

This affects:
- NCCL log output path (`MockNcclLog`)
- Physical simulation result path (`SimAiMain`)
- Build script directory creation (`build.sh`)

The analytical mode's result output path (`./results/`) is unaffected — it uses the `-r` command-line argument.

## 6. Build Notes

### CMake Whole-Archive Linking

The registry relies on static initializers in each algorithm/topology `.cc` file. To prevent the linker from dead-stripping these objects when building with a static library, all frontend `CMakeLists.txt` files use `--whole-archive`:

```cmake
target_link_libraries(SimAI_analytical -Wl,--whole-archive AstraSim -Wl,--no-whole-archive)
```

If you add a new frontend target that links `libAstraSim.a`, apply the same pattern.

### C++ Standard

The project now requires **C++17** (upgraded from C++11) to support `std::any` in `AlgorithmParams::extras`.

## 7. Modified Files Reference

### New Files

| File | Description |
|---|---|
| `system/collective/AlgorithmParams.hh` | Unified parameter struct for algorithm factories |
| `system/collective/AlgorithmRegistry.hh` | Registry class + `REGISTER_ALGORITHM` macro |
| `system/collective/AlgorithmRegistry.cc` | Registry singleton implementation |
| `system/topology/TopologyRegistry.hh` | Registry class + `REGISTER_TOPOLOGY` macro |
| `system/topology/TopologyRegistry.cc` | Registry singleton implementation |
| `system/SimAiPaths.hh` | Configurable path helpers |

### Modified Files

| File | Change |
|---|---|
| `CMakeLists.txt` | C++ standard 11 -> 17 |
| `system/Common.hh` | `config_name` field + `UserRegistered` enum value |
| `system/Sys.cc` | Registry-based dispatch replacing if/else chain |
| `system/MockNcclLog.h` | Path via `SimAiPaths.hh` |
| `topology/GeneralComplexTopology.cc` | Registry-based topology creation |
| `collective/Ring.cc` | `REGISTER_ALGORITHM("ring", "oneRing")` |
| `collective/AllToAll.cc` | `REGISTER_ALGORITHM("direct", "oneDirect")` |
| `collective/DoubleBinaryTreeAllReduce.cc` | `REGISTER_ALGORITHM("doubleBinaryTree")` |
| `collective/HalvingDoubling.cc` | `REGISTER_ALGORITHM("halvingDoubling", "oneHalvingDoubling")` |
| `collective/NcclTreeFlowModel.cc` | `REGISTER_ALGORITHM("NcclFlowModel", "ncclRingTreeModel")` |
| `topology/RingTopology.cc` | `REGISTER_TOPOLOGY` for all ring-based variants |
| `topology/DoubleBinaryTreeTopology.cc` | `REGISTER_TOPOLOGY("doubleBinaryTree")` |
| `network_frontend/analytical/CMakeLists.txt` | `--whole-archive` linking |
| `network_frontend/phynet/CMakeLists.txt` | `--whole-archive` linking |
| `network_frontend/phynet/SimAiMain.cc` | Path via `SimAiPaths.hh` |
| `build.sh` | `SIMAI_BASE_DIR` env var |
| `build/astra_ns3/build.sh` | `SIMAI_BASE_DIR` env var |

All paths above are relative to `astra-sim-alibabacloud/astra-sim/` (or `astra-sim-alibabacloud/` for build scripts and CMakeLists).
