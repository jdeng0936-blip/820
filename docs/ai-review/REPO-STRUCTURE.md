---
trigger: always_on
alwaysApply: true
---

# Eric820 仓库目录结构基线

> 审查时用于判断文件应该放在哪里、缺了什么、结构是否合理的参考依据。
> **最后更新**：基于项目 README 和工程路径确认。

---

## 1. 项目概况

| 项目 | 值 |
|---|---|
| 产品 | Eric820 嵌入式设备固件 |
| 开发环境 | **Windows + Keil MDK v5.x** |
| 构建流程 | menuconfig → `pkgs --update` → `scons --target=mdk5` → Keil Rebuild All |
| 烧录工具 | Segger J-Flash（SWD 接口） |
| 烧录方式 | 合并 sbl + recovery + application 三个 HEX（canbox 除外） |

---

## 2. 物理装置与板卡映射

```
┌──────────────────────────────────────────────────────────┐
│  装置 A — CAN 电流采集终端                                 │
│  ┌─────────────────────┐                                  │
│  │   board-canbox      │  CAN 接收盒（电流采集 + CAN 通讯） │
│  │   HC32 芯片          │  ← 注意：不是 STM32！             │
│  └─────────┬───────────┘                                  │
└────────────┼──────────────────────────────────────────────┘
             │ CAN 总线通讯
┌────────────┼──────────────────────────────────────────────┐
│  装置 B — 主机（双核心板）                                   │
│  ┌─────────┴───────────┐  SPI 通讯  ┌──────────────────┐  │
│  │  board-compute      │◄──────────►│  board-display   │  │
│  │  计算板              │            │  显示通讯板       │  │
│  │  STM32F429BI        │            │  STM32F429BI     │  │
│  └─────────────────────┘            │                  │  │
│                                     │  ┌────────────┐  │  │
│  ┌─────────────────────────┐        │  │ sbl        │  │  │
│  │  sbl (通用)             │───────►│  │ Bootloader │  │  │
│  │  二级 Bootloader        │        │  ├────────────┤  │  │
│  │  Keil MDK 工程          │        │  │ recovery   │  │  │
│  │  计算板 + 显示板共用     │        │  │ 恢复固件    │  │  │
│  └─────────────────────────┘        │  └────────────┘  │  │
│                                     └──────────────────┘  │
│  ※ sbl 和 recovery 为计算板与显示板通用                      │
└───────────────────────────────────────────────────────────┘
```

---

## 3. 子项目清单

| 序号 | 子项目 | Keil 工程路径 | 芯片 | 装置 | 用途 | 含 sbl/recovery |
|---|---|---|---|---|---|---|
| 1 | board-compute | `fireware/board-compute/rt-thread/bsp/stm32/eric820calcnewcan/project.uvprojx` | **STM32F429BI** | 装置B | 计算板 | 是 |
| 2 | board-display | `fireware/board-display/rt-thread/bsp/stm32/eric820dispnewcan/project.uvprojx` | **STM32F429BI** | 装置B | 显示通讯板 | 是 |
| 3 | recovery | `fireware/recovery/rt-thread/bsp/stm32/eric820_recovery/project.uvprojx` | **STM32F429BI** | 装置B(通用) | 恢复/升级固件 | — |
| 4 | sbl | `fireware/sbl/eric820_boot_sbl/MDK-ARM/eric820_boot_sbl.uvprojx` | **STM32F429BI** | 装置B(通用) | 二级 Bootloader | — |
| 5 | board-canbox | `fireware/board-canbox/rt-thread/bsp/hc32/eric820canbox/project.uvprojx` | **HC32 系列** | 装置A | CAN 电流采集 | 否 |

### 关键差异
- **Canbox 使用 HC32 芯片**，不是 STM32。审查时芯片手册、寄存器、HAL API 全部不同
- **SBL 是 CubeMX + Keil 工程**（无 RT-Thread），其余 4 个是 RT-Thread + scons 生成 Keil 工程
- **Recovery 和 SBL 为双板通用**，Flash 布局必须与 compute 和 display 两个应用固件都兼容

---

## 4. 通信关系

| 链路 | 方式 | 审查关注点 |
|---|---|---|
| canbox ↔ compute | **CAN 总线** | 双方 CAN 协议 ID 表、波特率、帧格式必须一致 |
| compute ↔ display | **SPI** | 双方 SPI 模式/频率/主从角色/片选/数据协议必须匹配 |
| sbl → 应用固件 | Flash 内部跳转 | 跳转地址与 compute/display 的链接脚本 FLASH 起始一致 |
| recovery → 主固件 | Flash 分区/OTA | 分区表与 compute/display 两个应用固件都兼容 |

---

## 5. 实际工程目录结构

### 5.1 RT-Thread 板卡工程（compute / display / recovery / canbox）

BSP 代码位于 `rt-thread/bsp/` 下，不是板卡根目录：

```
fireware/board-compute/                    # 计算板项目根目录
└── rt-thread/                             # RT-Thread 完整仓库
    ├── bsp/stm32/eric820calcnewcan/       # ★ 计算板 BSP（核心代码在这里）
    │   ├── applications/                  # 应用层代码
    │   ├── board/                         # 板级初始化（board.c/board.h/link脚本/CubeMX）
    │   ├── drivers/                       # 外设驱动
    │   ├── packages/                      # RT-Thread 软件包
    │   ├── rtconfig.h                     # RTOS 配置
    │   ├── rtconfig.py                    # scons 工具链配置
    │   ├── SConstruct                     # scons 顶层入口
    │   ├── Kconfig                        # menuconfig 入口
    │   ├── project.uvprojx                # Keil 工程文件（scons 生成）
    │   └── ...
    ├── components/                        # RT-Thread 组件
    ├── include/                           # RT-Thread 头文件
    ├── libcpu/                            # CPU 适配层
    ├── src/                               # RT-Thread 内核源码
    └── tools/                             # 构建工具
```

**各工程 BSP 路径对照：**

| 工程 | BSP 根路径 |
|---|---|
| board-compute | `fireware/board-compute/rt-thread/bsp/stm32/eric820calcnewcan/` |
| board-display | `fireware/board-display/rt-thread/bsp/stm32/eric820dispnewcan/` |
| recovery | `fireware/recovery/rt-thread/bsp/stm32/eric820_recovery/` |
| board-canbox | `fireware/board-canbox/rt-thread/bsp/hc32/eric820canbox/` |

### 5.2 SBL 工程（CubeMX + Keil，无 RT-Thread）

```
fireware/sbl/eric820_boot_sbl/
├── Core/                              # CubeMX 生成
│   ├── Inc/                           # 头文件
│   ├── Src/                           # 源文件（含 main.c）
│   └── Startup/                       # 启动文件（.s）
├── Drivers/                           # ST HAL 库
│   ├── CMSIS/
│   └── STM32F4xx_HAL_Driver/
├── MDK-ARM/                           # Keil 工程
│   └── eric820_boot_sbl.uvprojx
├── eric820_boot_sbl.ioc               # CubeMX 配置（权威硬件配置）
└── .mxproject
```

### 5.3 硬件目录

```
hardware/
├── bom/                               # 物料清单
├── library/                           # 元件库
├── pcb/                               # PCB 布局
├── production/                        # 生产文件（Gerber 等）
└── schematic/                         # 原理图
```

---

## 6. 构建流程（审查时验证工程师是否正确执行）

### RT-Thread 工程（compute / display / recovery / canbox）
```
1. cd fireware/board-xxx/rt-thread/bsp/stm32/eric820xxx/  # 进入 BSP 目录
2. menuconfig                                               # 配置
3. pkgs --update                                            # 更新在线组件
4. scons --target=mdk5                                      # 生成/更新 Keil 工程
5. 打开 project.uvprojx → Rebuild All                       # Keil 编译
```

### SBL 工程
```
1. 打开 fireware/sbl/eric820_boot_sbl/MDK-ARM/eric820_boot_sbl.uvprojx
2. Rebuild All
```

### 烧录（Segger J-Flash）
```
1. J-Flash 新建/打开工程，MCU 选择 STM32F429BI，接口 SWD
2. 合并 sbl.hex + recovery.hex + application.hex → 生产烧录 HEX
3. canbox 单独烧录（不含 sbl/recovery）
```

---

## 7. 文件归属规则

| 文件类型 | 应当位于 | 常见错误 |
|---|---|---|
| 应用代码 | `bsp/stm32/eric820xxx/applications/` | 放在 BSP 根目录或 drivers/ |
| 外设驱动 | `bsp/stm32/eric820xxx/drivers/` | 放在 applications/ |
| 板级初始化 | `bsp/stm32/eric820xxx/board/` | 直接修改 rt-thread/src/ |
| RT-Thread 配置 | `bsp/stm32/eric820xxx/rtconfig.h` | 手动改内核源码 |
| 第三方软件包 | `bsp/stm32/eric820xxx/packages/` | 手动复制且无版本管理 |
| Keil 工程文件 | `bsp/stm32/eric820xxx/project.uvprojx` | 手动编辑（应由 scons 生成） |
| SBL 代码 | `fireware/sbl/eric820_boot_sbl/Core/` | 放在其他板卡目录 |
| CubeMX 配置 | SBL: `.ioc` / 板卡: `board/CubeMX_Config/` | 未提交或放错位置 |

**铁律**：
- `rt-thread/` 下除了 `bsp/stm32/eric820xxx/`（或 `bsp/hc32/eric820canbox/`）以外的目录禁止修改
- `project.uvprojx` 应由 `scons --target=mdk5` 生成，禁止手动编辑后提交

---

## 8. 跨板卡审查规则

### 8.1 CAN 通信一致性（canbox ↔ compute）
- canbox 是 **HC32 芯片**，compute 是 **STM32F429**，两端 CAN 外设和 HAL API 不同
- 审查重点在**协议层**：帧 ID 表、波特率、数据格式、字节序必须双端一致
- CAN 协议定义建议提取为公共头文件

### 8.2 SPI 通信一致性（compute ↔ display）
- 两端都是 STM32F429，SPI 外设一致
- 审查 CPOL/CPHA、时钟频率、主从角色、片选引脚、数据协议

### 8.3 启动链完整性（sbl → recovery / application）
- SBL 和 Recovery 是**双板通用**的，Flash 布局必须与 compute 和 display 两个应用固件都兼容
- 烧录 HEX = sbl + recovery + application，三段 Flash 区域不得重叠
- SBL 跳转地址必须与两个应用固件的 FLASH ORIGIN 一致
- 不一致 → `F-BOOT-MISMATCH`

### 8.4 RT-Thread 内核版本
- 各板卡的 `rt-thread/include/rtdef.h` 版本宏应一致
- 不一致 → `W-KERNEL-MISMATCH`

---

## 9. 关键文件清单

### STM32 板卡（compute / display / recovery）

| 文件 | 位于 | 修改后必须验证 |
|---|---|---|
| `board/linker_scripts/link.lds` | BSP 目录下 | Flash 地址与 sbl 跳转一致 |
| `board/board.c` | BSP 目录下 | 时钟树与 CubeMX/原理图一致 |
| `rtconfig.h` | BSP 根目录 | 堆栈大小、Tick 频率、组件开关 |
| `rtconfig.py` | BSP 根目录 | 工具链路径、编译标志 |
| `SConstruct` / `SConscript` | BSP 及各子目录 | 新文件是否注册 |
| `project.uvprojx` | BSP 根目录 | 应由 scons 生成，非手动编辑 |
| `Kconfig` | BSP 根目录 | menuconfig 配置入口 |

### HC32 Canbox

| 文件 | 说明 |
|---|---|
| BSP 下所有文件 | HC32 芯片，参考手册不同于 STM32，审查时注意区分 |
| CAN 协议定义文件 | 必须与 compute 端一致 |

### SBL

| 文件 | 说明 |
|---|---|
| `eric820_boot_sbl.ioc` | CubeMX 配置（权威） |
| `Core/Src/main.c` | Flash 跳转地址逻辑 |
| `MDK-ARM/eric820_boot_sbl.uvprojx` | Keil 工程配置 |

---

## 10. .gitignore 应当忽略的文件

提交中出现以下文件标记为 `W-UNWANTED`：

- `*.o` / `*.d` / `*.lst` / `*.crf` / `*.dep` — 编译中间产物
- `*.axf` / `*.elf` / `*.bin` / `*.hex` — 编译输出
- `*.bak` / `*.~*` — 编辑器备份
- `Objects/` / `Listings/` / `build/` — Keil/scons 编译输出目录
- `DebugConfig/` / `*.build_log.htm` — Keil 调试配置
- `RTE/` — Keil 运行时环境
- `.vscode/` / `.idea/` — IDE 个人配置
- `__pycache__/` / `*.pyc` — Python 缓存
- `*.ioc.bak` — CubeMX 备份

---

## 11. 维护记录

| 日期 | 变更 | 说明 |
|---|---|---|
| 初始版本 | 新建 | 基于项目 README 确认真实结构；canbox 为 HC32 芯片；sbl/recovery 双板通用 |
