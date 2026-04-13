# Gitea CI/CD 固件自动编译指南

> 面向嵌入式开发者，从零建立"推代码 → 自动编译 → 下载 .hex"的完整流水线

---

## 一、为什么需要 CI/CD

当前的痛点：

```
你(macOS) 改代码 → 发源码给同事 → 同事(Windows+Keil)编译 → 生成hex → 烧录测试
                    ↑
              手动传递，容易出错：版本混乱、忘记改、编译环境不一致
```

CI/CD 之后：

```
你(macOS) 改代码 → git push → Gitea自动编译 → 自动生成hex → 同事直接下载烧录
                                ↑
                    全自动，统一环境，可溯源，零人工
```

---

## 二、整体架构（一张图看懂）

```
┌─────────────┐    git push     ┌─────────────────┐
│  你的 Mac    │ ──────────────→ │   Gitea 服务器    │
│  写代码       │                │   (代码仓库)      │
└─────────────┘                └────────┬────────┘
                                        │ 触发
                                        ▼
                               ┌─────────────────┐
                               │  Gitea Runner    │
                               │  (编译服务器)     │
                               │                  │
                               │  安装了:          │
                               │  - arm-none-eabi │
                               │  - scons/python  │
                               │  - make          │
                               └────────┬────────┘
                                        │ 编译完成
                                        ▼
                               ┌─────────────────┐
                               │  Release 页面     │
                               │                  │
                               │  canbox.hex      │
                               │  compute.hex     │
                               │  fiber.hex       │
                               │                  │
                               │  同事直接下载烧录  │
                               └─────────────────┘
```

---

## 三、前置条件

你需要：

| 组件 | 说明 | 你有吗？ |
|------|------|---------|
| Gitea 服务器 | 已有（内网离线环境） | 有 |
| Gitea Actions | Gitea 自带，需开启 | 需确认 |
| Runner 机器 | 一台 Linux/Windows 电脑，用来跑编译 | 需准备 |
| arm-none-eabi-gcc | 安装在 Runner 上 | Runner上装 |
| scons + Python | 安装在 Runner 上 | Runner上装 |

---

## 四、手把手搭建步骤

### 步骤 1：在 Gitea 上开启 Actions 功能

编辑 Gitea 服务器的配置文件 `app.ini`（通常在 `/etc/gitea/` 或 Gitea 安装目录下）：

```ini
[actions]
ENABLED = true
```

重启 Gitea 服务：
```bash
sudo systemctl restart gitea
```

### 步骤 2：准备一台 Runner 机器

Runner 就是"干活的工人"，它从 Gitea 接任务，执行编译，把结果传回来。

**推荐方案**：用一台闲置的 Linux 电脑（Ubuntu 20.04+），也可以是 Windows。

在 Runner 机器上安装编译工具：

```bash
# Ubuntu 示例

# 1. 安装 ARM 交叉编译器
sudo apt update
sudo apt install -y gcc-arm-none-eabi

# 2. 安装 Python 和 scons
sudo apt install -y python3 python3-pip
pip3 install scons

# 3. 验证
arm-none-eabi-gcc --version
scons --version
```

### 步骤 3：在 Runner 机器上安装 Gitea Runner

```bash
# 1. 下载 act_runner（Gitea 官方 Runner 程序）
# 到 https://gitea.com/gitea/act_runner/releases 下载对应系统版本
# 或者直接：
wget https://gitea.com/gitea/act_runner/releases/download/v0.2.11/act_runner-0.2.11-linux-amd64
chmod +x act_runner-0.2.11-linux-amd64
sudo mv act_runner-0.2.11-linux-amd64 /usr/local/bin/act_runner

# 2. 在 Gitea 网页上获取注册 Token
#    进入你的仓库 → 设置 → Actions → Runners → 创建新的 Runner
#    复制页面上显示的 注册 Token

# 3. 注册 Runner
act_runner register \
  --instance http://你的gitea地址:3000 \
  --token 你复制的Token \
  --name eric820-builder \
  --labels ubuntu-latest:host

# 4. 启动 Runner（后台常驻）
nohup act_runner daemon &
```

注册成功后，在 Gitea 网页的 Runner 列表中能看到你的 Runner 状态为"在线"。

### 步骤 4：在项目中创建工作流文件

这是最关键的一步。在项目根目录创建 `.gitea/workflows/build.yaml`：

```yaml
name: 固件编译

# 什么时候触发编译
on:
  push:
    branches: [master, main]    # 推送到主分支时
    paths:                       # 只有这些目录有改动才触发
      - 'fireware/**'
      - 'extracted_can_bus/**'
  workflow_dispatch:              # 也支持手动触发（网页上点按钮）

jobs:
  build-canbox:
    name: HC32 采集器
    runs-on: ubuntu-latest
    steps:
      - name: 拉取代码
        uses: actions/checkout@v4

      - name: 编译
        run: |
          cd fireware/board-canbox/rt-thread/bsp/hc32/eric820canbox
          export RTT_CC=gcc
          export RTT_EXEC_PATH=/usr/bin
          # 确保 packages 已就绪
          scons -j$(nproc)
          arm-none-eabi-objcopy -O ihex rtthread.elf canbox_hc32.hex
          arm-none-eabi-size rtthread.elf

      - name: 上传固件
        uses: actions/upload-artifact@v3
        with:
          name: canbox_hc32
          path: fireware/board-canbox/rt-thread/bsp/hc32/eric820canbox/canbox_hc32.hex

  build-compute:
    name: STM32 主机
    runs-on: ubuntu-latest
    steps:
      - name: 拉取代码
        uses: actions/checkout@v4

      - name: 编译
        run: |
          cd fireware/board-compute/rt-thread/bsp/stm32/eric820calcnewcan
          export RTT_CC=gcc
          export RTT_EXEC_PATH=/usr/bin
          scons -j$(nproc)
          arm-none-eabi-objcopy -O ihex rt-thread.elf compute_stm32f429.hex
          arm-none-eabi-size rt-thread.elf

      - name: 上传固件
        uses: actions/upload-artifact@v3
        with:
          name: compute_stm32f429
          path: fireware/board-compute/rt-thread/bsp/stm32/eric820calcnewcan/compute_stm32f429.hex

  build-fiber:
    name: 光纤模块
    runs-on: ubuntu-latest
    steps:
      - name: 拉取代码
        uses: actions/checkout@v4

      - name: 编译
        run: |
          # 光纤模块需要 Makefile（见下方说明）
          cd extracted_can_bus/Eric_Can_Bus_STM32
          make -j$(nproc)
          arm-none-eabi-size build/Eric_Can_Bus.elf

      - name: 上传固件
        uses: actions/upload-artifact@v3
        with:
          name: fiber_stm32f103
          path: extracted_can_bus/Eric_Can_Bus_STM32/build/fiber_stm32f103.hex
```

### 步骤 5：推送代码，触发编译

```bash
# 在你的 Mac 上
git add .gitea/workflows/build.yaml
git commit -m "feat(ci): 添加固件自动编译流水线"
git push
```

推送后：
1. 打开 Gitea 网页 → 你的仓库 → **Actions** 标签页
2. 你会看到一个正在运行的工作流
3. 绿色 ✅ = 编译成功，红色 ❌ = 编译失败（点进去看日志）
4. 编译成功后，点击工作流详情 → 底部 **Artifacts** 区域 → 下载 .hex 文件

---

## 五、同事如何使用

同事的操作流程（极简）：

```
1. 打开浏览器 → 进入 Gitea 仓库页面
2. 点击 Actions 标签 → 找到最新的绿色构建
3. 下载 Artifacts（三个 .hex 文件）
4. 打开 J-Link Commander 或 J-Flash
5. 选择对应芯片 → 加载 .hex → 烧录
```

同事完全不需要装任何编译环境。

---

## 六、进阶：自动发布 Release

在工作流末尾加一个发布步骤，给 .hex 打上版本标签：

```yaml
  release:
    name: 发布固件
    needs: [build-canbox, build-compute, build-fiber]  # 等三个都编完
    runs-on: ubuntu-latest
    if: startsWith(github.ref, 'refs/tags/v')          # 只在打 tag 时发布
    steps:
      - name: 下载所有固件
        uses: actions/download-artifact@v3

      - name: 创建 Release
        uses: softprops/action-gh-release@v1
        with:
          files: |
            canbox_hc32/*.hex
            compute_stm32f429/*.hex
            fiber_stm32f103/*.hex
          body: |
            ## 固件包
            - canbox_hc32.hex — HC32 采集器
            - compute_stm32f429.hex — STM32 主机
            - fiber_stm32f103.hex — 光纤转CAN模块
```

使用方法：
```bash
# 你在 Mac 上打标签
git tag v1.2.3
git push --tags
# Gitea 自动编译 + 自动发布 Release + 同事去 Release 页面下载
```

---

## 七、需要提前解决的问题

在 CI/CD 能跑之前，有几个前置工作要做：

| 序号 | 任务 | 说明 |
|------|------|------|
| 1 | **光纤模块写 Makefile** | 当前是纯 Keil 工程，CI 环境没有 Keil，需要补一个 GCC Makefile |
| 2 | **确保 scons 工程能 GCC 编译通过** | HC32 和 STM32 的 packages 依赖要完整（执行 `pkgs --update`） |
| 3 | **Runner 环境验证** | 在 Runner 机器上手动跑一次编译，确认工具链版本、依赖都对 |
| 4 | **离线环境适配** | 如果 Gitea 在内网无外网，Runner 的依赖需要离线安装 |

---

## 八、常见问题

**Q: 我的 Gitea 是内网离线的，Runner 怎么装依赖？**
A: 在有网的电脑上下载好 `arm-none-eabi-gcc` 和 `scons`，拷贝到内网 Runner 机器上安装。act_runner 本身是单二进制文件，直接拷贝即可。

**Q: 编译失败了怎么看原因？**
A: Gitea → Actions → 点击失败的工作流 → 展开失败的步骤 → 查看完整编译日志，和你在终端看到的一模一样。

**Q: 能不能在 CI 上用 Keil 编译？**
A: 技术上可以（Runner 用 Windows + 安装 Keil + 命令行调用 `armcc`），但 Keil 是商业授权软件，CI 上使用需要许可证。推荐统一迁移到 GCC，免费且跨平台。

**Q: 改了代码不想触发编译怎么办？**
A: commit message 中加 `[skip ci]` 即可跳过，例如 `git commit -m "docs: 更新文档 [skip ci]"`

**Q: 多个人同时推代码会冲突吗？**
A: 不会。每次 push 触发独立的编译任务，互不影响。最新一次推送的产物覆盖之前的。
