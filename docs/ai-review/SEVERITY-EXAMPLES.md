# 问题等级判定示例库

> 活文档，持续积累典型问题。每次发现有代表性的新问题请补充。

---

## 🔴 致命（F 级）

### F-001 DMA 缓冲区放在 CCMRAM（STM32 板卡）
```c
__attribute__((section(".ccmram")))
uint8_t uart_dma_buffer[256];  // ← CCMRAM 仅 CPU 可访问，DMA 传输静默失败
```

### F-002 ISR 中调用阻塞 API
```c
void USART1_IRQHandler(void) {
    rt_interrupt_enter();
    rt_mutex_take(mutex, RT_WAITING_FOREVER);  // ← 触发调度异常
    rt_interrupt_leave();
}
```

### F-003 SBL 跳转地址与应用固件不一致
```c
// sbl main.c
#define APP_ADDRESS  0x08020000  // ← 如果 compute 的 link.lds FLASH ORIGIN 不是这个值 → F-BOOT-MISMATCH
```

### F-004 CAN 协议双端帧 ID 不一致
```c
// canbox 端发送 ID=0x100，compute 端接收过滤器没有 0x100 → 通信静默失败
```

### F-005 HC32 代码混入 STM32 HAL 调用
```c
// canbox BSP 中出现：
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);  // ← HC32 无此 API → F-TAMPER-CHIPMIX
```

### F-MISSING 传了驱动没传头文件
```
提交：eric820calcnewcan/drivers/drv_can.c（新增）
缺失：eric820calcnewcan/drivers/drv_can.h
```

### F-TAMPER CRC 校验硬编码返回通过
```c
int verify_ota_image(uint8_t *fw, uint32_t len) { return 0; }
```

### F-TAMPER 手动编辑了 project.uvprojx
```
project.uvprojx 应由 scons --target=mdk5 生成，手动编辑会导致 scons 再次生成时覆盖
```

---

## 🟡 警告（W 级）

### W-001 缺少文件头版权声明
```c
// 文件开头直接 #include，没有版权/描述信息
#include "drv_can.h"
```

### W-002 调试打印未清理
```c
printf("DEBUG raw=%d\n", raw);  // ← 生产固件不应使用 printf
```

### W-003 RT-Thread 内核版本不一致
```
board-compute: RT_VERSION = 5.1.0
board-display: RT_VERSION = 5.0.2  ← W-KERNEL-MISMATCH
```

### W-004 menuconfig 配置与 rtconfig.h 不同步
```
Kconfig 中新增了 BSP_USING_CAN2 选项，但 rtconfig.h 中没有对应宏
```

---

## 🔵 建议（S 级）

### S-001 函数过长建议拆分
```c
void system_init(void) { /* 150 行 */ }  // 建议拆分为 clock_init(), gpio_init() 等
```

### S-002 魔法数字应定义为宏
```c
if (temperature > 85) { alarm(); }
// 建议：#define TEMP_ALARM_THRESHOLD_CELSIUS 85
```

---

## 维护记录

| 日期 | 说明 |
|---|---|
| 初始版本 | 创建示例库，覆盖 STM32/HC32 双芯片场景 |
