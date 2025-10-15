#ifndef DISKETCH_H
#define DISKETCH_H

#include "Epoch.h"
#include "Fragment.h"
#include "Ideal.h"
#include "PacketParser.h"
#include "Topology.h"

// DiSketch 整体配置
struct DiSketchConfig {
    std::string pcap_path;    // 输入数据集路径（pcap 文件）
    TopologyConfig topology;  // 拓扑与 fragment 配置
    uint32_t max_epochs = 0;  // 最大 epoch 数，0 表示直到数据结束
    uint32_t full_sketch_depth = 8;  // Full Sketch 使用的 Sketch 深度或层数
    double heavy_hitter_ratio = 0.0001;  // 重点流筛选占比（按包数）
    uint64_t epoch_duration_ns = 1000000000ULL;  // 每个 epoch 大小（纳秒）
    SketchKind sketch_kind = SketchKind::CountSketch;  // 拆分的 Sketch 类型
};

// DiSketch 运行报告
struct DiSketchReport {
    std::vector<EpochSummary> epochs;  // 按 epoch 汇总的统计结果
};

/// DiSketch 主管理器：协调多个 fragment、拓扑映射与聚合统计
class DiSketch {
   public:
    explicit DiSketch(DiSketchConfig config);

    // 执行完整的 DiSketch 流程，按输入数据的时间顺序迭代，返回按 epoch
    DiSketchReport run(const PacketParser::PacketVector& packets);

   private:
    DiSketchConfig config_;  // 全局配置
    Topology topology_;      // 提供流到路径的映射

    // 创建一个未拆分的 Sketch
    std::unique_ptr<Sketch> create_full_sketch(uint64_t memory_bytes) const;

    // 空间聚合: 从路径上多个 fragments 的时间聚合结果中恢复流量估计
    uint64_t spatial_aggregation(
        const TwoTuple& flow,
        const PathSetting& path,
        const std::vector<FragmentEpochReport>& fragment_reports) const;
};

#endif  // DISKETCH_H
