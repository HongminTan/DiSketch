#include "Fragment.h"

Fragment::Fragment(int index,
                   const FragmentSetting& setting,
                   uint64_t epoch_duration_ns)
    : index_(index),
      setting_(setting),
      epoch_duration_ns_(epoch_duration_ns),
      subepoch_count_(std::max(kMinSubepoch, setting.initial_subepoch)),
      hash_func_(std::make_unique<DefaultHashFunction>()) {
    sketch_ = create_sketch();
}

std::unique_ptr<Sketch> Fragment::create_sketch() const {
    switch (setting_.kind) {
        case SketchKind::CountMin:
            return std::make_unique<CountMin>(setting_.depth,
                                              setting_.memory_bytes);
        case SketchKind::CountSketch:
            return std::make_unique<CountSketch>(setting_.depth,
                                                 setting_.memory_bytes);
        case SketchKind::UnivMon:
            return std::make_unique<UnivMon>(setting_.depth,
                                             setting_.memory_bytes, nullptr,
                                             UnivMonBackend::CountSketch);
    }
    return nullptr;
}

std::shared_ptr<Sketch> Fragment::clone_sketch() const {
    switch (setting_.kind) {
        case SketchKind::CountMin:
            return std::make_shared<CountMin>(
                *static_cast<CountMin*>(sketch_.get()));
        case SketchKind::CountSketch:
            return std::make_shared<CountSketch>(
                *static_cast<CountSketch*>(sketch_.get()));
        case SketchKind::UnivMon:
            return std::make_shared<UnivMon>(
                *static_cast<UnivMon*>(sketch_.get()));
    }
    return nullptr;
}

void Fragment::begin_epoch(uint64_t epoch_id, uint64_t epoch_start_ns) {
    epoch_id_ = epoch_id;
    epoch_start_ns_ = epoch_start_ns;
    current_subepoch_ = 0;
    packet_counter_ = 0;
    emitted_records_.clear();
    // 每个 seed 都由 fragment_index 和 epoch_id 唯一确定
    hash_seed_ = (static_cast<uint64_t>(index_) << 32) | epoch_id;
    sketch_ = create_sketch();
}

void Fragment::process_packet(const TwoTuple& flow,
                              uint64_t packet_time_ns,
                              bool single_hop) {
    if (packet_time_ns < epoch_start_ns_) {
        return;
    }
    uint64_t delta = packet_time_ns - epoch_start_ns_;
    uint64_t duration =
        std::max<uint64_t>(1, epoch_duration_ns_ / subepoch_count_);
    uint32_t subepoch_index = static_cast<uint32_t>(
        std::min<uint64_t>(delta / duration, subepoch_count_ - 1));
    if (subepoch_index > current_subepoch_) {
        flush_until(subepoch_index);
    }
    if (!should_track(flow, hash_seed_, subepoch_index, subepoch_count_,
                      single_hop, setting_.boost_single_hop)) {
        return;
    }
    sketch_->update(flow, 1);
    packet_counter_ += 1;
}

FragmentEpochReport Fragment::close_epoch() {
    flush_until(subepoch_count_);
    flush_current();

    FragmentEpochReport report;
    report.epoch_id = epoch_id_;

    // 从 emitted_records_ 计算平均 ρ
    double sum = 0.0;
    for (const auto& record : emitted_records_) {
        sum += record.rho_estimate;
    }
    report.rho_average =
        emitted_records_.empty() ? 0.0 : sum / emitted_records_.size();
    report.records = emitted_records_;

    adjust_subepoch(report.rho_average);

    return report;
}

void Fragment::flush_current() {
    if (packet_counter_ == 0) {
        return;
    }

    SubepochRecord record;
    record.fragment_index = index_;
    record.epoch_id = epoch_id_;
    record.subepoch_id = current_subepoch_;
    record.total_subepochs = subepoch_count_;
    record.kind = setting_.kind;
    record.hash_seed = hash_seed_;
    record.packet_count = packet_counter_;
    record.rho_estimate = compute_rho();
    record.snapshot = clone_sketch();

    emitted_records_.push_back(record);
    sketch_->clear();
}

void Fragment::flush_until(uint32_t target_subepoch) {
    while (current_subepoch_ < target_subepoch) {
        flush_current();
        current_subepoch_ += 1;
        packet_counter_ = 0;
    }
}

bool Fragment::should_track(const TwoTuple& flow,
                            uint64_t hash_seed,
                            uint32_t subepoch_id,
                            uint32_t total_subepochs,
                            bool single_hop,
                            bool boost_single_hop) {
    DefaultHashFunction hash_func;
    uint32_t assigned =
        static_cast<uint32_t>(hash_func.hash(flow, hash_seed, total_subepochs));
    if (subepoch_id == assigned) {
        return true;
    }
    // 对于单跳流，在两个 Subepoch 中采样，来补充缺失的多 fragment 信息
    if (boost_single_hop && single_hop && total_subepochs >= 2) {
        uint32_t second = (assigned + total_subepochs / 2) %
                          std::max<uint32_t>(1, total_subepochs);
        return subepoch_id == second;
    }
    return false;
}

uint64_t Fragment::temporal_aggregation(const TwoTuple& flow,
                                        const FragmentEpochReport& report,
                                        bool single_hop,
                                        bool boost_single_hop) {
    // 在该 fragment 的所有 subepoch records 中查找匹配的记录
    for (const auto& record : report.records) {
        if (!record.snapshot) {
            continue;
        }
        // 判断该 subepoch 是否采样了目标流
        if (!should_track(flow, record.hash_seed, record.subepoch_id,
                          record.total_subepochs, single_hop,
                          boost_single_hop)) {
            continue;
        }
        // 找到匹配的 subepoch,查询并归一化(乘以 subepoch 总数)
        uint64_t value = record.snapshot->query(flow);
        value *= record.total_subepochs;
        return value;
    }
    return 0;
}

double Fragment::compute_rho() {
    if (packet_counter_ == 0) {
        return 0.0;
    }
    switch (setting_.kind) {
        case SketchKind::CountMin: {
            // CountMin：ρ̂ = Σc_i / w
            auto* cm = static_cast<const CountMin*>(sketch_.get());
            const auto& counters = cm->get_raw_data();
            if (counters.empty() || counters[0].empty()) {
                return 0.0;
            }
            uint64_t width = counters[0].size();
            double sum = 0.0;
            for (const auto& row : counters) {
                for (uint32_t c : row) {
                    sum += static_cast<double>(c);
                }
            }
            return sum / static_cast<double>(width);
        }
        case SketchKind::CountSketch: {
            // CountSketch：ρ̂ = Σc_i² / w
            auto* cs = static_cast<const CountSketch*>(sketch_.get());
            const auto& counters = cs->get_raw_data();
            if (counters.empty() || counters[0].empty()) {
                return 0.0;
            }
            uint64_t width = counters[0].size();
            double sum_square = 0.0;
            for (const auto& row : counters) {
                for (int c : row) {
                    double val = static_cast<double>(c);
                    sum_square += val * val;
                }
            }
            return sum_square / static_cast<double>(width);
        }
        default:
            // UnivMon 不需要计算 rho
            return 0.0;
    }
}

void Fragment::adjust_subepoch(double avg_rho) {
    // UnivMon 不进行动态调整
    if (setting_.kind == SketchKind::UnivMon) {
        subepoch_count_ = setting_.initial_subepoch;
        return;
    }

    uint32_t next = subepoch_count_;
    if (avg_rho > 2.0 * setting_.rho_target &&
        subepoch_count_ < setting_.max_subepoch) {
        next = std::min(setting_.max_subepoch, subepoch_count_ * 2);
    } else if (avg_rho < setting_.rho_target * 0.5 &&
               subepoch_count_ > kMinSubepoch) {
        next = std::max<uint32_t>(kMinSubepoch, subepoch_count_ / 2);
    }
    subepoch_count_ = next;
}