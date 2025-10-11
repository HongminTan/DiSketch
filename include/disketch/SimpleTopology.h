#ifndef DISKETCH_SIMPLE_TOPOLOGY_H
#define DISKETCH_SIMPLE_TOPOLOGY_H

#include <random>

#include "TwoTuple.h"
#include "disketch/SimulationTypes.h"

/// 维护实验中使用的 fragment 拓扑，将流映射到可选路径。
class SimpleTopology {
   public:
    /// 使用完整的拓扑配置初始化。
    explicit SimpleTopology(TopologyConfig config);

    /// 访问指定 fragment 的配置。
    const FragmentSetting& fragment(int index) const {
        return config_.fragments.at(index);
    }

    /// 返回全部路径列表。
    const std::vector<PathSetting>& paths() const { return config_.paths; }

    /// 返回路径数量。
    int path_count() const { return static_cast<int>(config_.paths.size()); }

    /// 根据流哈希选择稳定路径，确保多轮仿真结果可复现。
    const PathSetting& pick_path(const TwoTuple& flow) const;

   private:
    TopologyConfig config_;         // 存储 fragment 与路径配置
    mutable std::mt19937_64 rng_;   // 提供哈希种子来源
};

#endif  // DISKETCH_SIMPLE_TOPOLOGY_H
