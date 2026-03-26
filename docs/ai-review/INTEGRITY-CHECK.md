---
trigger: always_on
alwaysApply: true
---

# 提交完整性验证规则 — 防遗漏 / 防错传 / 防篡改

> **核心原则：代码审查的第一步不是看代码质量，而是验证"该交的东西交齐了没有"。**
> 提交不完整 = 致命问题（F 级），纳入审查报告与代码质量问题一起输出。

---

## 1. 强制配对检查

### 1.1 文件配对（缺一触发致命）

| 提交了 | 必须同时提交 | 缺失编号 | 说明 |
|---|---|---|---|
| `xxx.c` | `xxx.h` | F-MISSING-HEADER | 源文件与头文件必须配对 |
| `xxx.h` | `xxx.c`（或说明是纯声明） | F-MISSING-SOURCE | 头文件中声明的函数必须有实现 |
| 任何 `.c` 文件 | 所在目录的 `SConscript` | F-MISSING-SCONSCRIPT | 新文件必须注册到构建系统 |
| 修改了 `board.c` / 时钟 / 引脚 | CubeMX `.ioc` 或配置说明 | F-MISSING-HWCONFIG | 硬件配置变更必须有据可查 |
| 修改了内存布局 | 链接脚本（`.lds` / `.sct`） | F-MISSING-LINKER | 内存布局变更必须同步 |
| 修改了 RTOS 配置 | `rtconfig.h` | F-MISSING-RTCONFIG | RTOS 配置变更必须同步 |
| 新增外设驱动 | `Kconfig` 配置项 | F-MISSING-KCONFIG | 新外设必须可通过 menuconfig 开关 |
| 修改了 SBL 跳转地址 | compute 和 display 两端的链接脚本 | F-MISSING-BOOT-SYNC | SBL 与两个应用固件 Flash 地址必须一致 |
| 修改了 SBL CubeMX 生成代码 | `eric820_boot_sbl.ioc` | F-MISSING-IOC-SYNC | 禁止手动改 CubeMX 生成代码不同步 .ioc |
| 修改了 Recovery Flash 写入地址 | compute 和 display 的链接脚本 | F-MISSING-RECOVERY-SYNC | Recovery 与两个应用固件分区必须兼容 |
| 修改了 canbox CAN 协议 | compute 端 CAN 接收代码 | F-MISSING-CAN-SYNC | 装置间 CAN 协议双端必须同步 |
| 修改了 compute CAN 协议 | canbox 端 CAN 发送代码 | F-MISSING-CAN-SYNC | 装置间 CAN 协议双端必须同步 |
| 修改了 compute ↔ display SPI 协议 | 对端的 SPI 代码 | F-MISSING-SPI-SYNC | SPI 通信双端必须同步 |

### 1.2 内核与工程保护（最高优先级）

| 检测内容 | 编号 | 处理 |
|---|---|---|
| 修改了 BSP 目录以外的 `rt-thread/` 内核文件 | F-TAMPER-KERNEL | **立即标记致命，要求撤回** |
| 手动编辑了 `project.uvprojx` | F-TAMPER-PROJECT | 应由 `scons --target=mdk5` 生成 |
| 修改了 `sbl/eric820_boot_sbl/Drivers/` HAL 库 | F-TAMPER-HAL | 应通过 CubeMX 重新生成 |
| STM32 代码出现在 canbox(HC32) BSP 中 | F-TAMPER-CHIPMIX | 芯片类型混用，可能导致硬件故障 |

---

## 2. 依赖完整性扫描

### 2.1 #include 依赖扫描
- 扫描所有 `#include "xxx.h"` 引用（非系统头文件）
- 验证被引用的 `.h` 文件是否在提交物中或已确认存在于仓库
- 缺失 → `F-MISSING-INCLUDE`

### 2.2 外部函数/变量引用扫描
- 扫描 `extern` 声明和跨文件函数调用
- 验证实现文件是否在提交物中
- 缺失 → `F-MISSING-IMPL`

### 2.3 外部模块/第三方库扫描
- 检查是否引用了 RT-Thread 软件包、第三方库（lwIP/FatFS/cJSON 等）
- 验证 `packages/` 目录和 `rtconfig.h` 中对应宏
- 缺失 → `F-MISSING-PACKAGE`

### 2.4 Kconfig / menuconfig 一致性
- `rtconfig.h` 中的宏应与 `Kconfig` 定义的选项一致
- 新增 `Kconfig` 选项必须有 `default` 值
- 不一致 → `W-CONFIG-MISMATCH`

---

## 3. 敏感文件 / 关键文件缺失检测

| 敏感文件类型 | 检测方法 | 缺失编号 |
|---|---|---|
| **密钥/证书**（`.pem` `.key` `.crt`） | TLS/SSL 初始化相关调用 | F-MISSING-CREDENTIAL |
| **通讯协议配置**（CAN ID 表、SPI 帧定义） | 协议解析代码引用 | F-MISSING-PROTOCOL |
| **校准数据**（传感器校准系数、ADC 校准表） | 校准数组引用 | F-MISSING-CALIBRATION |
| **OTA / Bootloader 配置**（分区表、版本号） | OTA 逻辑 | F-MISSING-OTA |
| **Flash 分区表**（FAL 分区、`fal_cfg.h`） | FAL/文件系统 API | F-MISSING-PARTITION |
| **引脚映射表** | 新增外设但映射未更新 | F-MISSING-PINMAP |
| **中断向量注册** | 新增中断但入口未注册 | F-MISSING-IRQ |
| **构建索引** | 新增子目录但 SConscript 未引入 | F-MISSING-BUILD |

---

## 4. 代码删减 / 篡改检测

### 4.1 错误处理被删除或架空
```c
/* 可疑：返回值被忽略 */
HAL_UART_Transmit(&huart1, buf, len, 100);
/* 可疑：空 else / 错误后继续执行 */
if (ptr == RT_NULL) { rt_kprintf("fail\n"); /* 没有 return */ }
```
→ `F-TAMPER-ERRHANDLE`

### 4.2 安全校验被跳过
```c
// if (cmd_id > MAX_CMD_ID) return -1;  // ← 被注释
```
→ `F-TAMPER-VALIDATION`

### 4.3 关键功能空实现
```c
int verify_firmware_crc(uint8_t *data, uint32_t len) { return 0; }
```
→ `F-TAMPER-STUBBED`

### 4.4 调试后门未移除
```c
#define DEBUG_PASSWORD "123456"
#if 1  /* 原来是 #if DEBUG_MODE */
    skip_authentication();
#endif
```
→ `F-TAMPER-BACKDOOR`

### 4.5 日志泄露敏感信息
```c
rt_kprintf("token: %s\n", auth_token);
```
→ `F-TAMPER-LEAKAGE`

---

## 5. 完整性验证输出格式

完整性问题在报告 **最前面** 输出：

```
## ⛔ 提交完整性验证

### 验证结果：❌ 不通过（N 个问题）

| 编号 | 类型 | 问题描述 | 涉及文件 | 要求 |
|---|---|---|---|---|
| F-MISSING-HEADER-001 | 文件缺失 | `drv_can.c` 缺少配对头文件 | compute BSP | 补交 |
| F-TAMPER-KERNEL-001 | 内核篡改 | 修改了 rt-thread/src/ipc.c | — | 撤回修改 |
```

通过时输出：
```
## ✅ 提交完整性验证：通过
```

---

## 6. 提交清单校验

- [ ] 清单是否附带？→ 未附带直接打回
- [ ] 「涉及工程」是否标注？→ 未标注打回
- [ ] 清单文件数量与实际提交是否一致？→ 不一致追问
- [ ] 自检项有未勾选？→ 要求说明原因
- [ ] 变更说明是否为空或敷衍？→ 要求补充
- [ ] 构建验证是否勾选通过？→ 未通过要求说明
- 清单中列出但未提交 → `F-MISSING-DECLARED`
- 实际提交但清单未列 → `W-UNDECLARED`
