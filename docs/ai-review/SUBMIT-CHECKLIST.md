# Eric820 代码提交审查清单

> **每次提交代码审查前必须填写本清单，与代码一起提交。未附清单的提交直接打回。**

---

## 基本信息

- **提交人**：
- **提交日期**：
- **关联任务编号**（如有）：
- **变更类型**：[ ] 新功能　[ ] Bug 修复　[ ] 重构　[ ] 配置变更　[ ] 其他：___

---

## 涉及工程（必选，可多选）

- [ ] board-compute（计算板 · eric820calcnewcan）
- [ ] board-display（显示通讯板 · eric820dispnewcan）
- [ ] recovery（恢复固件 · eric820_recovery · 双板通用）
- [ ] sbl（二级 Bootloader · eric820_boot_sbl · 双板通用）
- [ ] board-canbox（CAN 采集终端 · eric820canbox · **HC32 芯片**）
- [ ] 跨工程公共修改（在变更说明中注明）

**模块/功能名称**：

---

## 提交文件清单

| 序号 | 工程 | 文件路径 | 变更类型 | 说明 |
|---|---|---|---|---|
| 1 | 例：compute | `bsp/stm32/eric820calcnewcan/drivers/drv_can.c` | 新增 | CAN 驱动 |
| 2 | | | | |
| 3 | | | | |

---

## 自检清单

### A. 文件完整性
- [ ] 每个 `.c` 文件都有对应的 `.h`（或已说明原因）
- [ ] 新文件已注册到 `SConscript`
- [ ] 所有 `#include` 引用的自定义头文件已包含或确认存在
- [ ] 涉及的 RT-Thread 软件包已说明版本
- [ ] **未修改 BSP 目录以外的 `rt-thread/` 内核文件**
- [ ] **未手动编辑 `project.uvprojx`**（应由 scons 生成）

### B. 硬件相关
- [ ] 不涉及硬件变更（勾选跳过 B 其余项）
- [ ] 修改了引脚配置 → 已附 CubeMX `.ioc` 或引脚说明
- [ ] 修改了时钟配置 → 已附时钟树截图或参数说明
- [ ] 新增外设 → 已确认 DMA 通道/中断优先级无冲突
- [ ] 修改了内存布局 → 链接脚本已同步

### C. 安全与敏感文件
- [ ] 不涉及密钥/证书
- [ ] 涉及密钥/证书 → 部署方式：___
- [ ] 不涉及通讯协议变更（CAN/SPI）
- [ ] 涉及协议变更 → 对端代码已同步提交：___
- [ ] 不涉及 OTA/分区表/Bootloader
- [ ] 涉及 OTA/分区变更 → 配置已附：___

### D. 代码质量
- [ ] 所有 HAL 函数返回值已检查
- [ ] 所有 `rt_malloc` 返回值已判空
- [ ] 无 `TODO` / `FIXME` / `HACK` 残留（或已说明）
- [ ] 无硬编码密码/测试账号/调试后门
- [ ] 无被注释掉的安全校验代码
- [ ] ISR 中无阻塞 API
- [ ] 调试打印已清理或在 `#ifdef DEBUG` 下

### E. 跨工程专项
- [ ] 不涉及跨工程变更（勾选跳过 E 其余项）
- [ ] CAN 协议修改 → canbox 和 compute 双端已同步
- [ ] SPI 协议修改 → compute 和 display 双端已同步
- [ ] SBL/Recovery 修改 → Flash 布局与 compute 和 display 两端都兼容
- [ ] 三段 HEX（sbl+recovery+app）合并后 Flash 区域不重叠

### F. 构建与测试
- [ ] 执行了完整构建流程：menuconfig → pkgs --update → scons --target=mdk5 → Keil Rebuild
- [ ] 编译通过：0 Error 0 Warning
- [ ] 已在目标板烧录并基本功能测试
- [ ] 未做板级测试 → 原因：___
- [ ] （如涉及烧录）已验证 J-Flash HEX 合并烧录正常

---

## 变更说明（至少 3 句话）



---

## 已知限制 / 遗留问题（无则填"无"）



---

## 备注（无则填"无"）


