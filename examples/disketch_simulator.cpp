#include <iomanip>
#include <iostream>
#include <string>

#include "ConfigParser.h"
#include "DiSketch.h"
#include "PacketParser.h"
#include "cxxopts.hpp"

namespace {

void accumulate_totals(const DiSketchReport& report,
                       HeavyHitterDetector& full_sketch,
                       HeavyHitterDetector& disketch) {
    for (const auto& epoch : report.epochs) {
        full_sketch.tp += epoch.full_sketch_detector.tp;
        full_sketch.fp += epoch.full_sketch_detector.fp;
        full_sketch.fn += epoch.full_sketch_detector.fn;
        full_sketch.tn += epoch.full_sketch_detector.tn;

        disketch.tp += epoch.disketch_detector.tp;
        disketch.fp += epoch.disketch_detector.fp;
        disketch.fn += epoch.disketch_detector.fn;
        disketch.tn += epoch.disketch_detector.tn;
    }
}

void emit_metrics_line(const std::string& method,
                       const HeavyHitterDetector& detector) {
    std::cout << method << ',' << std::fixed << std::setprecision(6)
              << detector.precision() << ',' << detector.recall() << ','
              << detector.f1_score() << ',' << detector.accuracy() << ','
              << detector.tp << ',' << detector.fp << ',' << detector.fn << ','
              << detector.tn << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    cxxopts::Options options("disketch_simulation", "DiSketch 网络测量仿真程序");

    options.add_options()
        ("c,config", "配置文件路径",
         cxxopts::value<std::string>()->default_value("../configs/disketch.ini"))
        ("q,quiet", "静默模式（仅输出结果行）",
         cxxopts::value<bool>()->default_value("false"))
        ("h,help", "显示帮助信息");

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

    if (quiet_mode) {
        config.enable_progress_bar = false;
    }

    DiSketch manager(config);
    DiSketchReport report = manager.run(packets);

        HeavyHitterDetector total_full_sketch;
        HeavyHitterDetector total_disketch;
    accumulate_totals(report, total_full_sketch, total_disketch);

    if (!quiet_mode) {
        std::cout << "method,precision,recall,f1,accuracy,tp,fp,fn,tn\n";
    }

    emit_metrics_line("FullSketch", total_full_sketch);
    emit_metrics_line("DiSketch", total_disketch);

    return 0;
}

