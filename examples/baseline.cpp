#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

#include "CountMin.h"
#include "CountSketch.h"
#include "HeavyHitterDetector.h"
#include "Ideal.h"
#include "PacketParser.h"
#include "UnivMon.h"

using namespace std;

struct BenchmarkResult {
    string name;
    HeavyHitterDetector detector;
    double time_ms;
};

int main() {
    // 配置参数
    const char* pcap_file = "../datasets/caida_600w.pcap";

    // 内存配置：64KB, 128KB, 256KB, 512KB, 1MB, 2MB, 4MB, 8MB
    vector<uint64_t> memory_sizes = {
        64 * 1024,        // 64KB
        128 * 1024,       // 128KB
        256 * 1024,       // 256KB
        512 * 1024,       // 512KB
        1024 * 1024,      // 1MB
        2 * 1024 * 1024,  // 2MB
        4 * 1024 * 1024,  // 4MB
        8 * 1024 * 1024   // 8MB
    };

    vector<string> memory_labels = {"64KB", "128KB", "256KB", "512KB",
                                    "1MB",  "2MB",   "4MB",   "8MB"};

    // 收集所有测试结果
    vector<BenchmarkResult> results;

    cout << "DiSketch Baseline Benchmark" << endl;
    cout << "Testing with: " << pcap_file << endl;
    cout << "Memory configurations: 64KB to 8MB" << endl;

    // 解析 PCAP 文件
    cout << "\n" << string(70, '=') << endl;
    cout << "Step 1: Parsing PCAP file..." << endl;
    cout << string(70, '=') << endl;

    PacketParser parser;
    vector<PacketRecord> packets;

    try {
        packets = parser.parse_pcap(pcap_file);
    } catch (const exception& e) {
        cerr << "Failed to parse PCAP file: " << e.what() << endl;
        return 1;
    }

    cout << "Parsed " << packets.size() << " packets" << endl;

    // 使用 Ideal 建立 Ground Truth
    cout << "\n" << string(70, '=') << endl;
    cout << "Step 2: Building ground truth with Ideal..." << endl;
    cout << string(70, '=') << endl;

    Ideal ideal;
    for (const auto& pkt : packets) {
        ideal.update(pkt.flow, 1);
    }

    cout << "Ideal: Total unique flows = " << ideal.get_flow_count() << endl;

    // 定义重流阈值（Heavy Hitter Threshold）
    // 使用包总数的 0.01% 作为阈值
    uint64_t heavy_hitter_threshold =
        static_cast<uint64_t>(packets.size() * 0.0001);
    cout << "Heavy Hitter Threshold: " << heavy_hitter_threshold << " packets"
         << endl;

    // 对每个内存配置进行测试
    for (size_t i = 0; i < memory_sizes.size(); i++) {
        uint64_t memory = memory_sizes[i];
        string mem_label = memory_labels[i];

        cout << "\n" << string(70, '=') << endl;
        cout << "Testing with memory: " << mem_label << endl;
        cout << string(70, '=') << endl;

        // 创建所有 Sketch 实例
        CountMin cm(8, memory);
        CountSketch cs(8, memory);
        UnivMon um_sah(6, memory, nullptr, UnivMonBackend::SaH);
        UnivMon um_cs(6, memory, nullptr, UnivMonBackend::CountSketch);

        // 分别为每个 Sketch 更新、计时并检测重流
        cout << "Updating sketches..." << endl;

        auto start_cm = chrono::high_resolution_clock::now();
        for (const auto& pkt : packets) {
            cm.update(pkt.flow, 1);
        }
        auto end_cm = chrono::high_resolution_clock::now();
        double time_cm =
            chrono::duration<double, milli>(end_cm - start_cm).count();
        HeavyHitterDetector detector_cm;
        detector_cm.detect(ideal, cm, heavy_hitter_threshold);
        results.push_back({"CM-" + mem_label, detector_cm, time_cm});

        auto start_cs = chrono::high_resolution_clock::now();
        for (const auto& pkt : packets) {
            cs.update(pkt.flow, 1);
        }
        auto end_cs = chrono::high_resolution_clock::now();
        double time_cs =
            chrono::duration<double, milli>(end_cs - start_cs).count();
        HeavyHitterDetector detector_cs;
        detector_cs.detect(ideal, cs, heavy_hitter_threshold);
        results.push_back({"CS-" + mem_label, detector_cs, time_cs});

        // SaH 计算速度很慢， 1 MB是可以接受的最大内存
        if (i < memory_sizes.size() - 3) {
            auto start_um_sah = chrono::high_resolution_clock::now();
            for (const auto& pkt : packets) {
                um_sah.update(pkt.flow, 1);
            }
            auto end_um_sah = chrono::high_resolution_clock::now();
            double time_um_sah =
                chrono::duration<double, milli>(end_um_sah - start_um_sah)
                    .count();
            HeavyHitterDetector detector_um_sah;
            detector_um_sah.detect(ideal, um_sah, heavy_hitter_threshold);
            results.push_back(
                {"UM-SaH-" + mem_label, detector_um_sah, time_um_sah});
        }

        auto start_um_cs = chrono::high_resolution_clock::now();
        for (const auto& pkt : packets) {
            um_cs.update(pkt.flow, 1);
        }
        auto end_um_cs = chrono::high_resolution_clock::now();
        double time_um_cs =
            chrono::duration<double, milli>(end_um_cs - start_um_cs).count();
        HeavyHitterDetector detector_um_cs;
        detector_um_cs.detect(ideal, um_cs, heavy_hitter_threshold);
        results.push_back({"UM-CS-" + mem_label, detector_um_cs, time_um_cs});
    }

    // ========== 打印汇总表格 ==========
    cout << "\n" << string(70, '=') << endl;
    cout << "Benchmark completed!" << endl;
    cout << string(70, '=') << endl;

    cout << "\n" << string(120, '=') << endl;
    cout << "SUMMARY TABLE" << endl;
    cout << string(120, '=') << endl;
    cout << left << setw(20) << "Sketch" << right << setw(12) << "Time(ms)"
         << setw(12) << "Precision" << setw(12) << "Accuracy" << setw(10)
         << "Recall" << setw(10) << "F1" << setw(8) << "TP" << setw(8) << "FP"
         << setw(8) << "FN" << setw(8) << "TN" << endl;
    cout << string(120, '-') << endl;

    for (const auto& result : results) {
        cout << left << setw(20) << result.name << right << fixed
             << setprecision(2) << setw(12) << result.time_ms << setw(11)
             << result.detector.precision() * 100 << "%" << setw(11)
             << result.detector.accuracy() * 100 << "%" << setw(9)
             << result.detector.recall() * 100 << "%" << setprecision(4)
             << setw(10) << result.detector.f1_score() << setw(8)
             << result.detector.tp << setw(8) << result.detector.fp << setw(8)
             << result.detector.fn << setw(8) << result.detector.tn << endl;
    }
    cout << string(120, '=') << endl;

    return 0;
}
