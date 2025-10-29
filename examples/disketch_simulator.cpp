#include <iomanip>
#include <iostream>
#include <string>

#include "ConfigParser.h"
#include "DiSketch.h"
#include "PacketParser.h"
#include "cxxopts.hpp"

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
    // 累加所有 epoch 的统计数据
    HeavyHitterDetector total_full_sketch;
    HeavyHitterDetector total_disketch;

    for (const auto& epoch : report.epochs) {
        // 累加统计数据
        total_full_sketch.tp += epoch.full_sketch_detector.tp;
        total_full_sketch.fp += epoch.full_sketch_detector.fp;
        total_full_sketch.fn += epoch.full_sketch_detector.fn;
        total_full_sketch.tn += epoch.full_sketch_detector.tn;

        total_disketch.tp += epoch.disketch_detector.tp;
        total_disketch.fp += epoch.disketch_detector.fp;
        total_disketch.fn += epoch.disketch_detector.fn;
        total_disketch.tn += epoch.disketch_detector.tn;
    }

    // 打印所有 epoch 的总体结果
    std::cout << "================ 所有Epoch总体结果 ================\n";
    std::cout << "总Epoch数: " << report.epochs.size() << "\n\n";
    std::cout << "累计重流检测指标:\n";
    std::cout << "┌──────────────┬────────┬────────┬────────┬────────┬─────"
                 "───┬────────┬────────┬────────┐\n";
    std::cout << "│   方法       │ Prec.  │ Recall │ F1     │ Acc.   │   "
                 "TP   │   FP   │   FN   │   TN   │\n";
    std::cout << "├──────────────┼────────┼────────┼────────┼────────┼─────"
                 "───┼────────┼────────┼────────┤\n";

    // 总体 Full Sketch 行
    std::cout << "│ Full Sketch  │ " << std::setw(5) << std::fixed
              << std::setprecision(1) << (total_full_sketch.precision() * 100)
              << "% │ " << std::setw(5) << std::fixed << std::setprecision(1)
              << (total_full_sketch.recall() * 100) << "% │ " << std::setw(6)
              << std::fixed << std::setprecision(4)
              << total_full_sketch.f1_score() << " │ " << std::setw(5)
              << std::fixed << std::setprecision(1)
              << (total_full_sketch.accuracy() * 100) << "% │ " << std::setw(6)
              << total_full_sketch.tp << " │ " << std::setw(6)
              << total_full_sketch.fp << " │ " << std::setw(6)
              << total_full_sketch.fn << " │ " << std::setw(6)
              << total_full_sketch.tn << " │\n";

    // 总体 DiSketch 行
    std::cout << "│ DiSketch     │ " << std::setw(5) << std::fixed
              << std::setprecision(1) << (total_disketch.precision() * 100)
              << "% │ " << std::setw(5) << std::fixed << std::setprecision(1)
              << (total_disketch.recall() * 100) << "% │ " << std::setw(6)
              << std::fixed << std::setprecision(4) << total_disketch.f1_score()
              << " │ " << std::setw(5) << std::fixed << std::setprecision(1)
              << (total_disketch.accuracy() * 100) << "% │ " << std::setw(6)
              << total_disketch.tp << " │ " << std::setw(6) << total_disketch.fp
              << " │ " << std::setw(6) << total_disketch.fn << " │ "
              << std::setw(6) << total_disketch.tn << " │\n";

    std::cout << "└──────────────┴────────┴────────┴────────┴────────┴─────"
                 "───┴────────┴────────┴────────┘\n";
}

}  // namespace

int main(int argc, char** argv) {
    // 使用 cxxopts 解析命令行参数
    cxxopts::Options options("disketch_simulation",
                             "DiSketch 网络测量仿真程序");

    options.add_options()("c,config", "配置文件路径",
                          cxxopts::value<std::string>()->default_value(
                              "../configs/disketch.ini"))(
        "q,quiet", "静默模式（仅输出CSV格式结果）",
        cxxopts::value<bool>()->default_value("false"))("h,help",
                                                        "显示帮助信息");

    cxxopts::ParseResult result;
    try {
        result = options.parse(argc, argv);
    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "参数解析错误: " << e.what() << std::endl;
        std::cerr << options.help() << std::endl;
        return 1;
    }

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    std::string config_path = result["config"].as<std::string>();
    bool quiet_mode = result["quiet"].as<bool>();

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

    // 打印配置信息（非静默模式）
    if (!quiet_mode) {
        print_config_info(config);
    }

    DiSketch manager(config);
    DiSketchReport report = manager.run(packets);

    if (quiet_mode) {
        // 静默模式：只输出CSV格式的结果
        // 累加所有 epoch 的统计数据
        HeavyHitterDetector total_full_sketch;
        HeavyHitterDetector total_disketch;

        for (const auto& epoch : report.epochs) {
            total_full_sketch.tp += epoch.full_sketch_detector.tp;
            total_full_sketch.fp += epoch.full_sketch_detector.fp;
            total_full_sketch.fn += epoch.full_sketch_detector.fn;
            total_full_sketch.tn += epoch.full_sketch_detector.tn;

            total_disketch.tp += epoch.disketch_detector.tp;
            total_disketch.fp += epoch.disketch_detector.fp;
            total_disketch.fn += epoch.disketch_detector.fn;
            total_disketch.tn += epoch.disketch_detector.tn;
        }

        // 输出CSV格式：method,precision,recall,f1,accuracy,tp,fp,fn,tn
        std::cout << "FullSketch," << std::fixed << std::setprecision(6)
                  << total_full_sketch.precision() << ","
                  << total_full_sketch.recall() << ","
                  << total_full_sketch.f1_score() << ","
                  << total_full_sketch.accuracy() << "," << total_full_sketch.tp
                  << "," << total_full_sketch.fp << "," << total_full_sketch.fn
                  << "," << total_full_sketch.tn << "\n";

        std::cout << "DiSketch," << std::fixed << std::setprecision(6)
                  << total_disketch.precision() << ","
                  << total_disketch.recall() << "," << total_disketch.f1_score()
                  << "," << total_disketch.accuracy() << ","
                  << total_disketch.tp << "," << total_disketch.fp << ","
                  << total_disketch.fn << "," << total_disketch.tn << "\n";
    } else {
        // 正常模式：打印格式化的报告
        print_report(report, config);
    }

    return 0;
}
