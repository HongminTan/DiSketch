#include "disketch/DisaggregationSimulation.h"

#include <algorithm>
#include <unordered_map>

#include "Ideal.h"

namespace {
inline uint32_t seeded_hash(const TwoTuple& flow,
                            uint64_t seed,
                            uint32_t mod) {
    TwoTupleHash hasher;
    uint64_t base = hasher(flow);
    uint64_t mixed = base ^ seed;
    return static_cast<uint32_t>(mixed % std::max<uint32_t>(1, mod));
}
}  // namespace

DisaggregationSimulation::DisaggregationSimulation(SimulationConfig config)
    : config_(std::move(config)), topology_(config_.topology) {}

SimulationReport DisaggregationSimulation::run(
    const PacketParser::PacketVector& packets) {
    SimulationReport report;
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

    std::vector<FragmentSimulator> disketch_fragments;
    disketch_fragments.reserve(config_.topology.fragments.size());
    for (size_t i = 0; i < config_.topology.fragments.size(); ++i) {
        disketch_fragments.emplace_back(static_cast<int>(i),
                                        config_.topology.fragments[i],
                                        epoch_duration);
    }

    TopologyConfig disco_topology = config_.topology;
    for (auto& frag : disco_topology.fragments) {
        frag.initial_subepoch = 1;
        frag.max_subepoch = 1;
        frag.boost_single_hop = false;
    }
    std::vector<FragmentSimulator> disco_fragments;
    disco_fragments.reserve(disco_topology.fragments.size());
    for (size_t i = 0; i < disco_topology.fragments.size(); ++i) {
        disco_fragments.emplace_back(static_cast<int>(i),
                                     disco_topology.fragments[i],
                                     epoch_duration);
    }

    uint64_t aggregated_memory = 0;
    for (const auto& frag : config_.topology.fragments) {
        aggregated_memory += frag.memory_bytes;
    }
    auto aggregated_sketch = create_aggregated_sketch(aggregated_memory);

    size_t packet_index = 0;
    for (uint64_t epoch = 0; epoch < total_epochs; ++epoch) {
        uint64_t epoch_start = first_ts + epoch * epoch_duration;
        uint64_t epoch_end = epoch_start + epoch_duration;

        for (auto& frag : disketch_fragments) {
            frag.begin_epoch(epoch, epoch_start);
        }
        for (auto& frag : disco_fragments) {
            frag.begin_epoch(epoch, epoch_start);
        }
        if (aggregated_sketch) {
            aggregated_sketch->clear();
        }

        Ideal ground_truth;
        ground_truth.clear();
        uint64_t epoch_packet_count = 0;
        std::unordered_map<TwoTuple, uint64_t, TwoTupleHash> epoch_counts;

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
            ground_truth.update(pkt.flow, 1);
            epoch_counts[pkt.flow] += 1;
            if (aggregated_sketch) {
                aggregated_sketch->update(pkt.flow, 1);
            }
            const auto& path = topology_.pick_path(pkt.flow);
            bool single_hop = path.node_indices.size() <= 1;
            for (int node_index : path.node_indices) {
                disketch_fragments[node_index].process_packet(
                    pkt.flow, ts, single_hop);
                disco_fragments[node_index].process_packet(
                    pkt.flow, ts, single_hop);
            }
            ++packet_index;
        }

        std::vector<SubepochRecord> disketch_records;
        std::vector<SubepochRecord> disco_records;
        double rho_sum = 0.0;
        uint32_t rho_count = 0;

        for (auto& frag : disketch_fragments) {
            FragmentEpochReport frag_report = frag.close_epoch();
            rho_sum += frag_report.rho_average;
            if (!frag_report.records.empty()) {
                rho_count += 1;
            }
            disketch_records.insert(disketch_records.end(),
                                    frag_report.records.begin(),
                                    frag_report.records.end());
        }
        for (auto& frag : disco_fragments) {
            FragmentEpochReport frag_report = frag.close_epoch();
            disco_records.insert(disco_records.end(),
                                 frag_report.records.begin(),
                                 frag_report.records.end());
        }

        EpochSummary summary;
        summary.epoch_id = epoch;
        summary.rho_average = rho_count == 0 ? 0.0 : rho_sum / rho_count;

        double threshold = epoch_packet_count * config_.heavy_hitter_ratio;
        std::vector<std::pair<TwoTuple, uint64_t>> flow_vec(
            epoch_counts.begin(), epoch_counts.end());
        std::sort(flow_vec.begin(), flow_vec.end(),
                  [](const auto& a, const auto& b) {
                      return a.second > b.second;
                  });
        if (config_.max_flow_report > 0 &&
            flow_vec.size() > config_.max_flow_report) {
            flow_vec.resize(config_.max_flow_report);
        }

        for (const auto& item : flow_vec) {
            FlowMetric metric;
            metric.flow = item.first;
            metric.ground_truth = item.second;
            if (aggregated_sketch) {
                metric.aggregated = aggregated_sketch->query(item.first);
            }
            const auto& path = topology_.pick_path(item.first);
            metric.disketch = estimate_flow_from_records(
                item.first, path, disketch_records, config_.topology);
            metric.disco = estimate_flow_from_records(
                item.first, path, disco_records, disco_topology);

            if (threshold <= 0.0 || metric.ground_truth >= threshold) {
                summary.flow_metrics.push_back(metric);
            }
        }

        report.epochs.push_back(std::move(summary));
    }

    return report;
}

std::unique_ptr<Sketch> DisaggregationSimulation::create_aggregated_sketch(
    uint64_t memory_bytes) const {
    if (memory_bytes == 0) {
        return nullptr;
    }
    switch (config_.sketch_kind) {
        case SketchKind::CountMin:
            return std::make_unique<CountMin>(config_.aggregated_depth,
                                              memory_bytes);
        case SketchKind::CountSketch:
            return std::make_unique<CountSketch>(config_.aggregated_depth,
                                                 memory_bytes);
        case SketchKind::UnivMon:
            return std::make_unique<UnivMon>(config_.aggregated_depth,
                                             memory_bytes,
                                             nullptr,
                                             UnivMonBackend::CountSketch);
    }
    return nullptr;
}

uint64_t DisaggregationSimulation::estimate_flow_from_records(
    const TwoTuple& flow,
    const PathSetting& path,
    const std::vector<SubepochRecord>& records,
    const TopologyConfig& topo_cfg) const {
    if (records.empty()) {
        return 0;
    }
    bool single_hop = path.node_indices.size() <= 1;
    std::vector<uint64_t> fragment_values;
    for (int node_index : path.node_indices) {
        bool boost = topo_cfg.fragments[node_index].boost_single_hop;
        for (const auto& record : records) {
            if (record.fragment_index != node_index) {
                continue;
            }
            if (!record.snapshot) {
                continue;
            }
            if (!record_contains_flow(record, flow, single_hop, boost)) {
                continue;
            }
            uint64_t value = record.snapshot->query(flow);
            value *= record.total_subepochs;
            fragment_values.push_back(value);
            break;
        }
    }
    if (fragment_values.empty()) {
        return 0;
    }
    if (config_.sketch_kind == SketchKind::CountMin) {
        return *std::min_element(fragment_values.begin(), fragment_values.end());
    }
    if (config_.sketch_kind == SketchKind::CountSketch) {
        std::sort(fragment_values.begin(), fragment_values.end());
        size_t mid = fragment_values.size() / 2;
        if (fragment_values.size() % 2 == 1) {
            return fragment_values[mid];
        }
        return (fragment_values[mid - 1] + fragment_values[mid]) / 2;
    }
    uint64_t sum = 0;
    for (uint64_t v : fragment_values) {
        sum += v;
    }
    return fragment_values.empty() ? 0 : sum / fragment_values.size();
}

bool DisaggregationSimulation::record_contains_flow(
    const SubepochRecord& record,
    const TwoTuple& flow,
    bool single_hop,
    bool boost_single_hop) const {
    uint32_t assigned =
        seeded_hash(flow, record.hash_seed, record.total_subepochs);
    if (record.subepoch_id == assigned) {
        return true;
    }
    if (boost_single_hop && single_hop && record.total_subepochs >= 2) {
        uint32_t second =
            (assigned + record.total_subepochs / 2) %
            std::max<uint32_t>(1, record.total_subepochs);
        return record.subepoch_id == second;
    }
    return false;
}
