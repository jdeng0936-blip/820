# Eric888 嵌入式项目

## 项目简介
Eric888 设备嵌入式固件，包含计算板和显示通讯板两块核心板。

## 开发环境
- 操作系统：Windows
- IDE：Keil MDK v5.x
- 芯片：STM32（填写具体型号）
- RTOS：RT-Thread v（填写版本号，见 include/rtdef.h）

## 工程位置
- 计算板：firmware/bsp/board-compute/Keil/（填工程文件名）.uvprojx
- 显示通讯板：firmware/bsp/board-display/Keil/（填工程文件名）.uvprojx

## 构建步骤
1. 用 Keil MDK 打开对应 .uvprojx 工程文件
2. Build → Rebuild All
3. 烧录：（补充烧录方式和工具）

## 目录说明
| 目录 | 说明 |
|------|------|
| firmware/bsp/board-compute/ | 计算板 BSP（应用+驱动+板级）|
| firmware/bsp/board-display/ | 显示通讯板 BSP |
| hardware/board-compute/ | 计算板硬件设计+生产资料 |
| hardware/board-display/ | 显示通讯板硬件设计+生产资料 |
| firmware/src/ | RT-Thread 内核 |
| firmware/components/ | RT-Thread 组件 |
| firmware/libcpu/ | CPU 适配层 |
| firmware/include/ | RT-Thread 头文件 |
