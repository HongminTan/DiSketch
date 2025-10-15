#include "disketch/Topology.h"

Topology::Topology(TopologyConfig config)
    : config_(std::move(config)),
      hash_func_(std::make_unique<DefaultHashFunction>()) {}

const PathSetting& Topology::pick_path(const TwoTuple& flow) const {
    if (config_.paths.empty()) {
        return nullptr;
    }
    uint64_t index = hash_func_->hash(flow, uint64(config_.paths.size()),
                                      config_.paths.size());
    return config_.paths[index];
}
