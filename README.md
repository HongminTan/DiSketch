# DiSketch

DiSketch 是一个基于 Sketch 算法的分布式网络测量框架,实现了论文 [DiSketch: Sketch Disaggregation Across Time and Space](https://arxiv.org/pdf/2503.13515v1) 中的时空分离架构。

## 核心设计思想

### 时空分离架构

DiSketch 将网络测量中的 Sketch 在**时间维度**和**空间维度**上进行分离:

- **时间分离 (Temporal Disaggregation)**: 将一个 epoch 拆分为多个 subepoch,每个 subepoch 独立采样和统计
- **空间分离 (Spatial Disaggregation)**: 将完整的 Sketch 拆分到网络中的多个节点(Fragment)上分布式部署

这种设计能够在有限的内存资源下,通过时空聚合实现接近集中式 Sketch 的精度。

### 代码实现架构

DiSketch 的代码清晰地体现了时空分离的设计:

1. **Fragment 类** - 负责**时间维度**的处理
   - 管理单个网络节点的 Sketch 实例
   - 将 epoch 拆分为多个 subepoch
   - 实现自适应的 subepoch 调整(基于 ρ 值)
   - 提供 `temporal_aggregation()` 静态方法,从多个 subepoch 中恢复流量估计

2. **DiSketch 类** - 负责**空间维度**的处理
   - 协调多个 Fragment 的运行
   - 实现 `spatial_aggregation()` 方法,沿着路径聚合多个 Fragment 的结果
   - 根据 Sketch 类型选择不同的聚合策略:
     - CountMin: 取最小值
     - CountSketch: 取中位数
     - UnivMon: 取平均值

3. **查询流程** - 体现时空分离
   ```
   对于每个流 f:
     1. 空间聚合: 遍历路径上的每个 Fragment
        2. 时间聚合: 在单个 Fragment 中从 subepochs 恢复估计值
     3. 合并所有 Fragment 的结果得到最终估计
   ```

## 项目结构

```
DiSketch/
├── CMakeLists.txt              # CMake 构建配置
├── README.md
├── configs/
│   └── disketch.ini            # 配置文件示例
├── datasets/                   # PCAP 数据集
│   ├── caida_600w.pcap
│   └── ...
├── examples/                   # 示例程序
│   ├── disketch_simulation.cpp # DiSketch 完整仿真
│   ├── baseline.cpp            # 基线对比
│   └── parse_pcap.cpp          # PCAP 解析示例
├── include/                    # 头文件
│   ├── DiSketch.h              # DiSketch 主类(空间聚合)
│   ├── Fragment.h              # Fragment 类(时间聚合)
│   ├── Topology.h              # 拓扑配置
│   ├── Epoch.h                 # Epoch 相关数据结构
│   ├── ConfigParser.h          # 配置解析器
│   ├── PacketParser.h          # PCAP 解析器
│   └── HeavyHitterDetector.h   # 重流检测指标
├── src/                        # 源文件
│   ├── DiSketch.cpp
│   ├── Fragment.cpp
│   ├── Topology.cpp
│   ├── ConfigParser.cpp
│   ├── PacketParser.cpp
│   └── HeavyHitterDetector.cpp
├── PcapPlusPlus-25.05/         # PCAP 解析库(已包含)
├── SketchLib/                  # Sketch 算法库(Git Submodule)
└── simpleini/                  # INI 解析库(已包含)
```

## 构建与运行

### 系统要求

- CMake ≥ 3.16
- Ninja 构建系统
- C++14 编译器
- Git (用于 submodule)

### 构建步骤

```bash
# 克隆项目(包含 submodule)
git clone --recursive https://github.com/HongminTan/DiSketch.git
cd DiSketch

# 创建构建目录
mkdir build && cd build

# 配置并编译
cmake -G Ninja ..
ninja

# 运行仿真
./disketch_simulation ../configs/disketch.ini
```

## 配置文件说明

配置文件使用 INI 格式,通过 SimpleIni 库解析。完整示例见 `configs/disketch.ini`。

### [global] - 全局配置

| 参数 | 类型 | 说明 | 示例 |
|------|------|------|------|
| `pcap` | 字符串 | PCAP 数据集路径 | `../datasets/caida_600w.pcap` |
| `sketch_kind` | 枚举 | Sketch 类型: `CountMin`, `CountSketch`, `UnivMon` | `CountSketch` |
| `epoch_ns` | 整数 | Epoch 时长(纳秒) | `100000000` (100ms) |
| `max_epochs` | 整数 | 最大处理 epoch 数,0=全部 | `6` |
| `full_sketch_depth` | 整数 | Full Sketch 基线的深度(层数) | `8` |
| `heavy_ratio` | 浮点数 | 重流阈值(占总包数比例) | `0.01` (1%) |

### [fragment:名称] - Fragment 配置

每个 `[fragment:名称]` section 定义一个网络节点的 Sketch 配置:

| 参数 | 类型 | 说明 | 示例 |
|------|------|------|------|
| `name` | 字符串 | Fragment 标识符 | `edge_a` |
| `kind` | 枚举 | Sketch 类型(可覆盖全局) | `CountSketch` |
| `memory` | 整数 | 分配内存(字节) | `131072` (128KB) |
| `depth` | 整数 | Sketch 深度(层数/行数) | `4` |
| `initial_subepoch` | 整数 | 初始 subepoch 数量 | `2` |
| `max_subepoch` | 整数 | 最大 subepoch 数量 | `16` |
| `rho_target` | 浮点数 | 目标噪声上界 ρ | `120.0` |
| `boost_single_hop` | 布尔 | 单跳流双采样增强 | `1` (true) |

**参数说明:**

- **memory**: 控制 Sketch 宽度。计算公式:
  - CountMin: `width = memory / (depth × 4)`
  - CountSketch: `width = memory / (depth × 4)`

- **initial_subepoch**: 第一个 epoch 的 subepoch 数量

- **max_subepoch**: Subepoch 数量上限,防止过度分割

- **rho_target**: Fragment 监控实际 ρ 值并自适应调整:
  - 若 `ρ > 2 × rho_target`: 下个 epoch 的 subepoch 数量翻倍
  - 若 `ρ < rho_target / 2`: 下个 epoch 的 subepoch 数量减半

- **boost_single_hop**: 对只经过单个 Fragment 的流,在两个 subepoch 中采样以提高精度

### [path:名称] - 路径配置

每个 `[path:名称]` section 定义一条数据转发路径:

| 参数 | 类型 | 说明 | 示例 |
|------|------|------|------|
| `name` | 字符串 | 路径标识符 | `edge-core-edge` |
| `nodes` | 字符串 | Fragment 名称列表(逗号分隔) | `edge_a,core,edge_b` |

**注意**: `nodes` 中引用的所有 Fragment 必须在前面的 `[fragment:*]` section 中定义。

## 输出示例

运行 `disketch_simulation` 会输出:

1. **配置信息**: Full Sketch 和各 Fragment 的详细参数
2. **每个 Epoch 的统计**:
   - 平均 ρ 值
   - 总包数、总流数
   - 各 Fragment 的 subepoch 数量
3. **重流检测指标对比**:
   - Precision, Recall, F1, Accuracy
   - TP, FP, FN, TN 混淆矩阵

## Git Submodule 管理

SketchLib 作为 Git Submodule 引入,提供 CountMin, CountSketch, UnivMon 等 Sketch 算法实现。

```bash
# 首次克隆(包含 submodule)
git clone --recursive https://github.com/HongminTan/DiSketch.git

# 已克隆但未初始化 submodule
git submodule update --init --recursive

# 更新 submodule 到最新版本
git submodule update --remote --merge
```

### 在当前项目中修改 Submodule 并推送

如果需要在 DiSketch 项目中修改 SketchLib 代码，并将修改推送到 SketchLib 仓库：

#### 步骤 1：在 Submodule 中进行修改

```bash
# 进入 submodule 目录
cd SketchLib

# 确保在正确的分支上
git checkout main

# 进行代码修改...
# 修改完成后，提交更改
git add .
git commit -m "Commit Message"
```

如果未在正确分支上并提交了commit，可以使用以下命令将更改应用到正确的分支：

```bash
# 找到提交的 hash
git reflog

# 切换到正确的分支
git checkout main

# cherry-pick 提交
git cherry-pick <commit_hash>
```

#### 步骤 2：推送到 SketchLib 仓库

```bash
# 推送到 SketchLib 的远程仓库
git push origin main
```

**注意**：需要有 SketchLib 仓库的写入权限才能推送。

#### 步骤 3：在主项目中更新 Submodule 引用

```bash
# 返回主项目目录
cd ..

# 此时 Git 会检测到 submodule 的 commit hash 已更改
git status

# 提交 submodule 指针的更新
git add SketchLib
git commit -m "Update SketchLib"

# 推送到 DiSketch 仓库
git push
```

### 查看 Submodule 状态

```bash
# 查看所有 submodule 的状态
git submodule status

# 查看 submodule 的详细信息
git submodule
```

### 删除 Submodule

```bash
# 1. 删除 submodule 的配置
git submodule deinit -f SketchLib

# 2. 删除 .git/modules 中的 submodule
rm -rf .git/modules/SketchLib

# 3. 删除工作目录中的 submodule
git rm -f SketchLib

# 4. 提交更改
git commit -m "Remove SketchLib submodule"
```
