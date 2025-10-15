#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

#include "ConfigParser.h"
#include "DiSketch.h"
#include "PacketParser.h"

namespace {

std::string sketch_kind_to_string(SketchKind kind) {
    switch (kind) {
        case SketchKind::CountMin:
            return "CountMin";
        case SketchKind::CountSketch:
            return "CountSketch";
        case SketchKind::UnivMon:
            return "UnivMon";
        default:
            return "Unknown";
    }
}

void print_config_info(const DiSketchConfig& config) {
    std::cout << "\n========== 配置信息 ==========\n";

    // Full Sketch 配置
    uint64_t full_sketch_memory = 0;
    for (const auto& frag : config.topology.fragments) {
        full_sketch_memory += frag.memory_bytes;
    }

    std::cout << "[Full Sketch]\n";
    std::cout << "  类型: " << sketch_kind_to_string(config.sketch_kind)
              << "\n";
    std::cout << "  层数: " << config.full_sketch_depth << "\n";
    std::cout << "  内存: " << full_sketch_memory << " bytes ("
              << (full_sketch_memory / 1024) << " KB)\n";

    // DiSketch 配置
    std::cout << "\n[DiSketch]\n";
    std::cout << "  Fragment 数量: " << config.topology.fragments.size()
              << "\n";
    for (size_t i = 0; i < config.topology.fragments.size(); ++i) {
        const auto& frag = config.topology.fragments[i];
        std::cout << "  Fragment[" << i << "] " << frag.name << ":\n";
        std::cout << "    类型: " << sketch_kind_to_string(frag.kind) << "\n";
        std::cout << "    内存: " << frag.memory_bytes << " bytes ("
                  << (frag.memory_bytes / 1024) << " KB)\n";
        std::cout << "    层数: " << frag.depth << "\n";
        std::cout << "    初始子epoch: " << frag.initial_subepoch << "\n";
        std::cout << "    最大子epoch: " << frag.max_subepoch << "\n";
        std::cout << "    rho目标: " << frag.rho_target << "\n";
    }

    std::cout << "\n[路径配置]\n";
    std::cout << "  路径数量: " << config.topology.paths.size() << "\n";
    for (size_t i = 0; i < config.topology.paths.size(); ++i) {
        const auto& path = config.topology.paths[i];
        std::cout << "  路径[" << i << "] " << path.name << ": [";
        for (size_t j = 0; j < path.node_indices.size(); ++j) {
            if (j > 0)
                std::cout << " -> ";
            int idx = path.node_indices[j];
            std::cout << config.topology.fragments[idx].name;
        }
        std::cout << "]\n";
    }
    std::cout << "==============================\n\n";
}

void print_report(const DiSketchReport& report, const DiSketchConfig& config) {
    std::cout << "================ DiSketch 运行结果 ================\n";
    for (const auto& epoch : report.epochs) {
        std::cout << "\nEpoch " << epoch.epoch_id
                  << " | 平均rho: " << std::fixed << std::setprecision(4)
                  << epoch.rho_average << "\n";
        std::cout << "总包数: " << epoch.total_packets
                  << " | 总流数: " << epoch.total_flows
                  << " | 重流阈值: " << std::fixed << std::setprecision(2)
                  << epoch.heavy_hitter_threshold << " 包"
                  << " | 真实重流数: " << epoch.flow_metrics.size() << "\n";

        // 打印每个 fragment 的子epoch信息
        std::cout << "\n各Fragment子epoch数: ";
        for (size_t i = 0; i < config.topology.fragments.size(); ++i) {
            if (i > 0)
                std::cout << ", ";
            std::cout << config.topology.fragments[i].name << "=";
            if (i < epoch.fragment_subepoch_counts.size()) {
                std::cout << epoch.fragment_subepoch_counts[i];
            } else {
                std::cout << "?";
            }
        }
        std::cout << "\n";

        // 紧凑的表格格式输出检测指标
        std::cout << "\n重流检测指标对比:\n";
        std::cout << "┌──────────────┬────────┬────────┬────────┬────────┬─────"
                     "───┬────────┬────────┬────────┐\n";
        std::cout << "│   方法       │ Prec.  │ Recall │ F1     │ Acc.   │   "
                     "TP   │   FP   │   FN   │   TN   │\n";
        std::cout << "├──────────────┼────────┼────────┼────────┼────────┼─────"
                     "───┼────────┼────────┼────────┤\n";

        // Full Sketch 行
        const auto& full = epoch.full_sketch_detector;
        std::cout << "│ Full Sketch  │ " << std::setw(5) << std::fixed
                  << std::setprecision(1) << (full.precision() * 100) << "% │ "
                  << std::setw(5) << std::fixed << std::setprecision(1)
                  << (full.recall() * 100) << "% │ " << std::setw(6)
                  << std::fixed << std::setprecision(4) << full.f1_score()
                  << " │ " << std::setw(5) << std::fixed << std::setprecision(1)
                  << (full.accuracy() * 100) << "% │ " << std::setw(6)
                  << full.tp << " │ " << std::setw(6) << full.fp << " │ "
                  << std::setw(6) << full.fn << " │ " << std::setw(6) << full.tn
                  << " │\n";

        // DiSketch 行
        const auto& disketch = epoch.disketch_detector;
        std::cout << "│ DiSketch     │ " << std::setw(5) << std::fixed
                  << std::setprecision(1) << (disketch.precision() * 100)
                  << "% │ " << std::setw(5) << std::fixed
                  << std::setprecision(1) << (disketch.recall() * 100) << "% │ "
                  << std::setw(6) << std::fixed << std::setprecision(4)
                  << disketch.f1_score() << " │ " << std::setw(5) << std::fixed
                  << std::setprecision(1) << (disketch.accuracy() * 100)
                  << "% │ " << std::setw(6) << disketch.tp << " │ "
                  << std::setw(6) << disketch.fp << " │ " << std::setw(6)
                  << disketch.fn << " │ " << std::setw(6) << disketch.tn
                  << " │\n";

        std::cout << "└──────────────┴────────┴────────┴────────┴────────┴─────"
                     "───┴────────┴────────┴────────┘\n";
    }
    std::cout << "\n====================================================\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string config_path = "../configs/disketch.ini";
    if (argc > 1) {
        config_path = argv[1];
    }

    ConfigParser config_parser;
    DiSketchConfig config;
    if (!config_parser.parse(config_path, config)) {
        return 1;
    }

    PacketParser packet_parser;
    PacketParser::PacketVector packets;
    try {
        packets = packet_parser.parse_pcap(config.pcap_path);
    } catch (const std::exception& ex) {
        std::cerr << "解析PCAP失败: " << ex.what() << std::endl;
        return 1;
    }
    if (packets.empty()) {
        std::cerr << "没有可用数据包，无法继续" << std::endl;
        return 1;
    }

    std::cout
        << "共" << packets.size() << "个数据包，时间跨度: "
        << (packets.back().timestamp - packets.front().timestamp).count() /
               1000000
        << " 毫秒\n";
    std::cout << "epoch长度: " << config.epoch_duration_ns / 1000000
              << " 毫秒\n";

    // 打印配置信息
    print_config_info(config);

    DiSketch manager(config);
    DiSketchReport report = manager.run(packets);
    std::cout << report.epochs.size() << " 个 epoch 处理完成。\n";
    print_report(report, config);

    return 0;
}
