#ifndef DISKETCH_DISAGGREGATION_SIMULATION_H
#define DISKETCH_DISAGGREGATION_SIMULATION_H

#include "PacketParser.h"
#include "disketch/FragmentSimulator.h"
#include "disketch/SimpleTopology.h"
#include "disketch/SimulationTypes.h"

/// 将 fragment 仿真、拓扑映射与聚合统计串联成完整实验流程的调度器。
class DisaggregationSimulation {
   public:
    /// @param config 仿真参数（数据集路径、拓扑、Sketch 设置等）
    explicit DisaggregationSimulation(SimulationConfig config);

    /// 执行完整仿真，按输入数据的时间顺序迭代，并返回按 epoch 汇总的结果。
    SimulationReport run(const PacketParser::PacketVector& packets);

   private:
    SimulationConfig config_;  // 全局仿真配置
    SimpleTopology topology_;  // 提供流到路径的映射

    std::unique_ptr<Sketch> create_aggregated_sketch(uint64_t memory_bytes) const;  // 构造集中聚合使用的 Sketch
    uint64_t estimate_flow_from_records(const TwoTuple& flow,
                                        const PathSetting& path,
                                        const std::vector<SubepochRecord>& records,
                                        const TopologyConfig& topo_cfg) const;      // 从各 fragment 记录中估计流量
    bool record_contains_flow(const SubepochRecord& record,
                              const TwoTuple& flow,
                              bool single_hop,
                              bool boost_single_hop) const;                        // 判断记录是否与目标流匹配
};

#endif  // DISKETCH_DISAGGREGATION_SIMULATION_H
