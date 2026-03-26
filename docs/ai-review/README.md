# docs/ai-review — Eric820 代码审查指令集

## 目录说明

本目录包含 AI 代码审查系统的全部指令文件，用于 Claude Projects 中的代码审查专用项目。

```
docs/ai-review/
├── README.md                  ← 本文件，目录说明与使用指南
├── REVIEW-RULES.md            ← 【核心】审查主指令（角色/维度/输出格式/铁律）
├── INTEGRITY-CHECK.md         ← 【核心】提交完整性验证规则（防漏传/错传/篡改）
├── SUBMIT-CHECKLIST.md        ← 【下发给工程师】提交前自检清单模板
├── REPO-STRUCTURE.md          ← 仓库目录结构基线（审查时的参考依据）
└── SEVERITY-EXAMPLES.md       ← 问题等级判定示例库（持续积累）
```

## 使用方法

### 1. Claude Projects 配置
在 Claude Projects 中创建「Eric820 代码审查」项目，将以下文件内容按顺序添加到 Project Instructions 中：

1. **REVIEW-RULES.md** — 主指令（必须）
2. **INTEGRITY-CHECK.md** — 完整性验证（必须）
3. **REPO-STRUCTURE.md** — 仓库结构基线（必须）
4. **SEVERITY-EXAMPLES.md** — 判定示例（推荐）

### 2. 下发给工程师
将 **SUBMIT-CHECKLIST.md** 下发给所有工程师，要求每次提交代码审查时必须附带填写好的清单。

### 3. 审查流程
```
工程师提交代码 + 填写好的清单
        ↓
AI 第一步：检查清单是否附带（无清单直接打回）
        ↓
AI 第二步：提交完整性验证（配对/依赖/敏感文件/篡改检测）
        ↓
AI 第三步：逐文件代码质量审查（六大维度）
        ↓
输出完整审查报告（完整性问题 + 质量问题，按等级分类）
```

## 项目概况

- **项目**：Eric820 嵌入式固件
- **5 个工程**：board-compute / board-display / recovery（通用）/ sbl（通用）/ board-canbox
- **开发环境**：Windows + Keil MDK v5.x
- **构建流程**：menuconfig → pkgs --update → scons --target=mdk5 → Keil Rebuild All
- **烧录**：Segger J-Flash 合并 sbl + recovery + app 三个 HEX（canbox 除外）

## 维护规则

- **SEVERITY-EXAMPLES.md** 是活文档，每次发现新的典型问题应补充进去
- **REPO-STRUCTURE.md** 在仓库目录结构发生重大变化时必须同步更新
- 所有指令文件的修改必须经过负责人确认
