# Eric820 嵌入式仓库 — 文件存放操作说明

> 本文件说明仓库各目录的用途及文件存放规则，请工程师按此指引操作。

---

## 一、固件目录 `fireware/`

存放 RT-Thread SDK 源码及两块板的 BSP 工程代码。

| 目录 | 放什么文件 | 示例 |
|------|-----------|------|
| `fireware/bsp/board-compute/Keil/` | 计算板 Keil 工程文件 | `*.uvprojx`、`*.uvoptx` |
| `fireware/bsp/board-compute/applications/` | 计算板应用层代码 | `main.c`、业务逻辑 `.c/.h` |
| `fireware/bsp/board-compute/drivers/` | 计算板板级驱动 | `drv_can.c`、`board.c`、`board.h` |
| `fireware/bsp/board-display/Keil/` | 显示通讯板 Keil 工程文件 | 同上 |
| `fireware/bsp/board-display/applications/` | 显示通讯板应用层代码 | 同上 |
| `fireware/bsp/board-display/drivers/` | 显示通讯板板级驱动 | 同上 |
| `fireware/src/` | RT-Thread 内核源码 | 从 SDK 拷入，一般不修改 |
| `fireware/components/` | RT-Thread 组件（FinSH、FAL 等）| 从 SDK 拷入，一般不修改 |
| `fireware/libcpu/` | CPU 适配层（Cortex-M 启动文件等）| 从 SDK 拷入，一般不修改 |
| `fireware/include/` | RT-Thread 头文件 | `rtthread.h`、`rtdef.h` 等 |

### 操作步骤
1. 将整个 RT-Thread SDK 的 `src/`、`components/`、`libcpu/`、`include/` 拷贝到 `fireware/` 下
2. 将计算板 BSP 工程整体拷入 `fireware/bsp/board-compute/`
3. 将显示通讯板 BSP 工程整体拷入 `fireware/bsp/board-display/`
4. 确认 Keil 工程文件放在对应 `Keil/` 子目录中

> ⚠️ **注意**：Keil 编译产物（`Objects/`、`Listings/`、`*.axf`、`*.hex` 等）已被 `.gitignore` 排除，无需手动删除。

---

## 二、硬件目录 `hardware/`

存放两块板的硬件设计源文件和生产制造资料。

| 目录 | 放什么文件 | 示例 |
|------|-----------|------|
| `hardware/board-compute/schematic/` | 计算板原理图（PADS Logic） | `*.sch`、`*.prj` |
| `hardware/board-compute/pcb/` | 计算板 PCB 文件（PADS Layout） | `*.pcb` |
| `hardware/board-compute/library/` | 元件库 / 封装库 | `*.pt5`、`*.ld5` |
| `hardware/board-compute/bom/` | BOM 物料清单 | `bom-compute-v1.0.xlsx` |
| `hardware/board-compute/production/v1.0/gerber/` | Gerber 投板文件 | `*.gbr`、`*.drl` |
| `hardware/board-compute/production/v1.0/stencil/` | 钢网文件 | `top-stencil.gbr` 等 |
| `hardware/board-compute/production/v1.0/pick-place/` | 贴片机坐标文件 | `pick-place.csv` |
| `hardware/board-compute/production/v1.0/designator/` | 位号文件 | `designator-map.csv` |
| `hardware/board-compute/production/v1.0/assembly/` | 装配图 / 丝印图 | `assembly-top.pdf` 等 |
| `hardware/board-display/...` | 显示通讯板（与计算板**相同结构**） | 同上 |

### 操作步骤
1. 将原理图源文件放入对应 `schematic/` 目录
2. 将 PCB 文件放入 `pcb/` 目录
3. 将 BOM 表放入 `bom/` 目录（文件名带版本号，如 `bom-compute-v1.0.xlsx`）
4. 生产资料按版本号建子目录（如 `production/v1.0/`），分别放入 gerber、钢网、坐标、位号、装配图
5. 新版本硬件 → 新建 `production/v1.1/` 目录，保留旧版本不删

> ⚠️ **注意**：PADS 的 `.bak`、`.lck`、`.tmp` 等临时文件已被 `.gitignore` 排除。

---

## 三、文档目录 `docs/`

### AI 审计报告 `docs/ai-review/`

存放每次发版前的 **AI 代码审计报告**，用于记录 AI 对固件代码的风险评估和质量论证结论。

| 目录 | 放什么文件 | 示例 |
|------|-----------|------|
| `docs/ai-review/` | AI 审计报告（发版前的代码审查报告） | `ai-review-compute-v1.0.0.md` |

### 文件命名规范
```
ai-review-<板名>-v<版本号>.md
```
- 计算板：`ai-review-compute-v1.0.0.md`
- 显示通讯板：`ai-review-display-v1.0.0.md`

### 什么时候需要生成 AI 审计报告？
- **每次固件发版 Tag 之前**，必须先生成对应板的 AI 审计报告
- 报告内容应包含：代码变更摘要、风险清单、质量评估结论
- 报告通过后方可打 Tag 发布

### 操作步骤
1. 发版前，将 `git log` 粘贴到 AI 提示词（见 `PROMPTS.md` 中的「发版前 AI 论证」）
2. 将 AI 输出的审计报告保存到 `docs/ai-review/` 目录，按命名规范命名
3. 将报告提交到 Git：
   ```bash
   git add docs/ai-review/ai-review-compute-v1.0.0.md
   git commit -m "docs: add AI review report for compute board v1.0.0"
   ```
4. 确认报告无高风险项后，再执行 Tag 发版

---

## 四、根目录管理文件

| 文件 | 用途 | 谁来维护 |
|------|------|----------|
| `README.md` | 项目简介、开发环境、构建步骤 | 工程师填写待填项 |
| `BUSINESS.md` | 业务说明（功能范围、验收标准） | 项目负责人 |
| `DELIVERY.md` | 交付记录（版本号、Tag、Commit ID） | 每次发版时更新 |
| `RULES.md` | 分支规范、提交格式 | 团队约定后固定 |
| `PROMPTS.md` | AI 提示词模板 | 按需补充 |
| `.gitignore` | Git 忽略规则 | 一般无需修改 |

---

## 五、快速检查清单

完成文件存放后，请逐项确认：

```
□ fireware/bsp/board-compute/ 下有 Keil 工程和源码
□ fireware/bsp/board-display/ 下有 Keil 工程和源码
□ fireware/src/ 下有 RT-Thread 内核源码
□ hardware/ 下有原理图、PCB、BOM
□ hardware/*/production/v1.0/ 下有 Gerber 等生产资料
□ 执行 git status 确认无 Objects/ Listings/ 等编译产物
□ 执行 git status 确认无 .bak .lck .tmp 等临时文件
```

---

## 六、Git 操作指南

### 第一步：初始化 Git 并提交

```bash
# 1. 进入项目根目录
cd /c/Projects/rt-thread

# 2. 初始化 Git
git init

# 3. 检查 .gitignore 是否生效
git status
# 应该看到：
# ✅ bsp/eric820/ 下的源码
# ✅ src/ components/ include/ 等内核
# ❌ 不应出现 bsp/ 下其他板子
# ❌ 不应出现 Objects/ Listings/

# 4. 全部加入暂存区
git add .

# 5. 再次确认
git status
# 确认无编译产物、无无关 BSP

# 6. 第一次提交
git commit -m "chore: initial archive [Eric820 RT-Thread SDK + BSP baseline]"
```

### 第二步：GitHub 创建仓库并推送

```bash
# 1. 在 GitHub 组织页面创建空仓库 eric820-embedded
#    ⚠️ 不要勾选 "Initialize with README"

# 2. 添加远程并推送
git remote add origin https://github.com/组织名/eric820-embedded.git
git branch -M main
git push -u origin main

# 如果文件较大，等待几分钟
```
