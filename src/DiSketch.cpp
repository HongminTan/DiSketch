#include "DiSketch.h"

DiSketch::DiSketch(DiSketchConfig config)
    : config_(std::move(config)), topology_(config_.topology) {}

DiSketchReport DiSketch::run(const PacketParser::PacketVector& packets) {
    DiSketchReport report;
    if (packets.empty()) {
        return report;
    }

    uint64_t epoch_duration = std::max<uint64_t>(1, config_.epoch_duration_ns);
    uint64_t first_ts = packets.front().timestamp.count();
    uint64_t last_ts = packets.back().timestamp.count();
    if (last_ts < first_ts) {
        return report;
    }
    uint64_t total_epochs = (last_ts - first_ts) / epoch_duration + 1;
    if (config_.max_epochs > 0) {
        total_epochs = std::min<uint64_t>(total_epochs, config_.max_epochs);
    }

    // 准备 fragments
    std::vector<Fragment> disketch_fragments;
    disketch_fragments.reserve(config_.topology.fragments.size());
    for (size_t i = 0; i < config_.topology.fragments.size(); ++i) {
        disketch_fragments.emplace_back(
            static_cast<int>(i), config_.topology.fragments[i], epoch_duration);
    }

    // 准备 Full Sketch
    uint64_t full_sketch_memory = 0;
    for (const auto& frag : config_.topology.fragments) {
        full_sketch_memory += frag.memory_bytes;
    }
    auto full_sketch = create_full_sketch(full_sketch_memory);

    // 逐 epoch 处理数据包
    size_t packet_index = 0;
    for (uint64_t epoch = 0; epoch < total_epochs; ++epoch) {
        uint64_t epoch_start = first_ts + epoch * epoch_duration;
        uint64_t epoch_end = epoch_start + epoch_duration;

        for (auto& frag : disketch_fragments) {
            frag.begin_epoch(epoch, epoch_start);
        }
        if (full_sketch) {
            full_sketch->clear();
        }

        Ideal ideal;
        ideal.clear();
        uint64_t epoch_packet_count = 0;

        // 处理一个 epoch
        while (packet_index < packets.size()) {
            const auto& pkt = packets[packet_index];
            uint64_t ts = pkt.timestamp.count();
            if (ts < epoch_start) {
                ++packet_index;
                continue;
            }
            if (ts >= epoch_end) {
                break;
            }
            epoch_packet_count += 1;
            ideal.update(pkt.flow, 1);
            if (full_sketch) {
                full_sketch->update(pkt.flow, 1);
            }
            const auto& path = topology_.pick_path(pkt.flow);
            bool single_hop = path.node_indices.size() <= 1;
            for (int node_index : path.node_indices) {
                disketch_fragments[node_index].process_packet(pkt.flow, ts,
                                                              single_hop);
            }
            ++packet_index;
        }

        // 收集当前 epoch 所有 fragment 的报告
        std::vector<FragmentEpochReport> fragment_reports;
        double rho_sum = 0.0;
        uint32_t rho_count = 0;

        for (auto& frag : disketch_fragments) {
            FragmentEpochReport frag_report = frag.close_epoch();
            rho_sum += frag_report.rho_average;
            if (!frag_report.records.empty()) {
                rho_count += 1;
            }
            fragment_reports.push_back(std::move(frag_report));
        }

        // 整理当前 epoch 报告
        EpochSummary summary;
        summary.epoch_id = epoch;
        summary.rho_average = rho_count == 0 ? 0.0 : rho_sum / rho_count;
        summary.total_packets = epoch_packet_count;
        summary.total_flows = ideal.get_flow_count();

        // 记录每个 fragment 的子epoch数量
        for (const auto& frag_report : fragment_reports) {
            uint32_t subepoch_count = 0;
            if (!frag_report.records.empty()) {
                subepoch_count = frag_report.records[0].total_subepochs;
            }
            summary.fragment_subepoch_counts.push_back(subepoch_count);
        }
        double threshold = epoch_packet_count * config_.heavy_hitter_ratio;
        summary.heavy_hitter_threshold = threshold;

        // 重置检测器
        summary.full_sketch_detector.reset();
        summary.disketch_detector.reset();

        // 遍历所有流,进行三种方法的对比评估
        const auto& epoch_counts = ideal.get_raw_data();
        for (const auto& flow_pair : epoch_counts) {
            const TwoTuple& flow = flow_pair.first;
            uint64_t packet_count = flow_pair.second;

            // 判断是否为真实重流
            bool is_real_heavy =
                (threshold <= 0.0 || packet_count >= threshold);

            FlowMetric metric;
            metric.flow = flow;
            metric.ideal = packet_count;

            if (full_sketch) {
                metric.full_sketch = full_sketch->query(flow);
                // 判断 Full Sketch 是否检测为重流
                bool detected_by_full = (metric.full_sketch >= threshold);

                // 更新 Full Sketch 检测器的统计
                if (is_real_heavy && detected_by_full) {
                    summary.full_sketch_detector.tp++;
                } else if (is_real_heavy && !detected_by_full) {
                    summary.full_sketch_detector.fn++;
                } else if (!is_real_heavy && detected_by_full) {
                    summary.full_sketch_detector.fp++;
                } else {
                    summary.full_sketch_detector.tn++;
                }
            }

            const auto& path = topology_.pick_path(flow);
            // 时空聚合: 先在每个 fragment 进行时间聚合,再在路径上进行空间聚合
            metric.disketch = spatial_aggregation(flow, path, fragment_reports);

            // 判断 DiSketch 是否检测为重流
            bool detected_by_disketch = (metric.disketch >= threshold);

            // 更新 DiSketch 检测器的统计
            if (is_real_heavy && detected_by_disketch) {
                summary.disketch_detector.tp++;
            } else if (is_real_heavy && !detected_by_disketch) {
                summary.disketch_detector.fn++;
            } else if (!is_real_heavy && detected_by_disketch) {
                summary.disketch_detector.fp++;
            } else {
                summary.disketch_detector.tn++;
            }

            if (threshold <= 0.0 || metric.ideal >= threshold) {
                summary.flow_metrics.push_back(metric);
            }
        }
        report.epochs.push_back(std::move(summary));
    }

    return report;
}

std::unique_ptr<Sketch> DiSketch::create_full_sketch(
    uint64_t memory_bytes) const {
    if (memory_bytes == 0) {
        return nullptr;
    }
    switch (config_.sketch_kind) {
        case SketchKind::CountMin:
            return std::make_unique<CountMin>(config_.full_sketch_depth,
                                              memory_bytes);
        case SketchKind::CountSketch:
            return std::make_unique<CountSketch>(config_.full_sketch_depth,
                                                 memory_bytes);
        case SketchKind::UnivMon:
            return std::make_unique<UnivMon>(config_.full_sketch_depth,
                                             memory_bytes, nullptr,
                                             UnivMonBackend::CountSketch);
    }
    return nullptr;
}

uint64_t DiSketch::spatial_aggregation(
    const TwoTuple& flow,
    const PathSetting& path,
    const std::vector<FragmentEpochReport>& fragment_reports) const {
    if (fragment_reports.empty()) {
        return 0;
    }

    bool single_hop = path.node_indices.size() <= 1;
    std::vector<uint64_t> fragment_values;

    // 遍历路径上的每个节点,对每个 fragment 进行时间聚合
    for (int node_index : path.node_indices) {
        if (node_index >= static_cast<int>(fragment_reports.size())) {
            continue;
        }
        const FragmentEpochReport& report = fragment_reports[node_index];
        // 验证 fragment_index 一致性
        if (!report.records.empty() &&
            report.records[0].fragment_index != node_index) {
            continue;
        }

        // 获得 Fragment 的时间聚合
        bool boost_single_hop =
            config_.topology.fragments[node_index].boost_single_hop;
        uint64_t value = Fragment::temporal_aggregation(
            flow, report, single_hop, boost_single_hop);
        if (value > 0) {
            fragment_values.push_back(value);
        }
    }
    if (fragment_values.empty()) {
        return 0;
    }
    // 根据 Sketch 类型进行空间聚合
    switch (config_.sketch_kind) {
        case SketchKind::CountMin:
            // CountMin: 取最小值
            return *std::min_element(fragment_values.begin(),
                                     fragment_values.end());
        case SketchKind::CountSketch: {
            // CountSketch: 取中位数
            std::sort(fragment_values.begin(), fragment_values.end());
            size_t mid = fragment_values.size() / 2;
            if (fragment_values.size() % 2 == 1) {
                return fragment_values[mid];
            }
            return (fragment_values[mid - 1] + fragment_values[mid]) / 2;
        }
        case SketchKind::UnivMon: {
            // UnivMon: 取平均值
            uint64_t sum = 0;
            for (uint64_t v : fragment_values) {
                sum += v;
            }
            return sum / fragment_values.size();
        }
        default:
            return 0;
    }
}
