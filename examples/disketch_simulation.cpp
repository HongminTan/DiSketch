#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

#include "PacketParser.h"
#include "disketch/DisaggregationSimulation.h"

namespace {

std::string trim(const std::string& input) {
    size_t start = input.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = input.find_last_not_of(" \t\r\n");
    return input.substr(start, end - start + 1);
}

bool to_bool(const std::string& value) {
    return value == "1" || value == "true" || value == "TRUE";
}

SketchKind parse_sketch_kind(const std::string& value) {
    if (value == "CountMin" || value == "countmin") {
        return SketchKind::CountMin;
    }
    if (value == "UnivMon" || value == "univmon") {
        return SketchKind::UnivMon;
    }
    return SketchKind::CountSketch;
}

struct PendingPath {
    std::string name;
    std::vector<std::string> node_names;
};

bool load_config(const std::string& path, SimulationConfig& config) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "无法打开配置文件: " << path << std::endl;
        return false;
    }

    std::unordered_map<std::string, int> fragment_index;
    std::vector<PendingPath> pending_paths;

    FragmentSetting* current_fragment = nullptr;
    PendingPath* current_path = nullptr;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line == "[fragment]") {
            config.topology.fragments.push_back(FragmentSetting());
            current_fragment = &config.topology.fragments.back();
            current_path = nullptr;
            continue;
        }
        if (line == "[path]") {
            pending_paths.push_back(PendingPath());
            current_fragment = nullptr;
            current_path = &pending_paths.back();
            continue;
        }

        auto equal_pos = line.find('=');
        if (equal_pos == std::string::npos) {
            continue;
        }
        std::string key = trim(line.substr(0, equal_pos));
        std::string value = trim(line.substr(equal_pos + 1));

        if (current_fragment) {
            if (key == "name") {
                current_fragment->name = value;
                fragment_index[value] =
                    static_cast<int>(config.topology.fragments.size() - 1);
            } else if (key == "kind") {
                current_fragment->kind = parse_sketch_kind(value);
            } else if (key == "memory") {
                current_fragment->memory_bytes =
                    static_cast<uint64_t>(std::stoull(value));
            } else if (key == "depth") {
                current_fragment->depth =
                    static_cast<uint32_t>(std::stoul(value));
            } else if (key == "initial_subepoch") {
                current_fragment->initial_subepoch =
                    static_cast<uint32_t>(std::stoul(value));
            } else if (key == "max_subepoch") {
                current_fragment->max_subepoch =
                    static_cast<uint32_t>(std::stoul(value));
            } else if (key == "rho_target") {
                current_fragment->rho_target = std::stod(value);
            } else if (key == "boost_single_hop") {
                current_fragment->boost_single_hop = to_bool(value);
            } else if (key == "background_ratio") {
                current_fragment->background_ratio = std::stod(value);
            }
            continue;
        }

        if (current_path) {
            if (key == "name") {
                current_path->name = value;
            } else if (key == "nodes") {
                std::stringstream ss(value);
                std::string token;
                current_path->node_names.clear();
                while (std::getline(ss, token, ',')) {
                    current_path->node_names.push_back(trim(token));
                }
            }
            continue;
        }

        if (key == "pcap") {
            config.pcap_path = value;
        } else if (key == "sketch_kind") {
            config.sketch_kind = parse_sketch_kind(value);
        } else if (key == "epoch_ns") {
            config.epoch_duration_ns =
                static_cast<uint64_t>(std::stoull(value));
        } else if (key == "max_epochs") {
            config.max_epochs = static_cast<uint32_t>(std::stoul(value));
        } else if (key == "aggregated_depth") {
            config.aggregated_depth =
                static_cast<uint32_t>(std::stoul(value));
        } else if (key == "heavy_ratio") {
            config.heavy_hitter_ratio = std::stod(value);
        } else if (key == "max_flow_report") {
            config.max_flow_report = static_cast<uint32_t>(std::stoul(value));
        }
    }

    for (auto& frag : config.topology.fragments) {
        if (frag.depth == 0) {
            frag.depth = 1;
        }
        if (frag.max_subepoch < frag.initial_subepoch) {
            frag.max_subepoch = frag.initial_subepoch;
        }
        if (frag.memory_bytes == 0) {
            frag.memory_bytes = 64 * 1024;
        }
    }

    for (const auto& pending : pending_paths) {
        PathSetting path_setting;
        path_setting.name = pending.name;
        for (const auto& node_name : pending.node_names) {
            auto it = fragment_index.find(node_name);
            if (it == fragment_index.end()) {
                std::cerr << "路径节点未定义: " << node_name << std::endl;
                return false;
            }
            path_setting.node_indices.push_back(it->second);
        }
        config.topology.paths.push_back(path_setting);
    }

    if (config.topology.fragments.empty() || config.topology.paths.empty()) {
        std::cerr << "配置缺少片段或路径定义" << std::endl;
        return false;
    }
    return true;
}

void print_report(const SimulationReport& report) {
    std::cout << "================ 仿真结果汇总 ================\n";
    for (const auto& epoch : report.epochs) {
        std::cout << "Epoch " << epoch.epoch_id
                  << " | 平均rho: " << epoch.rho_average << std::endl;
        for (const auto& metric : epoch.flow_metrics) {
            std::cout << "  Flow(" << metric.flow.src_ip << ","
                      << metric.flow.dst_ip << ")"
                      << " GT=" << metric.ground_truth
                      << " Agg=" << metric.aggregated
                      << " DISCO=" << metric.disco
                      << " DiSketch=" << metric.disketch << std::endl;
        }
        std::cout << "----------------------------------------------\n";
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::string config_path = "../configs/disketch.ini";
    if (argc > 1) {
        config_path = argv[1];
    }

    SimulationConfig config;
    if (!load_config(config_path, config)) {
        return 1;
    }

    PacketParser parser;
    PacketParser::PacketVector packets;
    try {
        packets = parser.parse_pcap(config.pcap_path);
    } catch (const std::exception& ex) {
        std::cerr << "解析PCAP失败: " << ex.what() << std::endl;
        return 1;
    }
    if (packets.empty()) {
        std::cerr << "没有可用数据包，无法继续仿真" << std::endl;
        return 1;
    }

    DisaggregationSimulation simulation(config);
    SimulationReport report = simulation.run(packets);
    print_report(report);

    return 0;
}
