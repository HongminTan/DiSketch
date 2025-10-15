#ifndef DISKETCH_EPOCH_H
#define DISKETCH_EPOCH_H

#include <memory>
#include <vector>

#include "HeavyHitterDetector.h"
#include "Sketch.h"
#include "TwoTuple.h"

// 支持的 Sketch 类型
enum class SketchKind { CountMin, CountSketch, UnivMon };

// 流量估计对比指标
struct FlowMetric {
    TwoTuple flow;             // 二元组
    uint64_t ideal = 0;        // 理想情况统计的真实包数
    uint64_t full_sketch = 0;  // 单一 Sketch 的估计值
    uint64_t disketch = 0;     // DiSketch 的估计值
};

// 子 epoch 记录
struct SubepochRecord {
    int fragment_index = 0;        // 记录来源的 fragment 下标
    uint64_t epoch_id = 0;         // 所属 epoch 号
    uint32_t subepoch_id = 0;      // 当前记录对应的子 epoch 编号
    uint32_t total_subepochs = 1;  // 当前 epoch 内的总子 epoch 数量
    uint64_t hash_seed = 0;        // 子 epoch 使用的随机哈希种子
    uint64_t packet_count = 0;     // 子 epoch 内观察到的数据包数量
    double rho_estimate = 0.0;     // 该子 epoch 估计得到的 ρ 值
    std::shared_ptr<Sketch> snapshot;  // 子 epoch 结束时的 Sketch 快照
    SketchKind kind = SketchKind::CountSketch;  // fragment 使用的 Sketch 类型
};

// Fragment 单个 epoch 的报告
struct FragmentEpochReport {
    uint64_t epoch_id = 0;     // Report 对应的 epoch 号
    double rho_average = 0.0;  // 该 fragment 在 epoch 内的平均 ρ 值
    std::vector<SubepochRecord> records;  // 子 epoch 的 SubepochRecord 列表
};

// 单个 epoch 的全局汇总
struct EpochSummary {
    uint64_t epoch_id = 0;                     // 统计的 epoch 号
    double rho_average = 0.0;                  // 全局平均 ρ
    uint64_t total_packets = 0;                // 该 epoch 内的总包数
    uint64_t total_flows = 0;                  // 该 epoch 内的总流数
    double heavy_hitter_threshold = 0.0;       // 重流的阈值（包数）
    std::vector<FlowMetric> flow_metrics;      // 流的统计结果对比矩阵
    HeavyHitterDetector full_sketch_detector;  // Full Sketch 的重流检测指标
    HeavyHitterDetector disketch_detector;     // DiSketch 的重流检测指标
    std::vector<uint32_t>
        fragment_subepoch_counts;  // 每个 fragment 在该 epoch 的子epoch数量
};

#endif  // DISKETCH_EPOCH_H
