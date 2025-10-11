#include "disketch/SimpleTopology.h"

#include <stdexcept>

namespace {
inline size_t path_hash(const TwoTuple& flow) {
    TwoTupleHash hasher;
    return hasher(flow) ^ 0x1234567887654321ULL;
}
}  // namespace

SimpleTopology::SimpleTopology(TopologyConfig config)
    : config_(std::move(config)), rng_(20251011) {}

const PathSetting& SimpleTopology::pick_path(const TwoTuple& flow) const {
    if (config_.paths.empty()) {
        throw std::runtime_error("路径配置为空");
    }
    size_t index = path_hash(flow) % config_.paths.size();
    return config_.paths[index];
}
