#include "HeavyHitterDetector.h"

void HeavyHitterDetector::detect(Ideal& ideal,
                                 Sketch& sketch,
                                 uint64_t threshold) {
    // 重置计数器
    reset();

    auto ideal_data = ideal.get_raw_data();

    // 找出所有真实的重流（Ground Truth）
    std::unordered_set<TwoTuple, TwoTupleHash> real_heavy_hitters;
    for (const auto& item : ideal_data) {
        if (item.second >= threshold) {
            real_heavy_hitters.insert(item.first);
        }
    }

    // 对每个流进行分类
    for (const auto& item : ideal_data) {
        const TwoTuple& flow = item.first;
        uint64_t ideal_count = item.second;
        uint64_t sketch_count = sketch.query(flow);

        bool is_real_hh = ideal_count >= threshold;
        bool is_pred_hh = sketch_count >= threshold;

        if (is_real_hh && is_pred_hh) {
            tp++;  // 真阳性：实际是重流，预测也是重流
        } else if (!is_real_hh && is_pred_hh) {
            fp++;  // 假阳性：实际不是重流，预测是重流
        } else if (is_real_hh && !is_pred_hh) {
            fn++;  // 假阴性：实际是重流，预测不是重流
        } else {
            tn++;  // 真阴性：实际不是重流，预测也不是重流
        }
    }
}

double HeavyHitterDetector::accuracy() const {
    int total = tp + tn + fp + fn;
    return total > 0 ? static_cast<double>(tp + tn) / total : 0.0;
}

double HeavyHitterDetector::precision() const {
    int predicted_positive = tp + fp;
    return predicted_positive > 0 ? static_cast<double>(tp) / predicted_positive
                                  : 0.0;
}

double HeavyHitterDetector::recall() const {
    int actual_positive = tp + fn;
    return actual_positive > 0 ? static_cast<double>(tp) / actual_positive
                               : 0.0;
}

double HeavyHitterDetector::f1_score() const {
    double prec = precision();
    double rec = recall();
    return (prec + rec) > 0 ? 2 * prec * rec / (prec + rec) : 0.0;
}

double HeavyHitterDetector::fpr() const {
    int actual_negative = fp + tn;
    return actual_negative > 0 ? static_cast<double>(fp) / actual_negative
                               : 0.0;
}

double HeavyHitterDetector::fnr() const {
    int actual_positive = tp + fn;
    return actual_positive > 0 ? static_cast<double>(fn) / actual_positive
                               : 0.0;
}

void HeavyHitterDetector::print_metrics(const std::string& sketch_name) const {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << sketch_name << " - Heavy Hitter Detection Metrics"
              << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Precision:             " << precision() * 100 << "%"
              << std::endl;
    std::cout << "Accuracy:              " << accuracy() * 100 << "%"
              << std::endl;
    std::cout << "Recall:                " << recall() * 100 << "%"
              << std::endl;
    std::cout << "F1 Score:              " << f1_score() << std::endl;
    std::cout << "TP (True Positive):    " << tp << std::endl;
    std::cout << "FP (False Positive):   " << fp << std::endl;
    std::cout << "FN (False Negative):   " << fn << std::endl;
    std::cout << "TN (True Negative):    " << tn << std::endl;
}

void HeavyHitterDetector::reset() {
    tp = tn = fp = fn = 0;
}
