# Eric820 嵌入式项目

## 项目简介

Eric820 设备嵌入式固件，包含计算板和显示通讯板两块核心板。

## 开发环境

* 操作系统：Windows
* IDE：Keil MDK v5.x
* 芯片：STM32（填写具体型号）
* RTOS：RT-Thread v（填写版本号，见 include/rtdef.h）

## 工程位置

* 计算板：fireware/board-compute/rt-thread/bsp/stm32/eric820calcnewcan/project.uvprojx
* 显示通讯板：fireware/board-display/rt-thread/bsp/stm32/eric820dispnewcan/project.uvprojx
* 计算板与显示板通用的recovery：fireware/recovery/rt-thread/bsp/stm32/eric820\_recovery/project.uvprojx
* 计算板与显示板通用的sbl启动代码：fireware/sbl/eric820\_boot\_sbl/MDK-ARM/eric820\_boot\_sbl.uvprojx
* Canbox板：fireware/board-canbox/rt-thread/bsp/hc32/eric820canbox/project.uvprojx

## 构建步骤

1. 进入对应的工程所在目录，右键ConEmu Here打开env工具
2. 使用menuconfig工具进行配置并保存
3. pkgs --update更新在线组件
4. scons --target=mdk5更新配置后的工程
5. Build → Rebuild All
6. 烧录：（补充烧录方式和工具）

## 烧录说明

1. Segger j-flash新建或打开工程，MCU选择STM32F429BI，接口SWD
2. 将sbl、recovery、application三个HEX文件合并
3. canbox不包含sbl和recovery

保存完整的生产烧录HEX文件用于烧录

## 目录说明

|目录|说明|
|-|-|
|fireware/bsp/board-compute/|计算板 BSP（应用+驱动+板级）|
|fireware/bsp/board-display/|显示通讯板 BSP|
|hardware/board-compute/|计算板硬件设计+生产资料|
|hardware/board-display/|显示通讯板硬件设计+生产资料|
|fireware/src/|RT-Thread 内核|
|fireware/components/|RT-Thread 组件|
|fireware/libcpu/|CPU 适配层|
|fireware/include/|RT-Thread 头文件|



