# DiSketch

## 项目简介

DiSketch 是一个基于 Sketch 算法的分布式部署并聚合统计的网络测量框架。

## 项目结构

```
DiSketch/
├── SketchLib/            # Sketch 算法库 (Git Submodule)
├── .gitignore
├── .gitmodules           # Git submodule 配置文件
└── README.md
```

### 关于 SketchLib Submodule

**SketchLib** 是作为 Git Submodule 引入的外部依赖库，来源于 [HongminTan/SketchLib](https://github.com/HongminTan/SketchLib)。

## Git Submodule 操作指南

### 首次克隆项目（包含 submodule）

如果首次克隆本项目，需要同时初始化并更新 submodule：

```bash
# 方式 1：克隆时自动初始化 submodule
git clone --recursive git@github.com:HongminTan/DiSketch.git

# 方式 2：先克隆项目，再初始化 submodule
git clone git@github.com:HongminTan/DiSketch.git
cd DiSketch
git submodule init
git submodule update
```

### 更新 Submodule 到最新版本

当 SketchLib 仓库有更新时，可以使用以下命令更新 submodule：

```bash
# 进入 submodule 目录
cd SketchLib

# 拉取最新代码
git pull origin main

# 返回主项目目录
cd ..

# 提交 submodule 的更新
git add SketchLib
git commit -m "Update SketchLib Submodule"
git push
```

或者使用一条命令更新所有 submodule：

```bash
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