#ifndef HEAVY_HITTER_DETECTOR_H
#define HEAVY_HITTER_DETECTOR_H

#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "Ideal.h"
#include "Sketch.h"
#include "TwoTuple.h"

class HeavyHitterDetector {
   public:
    // 分类指标
    int tp;  // True Positive
    int tn;  // True Negative
    int fp;  // False Positive
    int fn;  // False Negative

    HeavyHitterDetector() : tp(0), tn(0), fp(0), fn(0) {}

    // 检测重流并计算分类指标
    void detect(Ideal& ideal, Sketch& sketch, uint64_t threshold);

    // 计算准确率 (Accuracy)
    double accuracy() const;

    // 计算精确率 (Precision)
    double precision() const;

    // 计算召回率 (Recall)
    double recall() const;

    // 计算 F1 分数
    double f1_score() const;

    // 计算假阳性率 (False Positive Rate)
    double fpr() const;

    // 计算假阴性率 (False Negative Rate)
    double fnr() const;

    // 打印所有指标
    void print_metrics(const std::string& sketch_name) const;

    // 重置所有计数
    void reset();
};

#endif  // HEAVY_HITTER_DETECTOR_H
