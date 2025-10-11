#include "disketch/FragmentSimulator.h"

#include <algorithm>
#include <numeric>

namespace {
constexpr uint32_t kMinSubepoch = 1;

inline uint32_t seeded_hash(const TwoTuple& flow,
                            uint64_t seed,
                            uint32_t mod) {
    TwoTupleHash hasher;
    uint64_t base = hasher(flow);
    uint64_t mixed = base ^ seed;
    return static_cast<uint32_t>(mixed % std::max<uint32_t>(1, mod));
}
}  // namespace

FragmentSimulator::FragmentSimulator(int index,
                                     const FragmentSetting& setting,
                                     uint64_t epoch_duration_ns)
    : index_(index),
      setting_(setting),
      epoch_duration_ns_(epoch_duration_ns),
      subepoch_count_(std::max(kMinSubepoch, setting.initial_subepoch)),
      rng_(static_cast<uint64_t>(index) * 1315423911ULL) {
    sketch_ = create_sketch();
}

std::unique_ptr<Sketch> FragmentSimulator::create_sketch() const {
    switch (setting_.kind) {
        case SketchKind::CountMin:
            return std::make_unique<CountMin>(setting_.depth,
                                              setting_.memory_bytes);
        case SketchKind::CountSketch:
            return std::make_unique<CountSketch>(setting_.depth,
                                                 setting_.memory_bytes);
        case SketchKind::UnivMon:
            return std::make_unique<UnivMon>(setting_.depth,
                                             setting_.memory_bytes,
                                             nullptr,
                                             UnivMonBackend::CountSketch);
    }
    return nullptr;
}

std::shared_ptr<Sketch> FragmentSimulator::clone_sketch() const {
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

void FragmentSimulator::begin_epoch(uint64_t epoch_id,
                                    uint64_t epoch_start_ns) {
    epoch_id_ = epoch_id;
    epoch_start_ns_ = epoch_start_ns;
    current_subepoch_ = 0;
    context_ = SubepochContext();
    emitted_records_.clear();
    epoch_rho_values_.clear();
    std::uniform_int_distribution<uint64_t> dist;
    hash_seed_ = dist(rng_);
    sketch_ = create_sketch();
}

void FragmentSimulator::process_packet(const TwoTuple& flow,
                                       uint64_t packet_time_ns,
                                       bool single_hop) {
    if (packet_time_ns < epoch_start_ns_) {
        return;
    }
    uint64_t delta = packet_time_ns - epoch_start_ns_;
    uint64_t duration = std::max<uint64_t>(1, subepoch_duration_ns());
    uint32_t subepoch_index =
        static_cast<uint32_t>(
            std::min<uint64_t>(delta / duration, subepoch_count_ - 1));
    if (subepoch_index > current_subepoch_) {
        flush_until(subepoch_index);
    }
    if (!should_track(flow, subepoch_index, single_hop)) {
        return;
    }
    sketch_->update(flow, 1);
    context_.packet_counter += 1;
    context_.flow_counter[flow] += 1;
}

FragmentEpochReport FragmentSimulator::close_epoch() {
    flush_until(subepoch_count_);
    flush_current();

    FragmentEpochReport report;
    report.epoch_id = epoch_id_;
    double sum = 0.0;
    for (double v : epoch_rho_values_) {
        sum += v;
    }
    report.rho_average = epoch_rho_values_.empty()
                             ? 0.0
                             : sum / epoch_rho_values_.size();
    report.decided_subepochs = subepoch_count_;
    report.records = emitted_records_;

    adjust_subepoch();

    return report;
}

void FragmentSimulator::flush_until(uint32_t target_subepoch) {
    while (current_subepoch_ < target_subepoch) {
        flush_current();
        current_subepoch_ += 1;
        context_ = SubepochContext();
    }
}

void FragmentSimulator::flush_current() {
    if (context_.packet_counter == 0) {
        return;
    }
    SubepochRecord record;
    record.fragment_index = index_;
    record.epoch_id = epoch_id_;
    record.subepoch_id = current_subepoch_;
    record.total_subepochs = subepoch_count_;
    record.kind = setting_.kind;
    record.hash_seed = hash_seed_;
    record.packet_count = context_.packet_counter;
    record.rho_estimate = compute_rho(context_);
    record.snapshot = clone_sketch();

    emitted_records_.push_back(record);
    epoch_rho_values_.push_back(record.rho_estimate);
    sketch_->clear();
}

double FragmentSimulator::compute_rho(const SubepochContext& ctx) const {
    if (ctx.packet_counter == 0) {
        return 0.0;
    }
    uint64_t width = 1;
    switch (setting_.kind) {
        case SketchKind::CountMin:
            width = setting_.memory_bytes /
                    std::max<uint64_t>(1, setting_.depth * CMBUCKET_SIZE);
            break;
        case SketchKind::CountSketch:
            width = setting_.memory_bytes /
                    std::max<uint64_t>(1, setting_.depth * CSSKETCH_BUCKET_SIZE);
            break;
        case SketchKind::UnivMon:
            width = setting_.memory_bytes /
                    std::max<uint64_t>(1, setting_.depth * CSSKETCH_BUCKET_SIZE);
            break;
    }
    width = std::max<uint64_t>(1, width);

    double rho = 0.0;
    if (setting_.kind == SketchKind::CountMin) {
        double sum = 0.0;
        for (const auto& kv : ctx.flow_counter) {
            sum += static_cast<double>(kv.second);
        }
        rho = sum / static_cast<double>(width);
    } else {
        double sum_square = 0.0;
        for (const auto& kv : ctx.flow_counter) {
            sum_square += static_cast<double>(kv.second) *
                          static_cast<double>(kv.second);
        }
        rho = sum_square / static_cast<double>(width);
    }
    if (setting_.background_ratio > 0.0) {
        rho *= (1.0 + setting_.background_ratio);
    }
    return rho;
}

bool FragmentSimulator::should_track(const TwoTuple& flow,
                                     uint32_t subepoch_index,
                                     bool single_hop) const {
    uint32_t assigned = seeded_hash(flow, hash_seed_, subepoch_count_);
    if (subepoch_index == assigned) {
        return true;
    }
    if (setting_.boost_single_hop && single_hop && subepoch_count_ >= 2) {
        uint32_t second =
            (assigned + subepoch_count_ / 2) % std::max<uint32_t>(1, subepoch_count_);
        return subepoch_index == second;
    }
    return false;
}

uint64_t FragmentSimulator::subepoch_duration_ns() const {
    return std::max<uint64_t>(1, epoch_duration_ns_ / subepoch_count_);
}

void FragmentSimulator::adjust_subepoch() {
    double avg = epoch_rho_values_.empty()
                     ? 0.0
                     : std::accumulate(epoch_rho_values_.begin(),
                                       epoch_rho_values_.end(), 0.0) /
                           epoch_rho_values_.size();
    uint32_t next = subepoch_count_;
    if (avg > 2.0 * setting_.rho_target &&
        subepoch_count_ < setting_.max_subepoch) {
        next = std::min(setting_.max_subepoch, subepoch_count_ * 2);
    } else if (avg < setting_.rho_target * 0.5 && subepoch_count_ > kMinSubepoch) {
        next = std::max<uint32_t>(kMinSubepoch, subepoch_count_ / 2);
    }
    subepoch_count_ = next;
}
