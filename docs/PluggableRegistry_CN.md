<p align="left">
    中文&nbsp ｜ &nbsp<a href="PluggableRegistry.md">English</a>
</p>

# SimAI 可插拔注册机制：集合通信算法与网络拓扑扩展指南

## 1. 背景与目标

SimAI 的集合通信算法（Ring、DoubleBinaryTree、HalvingDoubling 等）和网络拓扑（RingTopology、DoubleBinaryTreeTopology 等）此前通过硬编码的 if/else 链进行分发：

- **算法分发**：`Sys.cc` 中 `generate_collective_phase()` 使用约 220 行的 if/else 链，基于 `CollectiveImplementationType` 枚举匹配
- **拓扑分发**：`GeneralComplexTopology.cc` 构造函数中使用类似的 if/else 链

新增一个算法或拓扑至少需要修改 3 个核心文件（`Common.hh` 枚举、`Sys.cc` 分发逻辑、`GeneralComplexTopology.cc` 实例化），容易出错且与上游合并时易产生冲突。

### 改动内容

引入了轻量级**注册表模式**，包含两个单例注册表：

- `AlgorithmRegistry` — 将配置字符串映射到算法工厂函数
- `TopologyRegistry` — 将配置字符串映射到拓扑工厂函数

所有现有算法和拓扑通过各自 `.cc` 文件中的宏完成自注册。`Sys.cc` 和 `GeneralComplexTopology.cc` 中的分发逻辑已替换为注册表查找。

此外，原先硬编码的 `/etc/astra-sim` 路径现在可通过 `SIMAI_BASE_DIR` 环境变量配置。

## 2. 架构概览

```
配置文件字符串（如 "ring"）
        |
        v
generate_collective_implementation_from_input()   [Sys.cc]
        |
        |  创建 CollectiveImplementation，config_name = "ring"
        v
generate_collective_phase()                        [Sys.cc]
        |
        |  构建 AlgorithmParams 结构体
        |  调用 AlgorithmRegistry::instance().create("ring", params)
        v
AlgorithmRegistry                                  [AlgorithmRegistry.cc]
        |
        |  查找 REGISTER_ALGORITHM("ring", ...) 注册的工厂函数
        |  调用 lambda -> new Ring(...)
        v
返回 Algorithm* 实例
```

拓扑侧通过 `GeneralComplexTopology.cc` 中的 `TopologyRegistry` 遵循相同模式。

### 关键文件

| 文件 | 作用 |
|---|---|
| `collective/AlgorithmRegistry.hh/.cc` | 算法注册表单例 + `REGISTER_ALGORITHM` 宏 |
| `collective/AlgorithmParams.hh` | 传递给所有工厂函数的统一参数结构体 |
| `topology/TopologyRegistry.hh/.cc` | 拓扑注册表单例 + `REGISTER_TOPOLOGY` 宏 |
| `system/SimAiPaths.hh` | 通过 `SIMAI_BASE_DIR` 配置的路径工具 |

## 3. 如何添加自定义集合通信算法

### 第一步：创建算法类

在 `astra-sim/system/collective/` 目录下创建头文件和实现文件：

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

算法必须继承 `Algorithm` 基类并实现纯虚函数 `run()`。

### 第二步：使用宏注册

在 `.cc` 文件顶部（namespace 外）添加注册：

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
// ... 类实现 ...
}  // namespace AstraSim
```

`AlgorithmParams` 结构体提供所有常用参数：

| 字段 | 类型 | 说明 |
|---|---|---|
| `collective_type` | `ComType` | All_Reduce、All_Gather、Reduce_Scatter、All_to_All 等 |
| `id` | `int` | 节点/Rank ID |
| `layer_num` | `int` | 层编号 |
| `topology` | `BasicLogicalTopology*` | 当前维度的拓扑（需转换为具体类型） |
| `data_size` | `uint64_t` | 操作的数据大小 |
| `boost_mode` | `bool` | 性能优化标志 |
| `direction` | `RingTopology::Direction` | Ring 方向（顺时针/逆时针） |
| `injection_policy` | `InjectionPolicy` | 数据包注入策略 |
| `direct_collective_window` | `int` | AllToAll 类算法的滑动窗口大小 |
| `flow_models` | `shared_ptr<void>` | NCCL 流模型（用于 NcclFlowModel 类算法） |
| `num_channels` | `int` | 通道数量（用于 NcclFlowModel 类算法） |
| `sys` | `Sys*` | 模拟系统访问（谨慎使用） |
| `extras` | `unordered_map<string, any>` | 自定义参数扩展点 |

### 第三步：在配置文件中使用

在系统输入文件中指定算法名称：

```
all-reduce-implementation: myCustomAlgo
```

多维配置使用下划线分隔：

```
all-reduce-implementation: myCustomAlgo_doubleBinaryTree_ring
```

**无需修改 `Sys.cc`、`Common.hh` 或任何其他核心文件。**

## 4. 如何添加自定义网络拓扑

### 第一步：创建拓扑类

拓扑必须继承 `LogicalTopology`（或 `BasicLogicalTopology` / `ComplexLogicalTopology`）并实现以下虚函数：

```cpp
virtual int get_num_of_dimensions();
virtual int get_num_of_nodes_in_dimension(int dimension);
virtual BasicLogicalTopology* get_basic_topology_at_dimension(int dimension, ComType type);
```

### 第二步：使用宏注册

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
// ... 类实现 ...
}  // namespace AstraSim
```

`TopologyParams` 结构体：

| 字段 | 类型 | 说明 |
|---|---|---|
| `id` | `int` | 节点 ID |
| `dimension_size` | `int` | 当前维度的节点数量 |
| `index_in_dimension` | `int` | 节点在维度内的索引 |
| `offset` | `int` | 节点寻址步长 |
| `is_last_dim` | `bool` | 是否为最后一个维度 |
| `total_npus` | `int` | 所有维度 NPU 总数 |

### 第三步：注册对应的算法

拓扑通常需要与算法配对。使用相同的配置名注册两者：

```cpp
// 在 MyCustomAlgo.cc 中
REGISTER_ALGORITHM("myCustom", [...工厂函数...])

// 在 MyCustomTopology.cc 中
REGISTER_TOPOLOGY("myCustom", [...工厂函数...])
```

然后在配置文件中：

```
all-reduce-implementation: myCustom
```

系统将使用 `TopologyRegistry` 创建拓扑，使用 `AlgorithmRegistry` 创建算法，两者通过相同的 `"myCustom"` 名称解析。

## 5. SIMAI_BASE_DIR 环境变量

原先硬编码为 `/etc/astra-sim` 的日志和结果路径现在可配置：

```bash
# 使用本地目录（无需 sudo）
export SIMAI_BASE_DIR=./simai_data
mkdir -p $SIMAI_BASE_DIR

# 或保持默认值（/etc/astra-sim，需要写入权限）
# unset SIMAI_BASE_DIR
```

影响范围：
- NCCL 日志输出路径（`MockNcclLog`）
- Physical 模拟结果路径（`SimAiMain`）
- 构建脚本目录创建（`build.sh`）

Analytical 模式的结果输出路径（`./results/`）不受影响——它使用 `-r` 命令行参数。

## 6. 构建说明

### CMake Whole-Archive 链接

注册机制依赖各算法/拓扑 `.cc` 文件中的静态初始化器。为防止链接器在使用静态库时丢弃这些目标文件，所有前端 `CMakeLists.txt` 使用 `--whole-archive`：

```cmake
target_link_libraries(SimAI_analytical -Wl,--whole-archive AstraSim -Wl,--no-whole-archive)
```

如果新增链接 `libAstraSim.a` 的前端目标，需应用相同模式。

### C++ 标准

项目现在要求 **C++17**（从 C++11 升级），以支持 `AlgorithmParams::extras` 中的 `std::any`。

## 7. 修改文件清单

### 新增文件

| 文件 | 说明 |
|---|---|
| `system/collective/AlgorithmParams.hh` | 算法工厂统一参数结构体 |
| `system/collective/AlgorithmRegistry.hh` | 注册表类 + `REGISTER_ALGORITHM` 宏 |
| `system/collective/AlgorithmRegistry.cc` | 注册表单例实现 |
| `system/topology/TopologyRegistry.hh` | 注册表类 + `REGISTER_TOPOLOGY` 宏 |
| `system/topology/TopologyRegistry.cc` | 注册表单例实现 |
| `system/SimAiPaths.hh` | 可配置路径工具函数 |

### 修改文件

| 文件 | 改动 |
|---|---|
| `CMakeLists.txt` | C++ 标准 11 -> 17 |
| `system/Common.hh` | 增加 `config_name` 字段 + `UserRegistered` 枚举值 |
| `system/Sys.cc` | 注册表分发替换 if/else 链 |
| `system/MockNcclLog.h` | 路径改用 `SimAiPaths.hh` |
| `topology/GeneralComplexTopology.cc` | 注册表拓扑创建 |
| `collective/Ring.cc` | `REGISTER_ALGORITHM("ring", "oneRing")` |
| `collective/AllToAll.cc` | `REGISTER_ALGORITHM("direct", "oneDirect")` |
| `collective/DoubleBinaryTreeAllReduce.cc` | `REGISTER_ALGORITHM("doubleBinaryTree")` |
| `collective/HalvingDoubling.cc` | `REGISTER_ALGORITHM("halvingDoubling", "oneHalvingDoubling")` |
| `collective/NcclTreeFlowModel.cc` | `REGISTER_ALGORITHM("NcclFlowModel", "ncclRingTreeModel")` |
| `topology/RingTopology.cc` | 所有 Ring 变体的 `REGISTER_TOPOLOGY` |
| `topology/DoubleBinaryTreeTopology.cc` | `REGISTER_TOPOLOGY("doubleBinaryTree")` |
| `network_frontend/analytical/CMakeLists.txt` | `--whole-archive` 链接 |
| `network_frontend/phynet/CMakeLists.txt` | `--whole-archive` 链接 |
| `network_frontend/phynet/SimAiMain.cc` | 路径改用 `SimAiPaths.hh` |
| `build.sh` | `SIMAI_BASE_DIR` 环境变量 |
| `build/astra_ns3/build.sh` | `SIMAI_BASE_DIR` 环境变量 |

以上路径相对于 `astra-sim-alibabacloud/astra-sim/`（构建脚本和 CMakeLists 相对于 `astra-sim-alibabacloud/`）。
