#ifndef DISKETCH_SIMULATION_TYPES_H
#define DISKETCH_SIMULATION_TYPES_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Sketch.h"
#include "TwoTuple.h"

enum class SketchKind { CountMin, CountSketch, UnivMon };

struct FragmentSetting {
    std::string name;                // 片段名称，用于在拓扑与输出中定位节点
    SketchKind kind = SketchKind::CountSketch;  // 片段使用的 Sketch 种类
    uint64_t memory_bytes = 0;        // 分配给片段的 Sketch 内存大小（字节）
    uint32_t depth = 4;               // CountMin/CountSketch 的行数，或 UnivMon 的层数
    uint32_t initial_subepoch = 1;    // 初始的子 epoch 数量
    uint32_t max_subepoch = 8;        // 允许自动扩展的子 epoch 上限
    double rho_target = 1.0;          // 目标 ρ（噪声界）阈值，用于动态扩展子 epoch
    bool boost_single_hop = false;    // 对经由单跳路径的流是否放大采样权重
    double background_ratio = 0.0;    // 背景流量比例，用于估计附加噪声强度
};

struct PathSetting {
    std::string name;                 // 路径名称
    std::vector<int> node_indices;    // 该路径上的片段下标序列，对应 fragments 中的索引
};

struct SubepochRecord {
    int fragment_index = 0;                    // 记录来源的片段下标
    uint64_t epoch_id = 0;                     // 所属 epoch 号
    uint32_t subepoch_id = 0;                  // 当前记录对应的子 epoch 编号
    uint32_t total_subepochs = 1;              // 当前 epoch 内的总子 epoch 数量
    SketchKind kind = SketchKind::CountSketch; // 片段使用的 Sketch 类型
    uint64_t hash_seed = 0;                    // 子 epoch 使用的随机哈希种子
    uint64_t packet_count = 0;                 // 子 epoch 内观察到的数据包数量
    double rho_estimate = 0.0;                 // 该子 epoch 估计得到的 ρ 值
    std::shared_ptr<Sketch> snapshot;          // 子 epoch 结束时的 Sketch 快照
};

struct FragmentEpochReport {
    uint64_t epoch_id = 0;                     // 报告对应的 epoch 号
    double rho_average = 0.0;                  // 该 fragment 在 epoch 内的平均 ρ 值
    uint32_t decided_subepochs = 1;            // 最终决策的子 epoch 数量
    std::vector<SubepochRecord> records;       // 子 epoch 的输出记录列表
};

struct TopologyConfig {
    std::vector<FragmentSetting> fragments;    // 全局 fragment 配置
    std::vector<PathSetting> paths;            // 可选的路径集合
};

struct FlowMetric {
    TwoTuple flow;                 // 目标流的 (src, dst) 二元组
    uint64_t ground_truth = 0;     // 理想情况统计的真实包数
    uint64_t aggregated = 0;       // 集中部署（不分片）聚合的估计值
    uint64_t disco = 0;            // DISCO 方案的估计值
    uint64_t disketch = 0;         // DiSketch 方案的估计值
};

struct EpochSummary {
    uint64_t epoch_id = 0;                     // 统计的 epoch 号
    double rho_average = 0.0;                  // 全局平均 ρ
    std::vector<FlowMetric> flow_metrics;      // 重点流的对比结果
};

struct SimulationConfig {
    std::string pcap_path;                       // 输入数据集路径（pcap 文件）
    SketchKind sketch_kind = SketchKind::CountSketch;  // 整体仿真采用的基线 Sketch 类型
    uint64_t epoch_duration_ns = 1000000000ULL;  // 每个 epoch 的时间窗口（纳秒）
    uint32_t max_epochs = 0;                     // 最多仿真的 epoch 数，0 表示直到数据结束
    uint32_t aggregated_depth = 8;               // 聚合端使用的 Sketch 深度或层数
    double heavy_hitter_ratio = 0.0001;          // 重点流筛选占比（按包数）
    uint32_t max_flow_report = 50;               // 输出的重点流最大数量
    TopologyConfig topology;                     // 拓扑与 fragment 配置
};

struct SimulationReport {
    std::vector<EpochSummary> epochs;            // 按 epoch 汇总的统计结果
};

#endif  // DISKETCH_SIMULATION_TYPES_H
