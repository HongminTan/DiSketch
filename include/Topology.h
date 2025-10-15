#ifndef DISKETCH_TOPOLOGY_H
#define DISKETCH_TOPOLOGY_H

#include "Fragment.h"
#include "HashFunction.h"
#include "TwoTuple.h"

// 路径配置
struct PathSetting {
    std::string name;  // 路径名称
    std::vector<int>
        node_indices;  // fragment 下标序列，对应 fragments 中的索引
};

// 拓扑配置
struct TopologyConfig {
    std::vector<FragmentSetting> fragments;  // 全局 fragment 配置
    std::vector<PathSetting> paths;          // 可选的路径集合
};

// 维护 fragment 拓扑，将流映射到可选路径
class Topology {
   public:
    // 使用完整的拓扑配置初始化
    explicit Topology(TopologyConfig config);

    // 访问指定 fragment 的配置
    const FragmentSetting& fragment(int index) const {
        return config_.fragments.at(index);
    }

    // 返回全部路径列表
    const std::vector<PathSetting>& paths() const { return config_.paths; }

    // 返回路径数量
    int path_count() const { return static_cast<int>(config_.paths.size()); }

    // 根据流哈希选择稳定路径，确保多轮运行结果可复现
    const PathSetting& pick_path(const TwoTuple& flow) const;

   private:
    TopologyConfig config_;  // 存储 fragment 与路径配置
    std::unique_ptr<HashFunction> hash_func_;
};

#endif  // DISKETCH_TOPOLOGY_H
