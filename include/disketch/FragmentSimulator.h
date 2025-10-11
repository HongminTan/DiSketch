#ifndef DISKETCH_FRAGMENT_SIMULATOR_H
#define DISKETCH_FRAGMENT_SIMULATOR_H

#include <cstdint>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

#include "CountMin.h"
#include "CountSketch.h"
#include "TwoTuple.h"
#include "UnivMon.h"
#include "disketch/SimulationTypes.h"

/// 负责模拟单个 fragment（部署点）在一个 epoch 内的行为。
/// 维护子 epoch 划分、Sketch 更新、ρ 估计和自适应调节逻辑。
class FragmentSimulator {
   public:
    /// @param index fragment 在拓扑配置中的下标，用于输出标识
    /// @param setting fragment 的 Sketch 与自适应控制参数
    /// @param epoch_duration_ns 一个 epoch 的时间长度（纳秒）
    FragmentSimulator(int index,
                      const FragmentSetting& setting,
                      uint64_t epoch_duration_ns);

    /// 以给定的 epoch 号与开始时间重置内部状态。
    void begin_epoch(uint64_t epoch_id, uint64_t epoch_start_ns);

    /// 处理单个数据包，并根据是否单跳决定是否采样。
    /// @param flow 数据包对应的流二元组
    /// @param packet_time_ns 数据包到达时间（纳秒）
    /// @param single_hop 该包是否只经过一个 fragment
    void process_packet(const TwoTuple& flow,
                        uint64_t packet_time_ns,
                        bool single_hop);

    /// 在 epoch 结束时输出片段的子 epoch 汇总，并重置为下一轮做准备。
    FragmentEpochReport close_epoch();

    /// 返回 fragment 的静态配置。
    const FragmentSetting& config() const { return setting_; }

   private:
    struct SubepochContext {
        std::unordered_map<TwoTuple, uint64_t, TwoTupleHash> flow_counter;  // 当前子 epoch 的流频统计
        uint64_t packet_counter = 0;                                        // 当前子 epoch 的包计数
    };

    int index_;                     // fragment 下标
    FragmentSetting setting_;       // fragment 配置
    uint64_t epoch_duration_ns_;    // epoch 长度
    uint64_t hash_seed_ = 0;        // 当前 epoch 使用的哈希种子
    uint64_t epoch_id_ = 0;         // 当前 epoch 号
    uint64_t epoch_start_ns_ = 0;   // 当前 epoch 起始时间
    uint32_t subepoch_count_ = 1;   // 当前 epoch 的子 epoch 总数
    uint32_t current_subepoch_ = 0; // 已处理的子 epoch 下标

    std::mt19937_64 rng_;
    std::unique_ptr<Sketch> sketch_;
    std::unique_ptr<Sketch> create_sketch() const;
    std::shared_ptr<Sketch> clone_sketch() const;

    SubepochContext context_;
    std::vector<double> epoch_rho_values_;     // 本 epoch 内每个子 epoch 的 ρ 估计
    std::vector<SubepochRecord> emitted_records_;  // 输出的子 epoch 记录

    void flush_until(uint32_t target_subepoch);  // 将内部状态推进到指定子 epoch
    void flush_current();                        // 输出当前子 epoch
    double compute_rho(const SubepochContext& ctx) const;  // 根据统计估计 ρ
    bool should_track(const TwoTuple& flow,
                      uint32_t subepoch_index,
                      bool single_hop) const;   // 判断是否保留该包
    uint64_t subepoch_duration_ns() const;       // 计算当前子 epoch 的时间跨度
    void adjust_subepoch();                      // 根据 ρ 动态调整子 epoch 数
};

#endif  // DISKETCH_FRAGMENT_SIMULATOR_H
