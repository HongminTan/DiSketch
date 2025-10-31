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
| `sketch/countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 76892 | 2 | 0 | 17086789 | 0.974 | 1.000 | 0.987 | 1.000 | 76889 | 2090 | 3 | 17084701 |
| `sketch/countsketch.ini` | 0.995 | 1.000 | 0.998 | 1.000 | 76892 | 371 | 0 | 17086420 | 0.974 | 1.000 | 0.987 | 1.000 | 76892 | 2090 | 0 | 17084701 |
| `sketch/univmon.ini` | 0.899 | 1.000 | 0.947 | 0.999 | 76892 | 8614 | 0 | 17078177 | 0.401 | 0.902 | 0.555 | 0.994 | 69367 | 103562 | 7525 | 16983229 |

### Epoch 时长

| 配置 | Full Precision | Full Recall | Full F1 | Full Accuracy | Full TP | Full FP | Full FN | Full TN | DiSketch Precision | DiSketch Recall | DiSketch F1 | DiSketch Accuracy | DiSketch TP | DiSketch FP | DiSketch FN | DiSketch TN |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `epoch/50ms_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 159666 | 2 | 0 | 17644552 | 0.986 | 1.000 | 0.993 | 1.000 | 159662 | 2246 | 4 | 17642308 |
| `epoch/50ms_countsketch.ini` | 0.998 | 1.000 | 0.999 | 1.000 | 159666 | 377 | 0 | 17644177 | 0.986 | 1.000 | 0.993 | 1.000 | 159664 | 2246 | 2 | 17642308 |
| `epoch/50ms_univmon.ini` | 0.860 | 1.000 | 0.925 | 0.999 | 159666 | 25911 | 0 | 17618643 | 0.205 | 0.896 | 0.334 | 0.968 | 143126 | 553686 | 16540 | 17090868 |
| `epoch/100ms_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 76892 | 2 | 0 | 17086789 | 0.974 | 1.000 | 0.987 | 1.000 | 76889 | 2090 | 3 | 17084701 |
| `epoch/100ms_countsketch.ini` | 0.995 | 1.000 | 0.998 | 1.000 | 76892 | 371 | 0 | 17086420 | 0.974 | 1.000 | 0.987 | 1.000 | 76892 | 2090 | 0 | 17084701 |
| `epoch/100ms_univmon.ini` | 0.897 | 1.000 | 0.946 | 0.999 | 76892 | 8797 | 0 | 17077994 | 0.400 | 0.902 | 0.555 | 0.994 | 69355 | 103840 | 7537 | 16982951 |
| `epoch/200ms_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 38152 | 1 | 0 | 16403436 | 0.951 | 1.000 | 0.975 | 1.000 | 38148 | 1946 | 4 | 16401491 |
| `epoch/200ms_countsketch.ini` | 0.991 | 1.000 | 0.995 | 1.000 | 38152 | 357 | 0 | 16403080 | 0.951 | 1.000 | 0.975 | 1.000 | 38151 | 1946 | 1 | 16401491 |
| `epoch/200ms_univmon.ini` | 0.889 | 1.000 | 0.941 | 1.000 | 38152 | 4772 | 0 | 16398665 | 0.509 | 0.913 | 0.654 | 0.998 | 34818 | 33531 | 3334 | 16369906 |
| `epoch/500ms_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 15400 | 1 | 0 | 15411051 | 0.891 | 1.000 | 0.942 | 1.000 | 15395 | 1886 | 5 | 15409166 |
| `epoch/500ms_countsketch.ini` | 0.978 | 1.000 | 0.989 | 1.000 | 15400 | 342 | 0 | 15410710 | 0.891 | 1.000 | 0.942 | 1.000 | 15397 | 1887 | 3 | 15409165 |
| `epoch/500ms_univmon.ini` | 0.815 | 1.000 | 0.898 | 1.000 | 15400 | 3499 | 0 | 15407553 | 0.418 | 0.919 | 0.574 | 0.999 | 14150 | 19713 | 1250 | 15391339 |

### 数据集

| 配置 | Full Precision | Full Recall | Full F1 | Full Accuracy | Full TP | Full FP | Full FN | Full TN | DiSketch Precision | DiSketch Recall | DiSketch F1 | DiSketch Accuracy | DiSketch TP | DiSketch FP | DiSketch FN | DiSketch TN |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `dataset/mawi_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 76892 | 2 | 0 | 17086789 | 0.974 | 1.000 | 0.987 | 1.000 | 76889 | 2090 | 3 | 17084701 |
| `dataset/mawi_countsketch.ini` | 0.995 | 1.000 | 0.998 | 1.000 | 76892 | 371 | 0 | 17086420 | 0.974 | 1.000 | 0.987 | 1.000 | 76892 | 2090 | 0 | 17084701 |
| `dataset/mawi_univmon.ini` | 0.897 | 1.000 | 0.946 | 0.999 | 76892 | 8856 | 0 | 17077935 | 0.401 | 0.902 | 0.555 | 0.994 | 69332 | 103504 | 7560 | 16983287 |
| `dataset/caida_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 536 | 0 | 0 | 6130522 | 0.833 | 0.998 | 0.908 | 1.000 | 535 | 107 | 1 | 6130415 |
| `dataset/caida_countsketch.ini` | 0.966 | 1.000 | 0.983 | 1.000 | 536 | 19 | 0 | 6130503 | 0.826 | 1.000 | 0.905 | 1.000 | 536 | 113 | 0 | 6130409 |
| `dataset/caida_univmon.ini` | 0.804 | 1.000 | 0.891 | 1.000 | 536 | 131 | 0 | 6130391 | 0.144 | 0.728 | 0.241 | 1.000 | 390 | 2317 | 146 | 6128205 |

### Heavy Ratio

| 配置 | Full Precision | Full Recall | Full F1 | Full Accuracy | Full TP | Full FP | Full FN | Full TN | DiSketch Precision | DiSketch Recall | DiSketch F1 | DiSketch Accuracy | DiSketch TP | DiSketch FP | DiSketch FN | DiSketch TN |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `heavy_ratio/0.0001_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 17131748 | 0 | 0 | 31935 | 1.000 | 1.000 | 1.000 | 1.000 | 17130359 | 1517 | 1389 | 30418 |
| `heavy_ratio/0.0001_countsketch.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 17131748 | 223 | 0 | 31712 | 1.000 | 1.000 | 1.000 | 1.000 | 17130359 | 1517 | 1389 | 30418 |
| `heavy_ratio/0.0001_univmon.ini` | 0.999 | 1.000 | 0.999 | 0.999 | 17131748 | 18591 | 0 | 13344 | 0.998 | 0.577 | 0.731 | 0.576 | 9884209 | 21541 | 7247539 | 10394 |
| `heavy_ratio/0.001_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 607714 | 17 | 0 | 16555952 | 0.947 | 1.000 | 0.973 | 0.998 | 607691 | 34083 | 23 | 16521886 |
| `heavy_ratio/0.001_countsketch.ini` | 0.992 | 1.000 | 0.996 | 1.000 | 607714 | 5044 | 0 | 16550925 | 0.947 | 1.000 | 0.973 | 0.998 | 607691 | 34083 | 23 | 16521886 |
| `heavy_ratio/0.001_univmon.ini` | 0.244 | 1.000 | 0.392 | 0.890 | 607714 | 1884311 | 0 | 14671658 | 0.058 | 0.825 | 0.108 | 0.518 | 501431 | 8161088 | 106283 | 8394881 |
| `heavy_ratio/0.005_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 154287 | 3 | 0 | 17009393 | 0.972 | 1.000 | 0.986 | 1.000 | 154282 | 4410 | 5 | 17004986 |
| `heavy_ratio/0.005_countsketch.ini` | 0.996 | 1.000 | 0.998 | 1.000 | 154287 | 684 | 0 | 17008712 | 0.972 | 1.000 | 0.986 | 1.000 | 154283 | 4414 | 4 | 17004982 |
| `heavy_ratio/0.005_univmon.ini` | 0.857 | 1.000 | 0.923 | 0.999 | 154287 | 25688 | 0 | 16983708 | 0.209 | 0.885 | 0.339 | 0.969 | 136551 | 515781 | 17736 | 16493615 |
| `heavy_ratio/0.01_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 76892 | 2 | 0 | 17086789 | 0.974 | 1.000 | 0.987 | 1.000 | 76889 | 2090 | 3 | 17084701 |
| `heavy_ratio/0.01_countsketch.ini` | 0.995 | 1.000 | 0.998 | 1.000 | 76892 | 371 | 0 | 17086420 | 0.974 | 1.000 | 0.987 | 1.000 | 76892 | 2090 | 0 | 17084701 |
| `heavy_ratio/0.01_univmon.ini` | 0.899 | 1.000 | 0.947 | 0.999 | 76892 | 8671 | 0 | 17078120 | 0.400 | 0.902 | 0.555 | 0.994 | 69351 | 103836 | 7541 | 16982955 |

### 性能边界

| 配置 | Full Precision | Full Recall | Full F1 | Full Accuracy | Full TP | Full FP | Full FN | Full TN | DiSketch Precision | DiSketch Recall | DiSketch F1 | DiSketch Accuracy | DiSketch TP | DiSketch FP | DiSketch FN | DiSketch TN |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `performance_bounds/best_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 114 | 0 | 0 | 6130944 | 0.809 | 1.000 | 0.894 | 1.000 | 114 | 27 | 0 | 6130917 |
| `performance_bounds/best_countsketch.ini` | 0.966 | 1.000 | 0.983 | 1.000 | 114 | 4 | 0 | 6130940 | 0.809 | 1.000 | 0.894 | 1.000 | 114 | 27 | 0 | 6130917 |
| `performance_bounds/best_univmon.ini` | 0.877 | 1.000 | 0.934 | 1.000 | 114 | 16 | 0 | 6130928 | 0.226 | 0.816 | 0.354 | 1.000 | 93 | 318 | 21 | 6130626 |
| `performance_bounds/worst_countmin.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 159666 | 0 | 0 | 17644554 | 1.000 | 1.000 | 1.000 | 1.000 | 159659 | 22 | 7 | 17644532 |
| `performance_bounds/worst_countsketch.ini` | 1.000 | 1.000 | 1.000 | 1.000 | 159666 | 12 | 0 | 17644542 | 0.995 | 1.000 | 0.997 | 1.000 | 159664 | 844 | 2 | 17643710 |
| `performance_bounds/worst_univmon.ini` | 0.877 | 1.000 | 0.935 | 0.999 | 159666 | 22360 | 0 | 17622194 | 0.097 | 0.855 | 0.174 | 0.927 | 136504 | 1276144 | 23162 | 16368410 |

## 复现实验

所有脚本位于 `configs/experiments/scripts/`，运行前请确保已在项目根目录执行过 `cmake --build build`，生成 `build/disketch_simulator`。

### 并行批量运行

```bash
cd configs/experiments
uv run scripts/run_parallel.py
```

- `--groups sketch,dataset`：只运行指定实验组，多个组用逗号分隔，默认全部
- `--workers N`：设置最大并发数（默认取 CPU 核心数与配置数量的较小值）
- `--quiet`：关闭终端进度条，仅保留日志和配置文件注释

脚本会将结果写回每个 `.ini` 顶部注释，并把原始输出保存到同级目录的 `logs/*.log` 中。
