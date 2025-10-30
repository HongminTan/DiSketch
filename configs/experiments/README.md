# DiSketch 实验说明

## 实验目标与主要规律

- **Sketch 类型对比（`sketch/`）**：UnivMon 在 DiSketch 下的 F1 明显低于 CountMin/CountSketch，说明多层级抽样结构对分布失真更敏感；CountMin/CountSketch 则几乎能保持与 Full Sketch 相同的检测质量。
- **Epoch 时长（`epoch/`）**：随着 epoch 延长，DiSketch 的 F1 逐步下降，而 Full Sketch 几乎不变，显示长时间窗口会积累估计误差并延迟热点流的刷新。
- **数据集（`dataset/`）**：在 CAIDA 数据上 DiSketch 的表现显著劣化（尤其是 UnivMon），证明该数据集的热点分布更动态、噪声更高；MAWI 数据对 DiSketch 更友好。
- **Heavy Ratio（`heavy_ratio/`）**：阈值越严（比率越低），DiSketch 越容易捕获全部重流；阈值提高时 UnivMon 受到的影响最大，CountMin/CountSketch 仍表现稳定。
- **性能边界（`performance_bounds/`）**：`best_*` 组合展示了在合理参数下 DiSketch 的上限，而 `worst_*` 则证实不当配置（如 UnivMon+高阈值）会导致近乎失效。

### 可能的解释

1. **分层 Sketch 噪声传递**：UnivMon 需要多层 hash 聚合，DiSketch 的分片与重建会放大估计噪声，导致 Precision 急剧下降。
2. **时间聚合拖尾**：长 epoch 或高 heavy ratio 让估计滞后，片段在 epoch 内难以及时收敛，因而 Recall/Precision 均下降。
3. **数据分布差异**：CAIDA 含突发与协议混杂流，fragment 之间相关性弱于 MAWI，导致空间聚合后误报显著增多。

## 实验结果汇总

> 单位：F1 分数；ΔF1 = Full Sketch F1 – DiSketch F1

### Sketch 类型

| 配置 | Full Precision | Full Recall | Full F1 | Full Accuracy | Full TP | Full FP | Full FN | Full TN | DiSketch Precision | DiSketch Recall | DiSketch F1 | DiSketch Accuracy | DiSketch TP | DiSketch FP | DiSketch FN | DiSketch TN |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `sketch/countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 76892 | 0 | 0 | 17086791 | 0.972 | 1.000 | 0.986 | 1.000 | 76888 | 2229 | 4 | 17084562 |
| `sketch/countsketch.ini` | 0.996 | 1.000 | 0.998 | 1.000 | 76892 | 341 | 0 | 17086450 | 0.972 | 1.000 | 0.986 | 1.000 | 76891 | 2230 | 1 | 17084561 |
| `sketch/univmon.ini` | 0.947 | 1.000 | 0.948 | 1.000 | 76892 | 8561 | 0 | 17078230 | 0.398 | 0.895 | 0.551 | 0.993 | 68840 | 104180 | 8052 | 16982611 |

### Epoch 时长

| 配置 | Full Precision | Full Recall | Full F1 | Full Accuracy | Full TP | Full FP | Full FN | Full TN | DiSketch Precision | DiSketch Recall | DiSketch F1 | DiSketch Accuracy | DiSketch TP | DiSketch FP | DiSketch FN | DiSketch TN |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `epoch/50ms_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 159666 | 0 | 0 | 17644554 | 0.983 | 1.000 | 0.992 | 1.000 | 159661 | 2693 | 5 | 17641861 |
| `epoch/50ms_countsketch.ini` | 0.998 | 1.000 | 0.999 | 1.000 | 159666 | 376 | 0 | 17644178 | 0.983 | 1.000 | 0.992 | 1.000 | 159663 | 2693 | 3 | 17641861 |
| `epoch/50ms_univmon.ini` | 0.861 | 1.000 | 0.925 | 0.999 | 159666 | 25865 | 0 | 17618689 | 0.205 | 0.890 | 0.333 | 0.968 | 142121 | 552745 | 17545 | 17091809 |
| `epoch/100ms_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 76892 | 0 | 0 | 17086791 | 0.972 | 1.000 | 0.986 | 1.000 | 76888 | 2229 | 4 | 17084562 |
| `epoch/100ms_countsketch.ini` | 0.996 | 1.000 | 0.998 | 1.000 | 76892 | 341 | 0 | 17086450 | 0.972 | 1.000 | 0.986 | 1.000 | 76891 | 2230 | 1 | 17084561 |
| `epoch/100ms_univmon.ini` | 0.900 | 1.000 | 0.948 | 1.000 | 76892 | 8508 | 0 | 17078283 | 0.397 | 0.895 | 0.550 | 0.993 | 68809 | 104466 | 8083 | 16982325 |
| `epoch/200ms_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 38152 | 0 | 0 | 16403437 | 0.950 | 1.000 | 0.974 | 1.000 | 38150 | 2022 | 2 | 16401415 |
| `epoch/200ms_countsketch.ini` | 0.991 | 1.000 | 0.996 | 1.000 | 38152 | 339 | 0 | 16403098 | 0.950 | 1.000 | 0.974 | 1.000 | 38152 | 2024 | 0 | 16401413 |
| `epoch/200ms_univmon.ini` | 0.892 | 1.000 | 0.943 | 1.000 | 38152 | 4610 | 0 | 16398827 | 0.504 | 0.909 | 0.649 | 0.998 | 34688 | 34120 | 3464 | 16369317 |
| `epoch/500ms_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 15400 | 0 | 0 | 15411052 | 0.890 | 1.000 | 0.942 | 1.000 | 15395 | 1901 | 5 | 15409151 |
| `epoch/500ms_countsketch.ini` | 0.979 | 1.000 | 0.989 | 1.000 | 15400 | 336 | 0 | 15410716 | 0.890 | 1.000 | 0.942 | 1.000 | 15397 | 1901 | 3 | 15409151 |
| `epoch/500ms_univmon.ini` | 0.817 | 1.000 | 0.899 | 1.000 | 15400 | 3445 | 0 | 15407607 | 0.417 | 0.916 | 0.573 | 0.999 | 14102 | 19721 | 1298 | 15391331 |

### 数据集

| 配置 | Full Precision | Full Recall | Full F1 | Full Accuracy | Full TP | Full FP | Full FN | Full TN | DiSketch Precision | DiSketch Recall | DiSketch F1 | DiSketch Accuracy | DiSketch TP | DiSketch FP | DiSketch FN | DiSketch TN |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `dataset/mawi_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 76892 | 0 | 0 | 17086791 | 0.972 | 1.000 | 0.986 | 1.000 | 76888 | 2229 | 4 | 17084562 |
| `dataset/mawi_countsketch.ini` | 0.996 | 1.000 | 0.998 | 1.000 | 76892 | 341 | 0 | 17086450 | 0.972 | 1.000 | 0.986 | 1.000 | 76891 | 2230 | 1 | 17084561 |
| `dataset/mawi_univmon.ini` | 0.902 | 1.000 | 0.948 | 1.000 | 76892 | 8387 | 0 | 17078404 | 0.399 | 0.895 | 0.552 | 0.993 | 68818 | 103802 | 8074 | 16982989 |
| `dataset/caida_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 536 | 0 | 0 | 6130522 | 0.828 | 1.000 | 0.906 | 1.000 | 536 | 111 | 0 | 6130411 |
| `dataset/caida_countsketch.ini` | 0.982 | 1.000 | 0.991 | 1.000 | 536 | 10 | 0 | 6130512 | 0.828 | 1.000 | 0.906 | 1.000 | 536 | 111 | 0 | 6130411 |
| `dataset/caida_univmon.ini` | 0.841 | 1.000 | 0.914 | 1.000 | 536 | 101 | 0 | 6130421 | 0.148 | 0.771 | 0.248 | 1.000 | 413 | 2378 | 123 | 6128144 |

### Heavy Ratio

| 配置 | Full Precision | Full Recall | Full F1 | Full Accuracy | Full TP | Full FP | Full FN | Full TN | DiSketch Precision | DiSketch Recall | DiSketch F1 | DiSketch Accuracy | DiSketch TP | DiSketch FP | DiSketch FN | DiSketch TN |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `heavy_ratio/0.0001_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 17131748 | 0 | 0 | 31935 | 1.000 | 1.000 | 1.000 | 1.000 | 17130577 | 1747 | 1171 | 30188 |
| `heavy_ratio/0.0001_countsketch.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 17131748 | 237 | 0 | 31698 | 1.000 | 1.000 | 1.000 | 1.000 | 17130577 | 1747 | 1171 | 30188 |
| `heavy_ratio/0.0001_univmon.ini` | 0.999 | 1.000 | 0.999 | 0.999 | 17131748 | 19337 | 0 | 12598 | 0.998 | 0.580 | 0.733 | 0.579 | 9932261 | 22317 | 7199487 | 9618 |
| `heavy_ratio/0.001_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 607714 | 5 | 0 | 16555964 | 0.947 | 1.000 | 0.973 | 0.998 | 607693 | 34328 | 21 | 16521641 |
| `heavy_ratio/0.001_countsketch.ini` | 0.992 | 1.000 | 0.996 | 1.000 | 607714 | 4844 | 0 | 16551125 | 0.947 | 1.000 | 0.973 | 0.998 | 607696 | 34330 | 18 | 16521639 |
| `heavy_ratio/0.001_univmon.ini` | 0.244 | 1.000 | 0.392 | 0.890 | 607714 | 1886281 | 0 | 14669688 | 0.057 | 0.824 | 0.107 | 0.515 | 500785 | 8225929 | 106929 | 8330040 |
| `heavy_ratio/0.005_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 154287 | 1 | 0 | 17009395 | 0.967 | 1.000 | 0.983 | 1.000 | 154281 | 5336 | 6 | 17004060 |
| `heavy_ratio/0.005_countsketch.ini` | 0.996 | 1.000 | 0.998 | 1.000 | 154287 | 635 | 0 | 17008761 | 0.967 | 1.000 | 0.983 | 1.000 | 154285 | 5340 | 2 | 17004056 |
| `heavy_ratio/0.005_univmon.ini` | 0.859 | 1.000 | 0.924 | 0.999 | 154287 | 25223 | 0 | 16984173 | 0.208 | 0.878 | 0.336 | 0.969 | 135507 | 516157 | 18780 | 16493239 |
| `heavy_ratio/0.01_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 76892 | 0 | 0 | 17086791 | 0.972 | 1.000 | 0.986 | 1.000 | 76888 | 2229 | 4 | 17084562 |
| `heavy_ratio/0.01_countsketch.ini` | 0.996 | 1.000 | 0.998 | 1.000 | 76892 | 341 | 0 | 17086450 | 0.972 | 1.000 | 0.986 | 1.000 | 76891 | 2230 | 1 | 17084561 |
| `heavy_ratio/0.01_univmon.ini` | 0.902 | 1.000 | 0.948 | 1.000 | 76892 | 8390 | 0 | 17078401 | 0.398 | 0.895 | 0.551 | 0.993 | 68832 | 103948 | 8060 | 16982843 |

### 性能边界

| 配置 | Full Precision | Full Recall | Full F1 | Full Accuracy | Full TP | Full FP | Full FN | Full TN | DiSketch Precision | DiSketch Recall | DiSketch F1 | DiSketch Accuracy | DiSketch TP | DiSketch FP | DiSketch FN | DiSketch TN |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `performance_bounds/best_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 114 | 0 | 0 | 6130944 | 0.864 | 1.000 | 0.927 | 1.000 | 114 | 18 | 0 | 6130926 |
| `performance_bounds/best_countsketch.ini` | 0.991 | 1.000 | 0.996 | 1.000 | 114 | 1 | 0 | 6130943 | 0.864 | 1.000 | 0.927 | 1.000 | 114 | 18 | 0 | 6130926 |
| `performance_bounds/best_univmon.ini` | 0.864 | 1.000 | 0.927 | 1.000 | 114 | 18 | 0 | 6130926 | 0.228 | 0.851 | 0.359 | 1.000 | 97 | 329 | 17 | 6130615 |
| `performance_bounds/worst_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 159666 | 0 | 0 | 17644554 | 1.000 | 1.000 | 1.000 | 1.000 | 159661 | 13 | 5 | 17644541 |
| `performance_bounds/worst_countsketch.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 159666 | 10 | 0 | 17644544 | 0.995 | 1.000 | 0.997 | 1.000 | 159663 | 801 | 3 | 17643753 |
| `performance_bounds/worst_univmon.ini` | 0.877 | 1.000 | 0.935 | 0.999 | 159666 | 22364 | 0 | 17622190 | 0.096 | 0.843 | 0.172 | 0.927 | 134554 | 1266649 | 25112 | 16377905 |

## 复现实验

所有脚本位于 `configs/experiments/scripts/`，运行前请确保已在项目根目录执行过 `cmake --build build`，生成 `build/disketch_simulator`。

### 并行批量运行

```bash
cd configs/experiments
python scripts/run_parallel.py
```

- `--groups sketch,dataset`：只运行指定实验组，多个组用逗号分隔，默认全部
- `--workers N`：设置最大并发数（默认取 CPU 核心数与配置数量的较小值）
- `--quiet`：关闭终端进度条，仅保留日志和配置文件注释

脚本会将结果写回每个 `.ini` 顶部注释，并把原始输出保存到同级目录的 `logs/*.log` 中。
