#ifndef DISKETCH_FRAGMENT_H
#define DISKETCH_FRAGMENT_H

#include "CountMin.h"
#include "CountSketch.h"
#include "Epoch.h"
#include "HashFunction.h"
#include "TwoTuple.h"
#include "UnivMon.h"

// 最小 subepoch 数量
constexpr uint32_t kMinSubepoch = 1;

// Fragment 配置参数
struct FragmentSetting {
    std::string name;           // fragment 名称
    uint32_t depth = 4;         // CountMin/CountSketch 的行数 或 UnivMon 的层数
    double rho_target = 1.0;    // 目标 ρ 阈值，用于动态扩展 subepoch 数量
    uint64_t memory_bytes = 0;  // 分配给 fragment 的 Sketch 内存大小（字节）
    uint32_t max_subepoch = 8;  // 允许自动扩展的 subepoch 上限
    uint32_t initial_subepoch = 1;  // 初始的 subepoch 数量
    bool boost_single_hop = false;  // 单跳流是否在多个 subepoch 中采样
    SketchKind kind = SketchKind::CountSketch;  // fragment 使用的 Sketch 种类
};

// 负责管理单个 fragment 在一个 epoch 内的行为
// 维护 subepoch 划分、Sketch 更新、ρ 估计和自适应调节逻辑
class Fragment {
   public:
    /**
     * @param index: fragment 在拓扑中的下标
     * @param setting: fragment 的 Sketch 与自适应控制参数
     * @param epoch_duration_ns: 一个 epoch 的时间长度（纳秒）
     */
    Fragment(int index,
             const FragmentSetting& setting,
             uint64_t epoch_duration_ns);

    // 以给定的 epoch 号与开始时间重置内部状态。
    void begin_epoch(uint64_t epoch_id, uint64_t epoch_start_ns);

    /** 处理单个数据包
     * @param flow: 数据包对应的流二元组
     * @param packet_time_ns: 数据包到达时间（纳秒）
     * @param single_hop: 该包是否只经过一个 fragment
     */
    void process_packet(const TwoTuple& flow,
                        uint64_t packet_time_ns,
                        bool single_hop);

    // 在 epoch 结束时输出 fragment 的 subepoch 汇总，并重置为下一轮做准备
    FragmentEpochReport close_epoch();

    // 返回 fragment 的静态配置
    const FragmentSetting& config() const { return setting_; }

    // 判断某个流是否应该被指定 subepoch 采样
    static bool should_track(const TwoTuple& flow,
                             uint64_t hash_seed,
                             uint32_t subepoch_id,
                             uint32_t total_subepochs,
                             bool single_hop,
                             bool boost_single_hop);

    // 时间聚合: 从 FragmentEpochReport 的多个 subepoch 中恢复流量估计
    static uint64_t temporal_aggregation(const TwoTuple& flow,
                                         const FragmentEpochReport& report,
                                         bool single_hop,
                                         bool boost_single_hop);

   private:
    int index_;                       // fragment 下标
    FragmentSetting setting_;         // fragment 配置
    uint64_t epoch_duration_ns_;      // epoch 长度
    uint64_t hash_seed_ = 0;          // 当前 epoch 使用的哈希种子
    uint64_t epoch_id_ = 0;           // 当前 epoch 号
    uint64_t epoch_start_ns_ = 0;     // 当前 epoch 起始时间
    uint32_t subepoch_count_ = 1;     // 当前 epoch 的 subepoch 总数
    uint32_t current_subepoch_ = 0;   // 已处理的 subepoch 下标
    uint64_t packet_counter_ = 0;     // 当前 subepoch 的包计数
    uint64_t subepoch_duration_ = 0;  // subepoch 时长
    double current_rho_ = 0.0;        // 当前 subepoch 的 ρ 值
    std::vector<SubepochRecord> emitted_records_;  // 已输出的 subepoch 记录

    std::unique_ptr<HashFunction> hash_func_;
    std::unique_ptr<Sketch> sketch_;
    std::unique_ptr<Sketch> create_sketch() const;
    std::shared_ptr<Sketch> clone_sketch() const;

    // 输出当前 subepoch 的 SubepochRecord
    void flush_current();
    // 将内部状态推进到指定子 epoch
    void flush_until(uint32_t target_subepoch);

    // 更新 sketch 并增量更新 current_rho_
    void update_sketch_and_rho(const TwoTuple& flow);
    // 根据 ρ 动态调整 subepoch 数
    void adjust_subepoch(double avg_rho);
};

#endif  // DISKETCH_FRAGMENT_H
