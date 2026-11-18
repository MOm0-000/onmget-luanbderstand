# Net-Lab：从零实现简化 TCP/IP 协议栈

> 使用 **C 语言 + libpcap** 构建一个用于计算机网络教学与实验的用户态协议栈，循序实现 Ethernet、ARP、IPv4、ICMP、UDP、TCP，并在此基础上尝试构建简单的 HTTP 服务。

[![Language](https://img.shields.io/badge/language-C-blue)](https://github.com/MOm0-000/onmget-luanbderstand)
[![Build](https://img.shields.io/badge/build-CMake-brightgreen)](CMakeLists.txt)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey)](#环境要求)

**仓库地址：** [MOm0-000/onmget-luanbderstand](https://github.com/MOm0-000/onmget-luanbderstand)  
**实验指导书：** <https://comp-network.p.cs-lab.top/>

> **项目状态说明：** 本仓库是一份持续完善的教学实验代码，**并非可直接用于生产环境的完整 TCP/IP 协议栈**。Ethernet、ARP、IPv4、ICMP、UDP 中已有主要处理逻辑；**TCP 及 HTTP 相关实现仍含 TODO**，因此不能将 TCP Echo / Web Server 视为当前已验证可用的功能。具体以源码和测试结果为准。

## 目录

- [项目概览](#项目概览)
- [分层架构](#分层架构)
- [项目目录](#项目目录)
- [协议实现与开发进度](#协议实现与开发进度)
- [环境要求](#环境要求)
- [快速开始（Ubuntu）](#快速开始ubuntu)
- [运行示例与网络配置](#运行示例与网络配置)
- [测试与验证](#测试与验证)
- [核心数据流](#核心数据流)
- [常见问题与注意事项](#常见问题与注意事项)
- [参考资料](#参考资料)

## 项目概览

本项目不是对系统 Socket API 的简单封装，而是通过 `libpcap` / `Npcap` **接收和发送原始以太网帧**，在用户态完成报文解析、封装、协议分发和应用层回调。适用于理解网络协议分层、报文格式、校验和、地址解析及网络调试流程。

主要特点：

- **分层组织：** 按以太网、网络层、传输层和应用层划分代码。
- **收发可追踪：** 每个协议模块提供清晰的报文处理入口。
- **可替换的驱动：** 正常运行时使用 `src/driver.c` 对接网卡；离线测试时使用 `testing/faker/driver.c` 读取和写入 PCAP 文件。
- **统一缓冲区：** 利用 `buf_t` 在报文头部插入/移除协议头，在尾部补齐数据。
- **统一映射表：** 利用 `map_t` 保存协议处理函数、ARP 映射以及 UDP/TCP 相关状态。
- **实验可重复：** CMake + CTest 配置了多组组件级测试，便于逐步完成实验并核对抓包结果。

## 分层架构

```text
                    应用程序（app/）
         ┌────────────┬────────────┬────────────┐
         │ UDP Echo   │ TCP Echo   │ HTTP Server│
         └──────┬─────┴─────┬──────┴─────┬──────┘
                │           │            │
         ┌──────▼───────────▼────────────▼──────┐
         │            传输层                  │
         │      UDP（udp.c）/ TCP（tcp.c）      │
         └──────────────────┬──────────────────┘
                            │
         ┌──────────────────▼──────────────────┐
         │              网络层                  │
         │  IPv4（ip.c） + ICMP（icmp.c）       │
         │        ARP 地址解析（arp.c）          │
         └──────────────────┬──────────────────┘
                            │
         ┌──────────────────▼──────────────────┐
         │           数据链路层                 │
         │          Ethernet（ethernet.c）      │
         └──────────────────┬──────────────────┘
                            │
         ┌──────────────────▼──────────────────┐
         │            网卡驱动层                │
         │         libpcap / Npcap              │
         └──────────────────┬──────────────────┘
                            │
                    物理/虚拟网络接口
```

`net.c` 负责协议栈初始化、协议处理函数注册及分发，`net_poll()` 负责推动轮询接收；`buf.c`、`map.c`、`utils.c` 为各层提供通用能力。

> **实现范围提示：** 上述图示表示项目的设计结构，并不代表所有分层功能已完整实现。特别是 TCP 与 HTTP 模块尚未完成。

## 项目目录

```text
net-lab/
├── CMakeLists.txt          # CMake 构建与 CTest 测试配置
├── Makefile                # build / run / clean / format 等便捷命令
├── setup_veth.sh           # Linux veth 网络环境配置脚本
├── .clang-format           # C 代码格式规范
├── .gitattributes          # Git 文本换行与二进制文件属性配置
├── include/                # 协议结构体、常量、接口定义
│   ├── config.h            # 实验 IP/MAC、MTU 等参数
│   ├── net.h               # 协议注册、初始化和分发
│   ├── driver.h            # 驱动抽象接口
│   ├── ethernet.h
│   ├── arp.h
│   ├── ip.h
│   ├── icmp.h
│   ├── udp.h
│   ├── tcp.h
│   ├── buf.h
│   ├── map.h
│   └── utils.h
├── src/                    # 协议栈核心实现
│   ├── driver.c            # libpcap / Npcap 实际网卡驱动
│   ├── net.c               # 协议栈总入口与分发
│   ├── ethernet.c          # 以太网帧收发
│   ├── arp.c               # ARP 解析、缓存与请求/应答
│   ├── ip.c                # IPv4 收发、校验与发送分片
│   ├── icmp.c              # ICMP 回显和不可达报文
│   ├── udp.c               # UDP 收发与端口回调
│   ├── tcp.c               # TCP 连接状态与报文处理（未完成）
│   ├── buf.c               # 通用报文缓冲区
│   ├── map.c               # 通用映射表
│   └── utils.c             # 地址格式化、校验和等工具
├── app/                    # 上层示例
│   ├── udp_server.c        # UDP Echo，端口 60000
│   ├── tcp_server.c        # TCP Echo，端口 60000（依赖未完成的 TCP）
│   ├── web_server.c        # HTTP 服务，端口 80（仍有 TODO）
│   └── resource/           # HTML/CSS/图片等静态资源
├── testing/                # 协议单元/组件测试与输入数据
│   ├── faker/              # 模拟协议及离线 PCAP 驱动
│   ├── data/               # 各项测试的输入、参考输出
│   └── *_test.c            # 对应协议测试程序
├── RESULT/                 # 已保存的实验截图和结果
├── Npcap/                  # Windows 平台 Npcap 头文件与库
├── build/                  # 构建结果（仓库中已有部分生成文件）
└── Testing/                # CTest 产生的测试记录
```

## 协议实现与开发进度

以下判断依据是仓库中**当前可见源码**，不是通过完整端到端测试后作出的性能或正确性保证。

| 模块 | 主要代码 | 当前可见实现情况 |
| --- | --- | --- |
| Ethernet | `src/ethernet.c` | 已实现帧头解析、协议分发、帧封装及最小载荷填充 |
| ARP | `src/arp.c` | 已实现请求/应答、IP→MAC 映射、待发送报文缓存 |
| IPv4 | `src/ip.c` | 已实现基本报头检查、校验和、上层分发及**发送方向分片**；接收方向未实现分片重组 |
| ICMP | `src/icmp.c` | 已实现 Echo Reply 与部分目的不可达响应 |
| UDP | `src/udp.c` | 已实现报文收发、伪首部校验和、端口回调、端口不可达处理 |
| TCP | `src/tcp.c` | 有连接表和状态机框架；**`tcp_out()` 与多个状态转移/数据交付步骤仍为 TODO** |
| HTTP | `app/web_server.c` | 有资源查找、MIME 判断与请求处理框架；**响应头和响应体发送仍为 TODO** |

### 重要实现细节

- `net_add_protocol()` / `net_in()` 根据协议编号登记和分发上层处理函数。
- 入口程序使用单线程 `while (1) { net_poll(); }` 轮询数据；不是基于多线程或内核 Socket 的服务。
- 发送 IPv4 报文时会根据 MTU 进行分片；**接收端会丢弃需要分片重组的 IPv4 包**。
- UDP 示例在 **60000** 端口收取数据，并将原始数据回发给发送方。
- TCP 和 HTTP 相关目标可以参与编译，但**“能编译”不意味着“功能已经实现”**。

## 环境要求

推荐 **Ubuntu / Linux**；项目也包含针对 Windows Npcap 的兼容代码和依赖文件，但本 README 主要给出 Ubuntu 下的操作步骤。

- C 编译器（例如 GCC）和 Make
- CMake（源码声明最低版本为 3.0.0）
- `libpcap` 开发库（Ubuntu：`libpcap-dev`）
- `iproute2`（提供 `ip` 命令）和 `ethtool`（配置 veth 网卡特性）
- 可选：`netcat-openbsd`、`curl`、Wireshark / `tcpdump`、`clang-format`
- 使用真实/虚拟网卡抓包时，需具备相应系统权限

Ubuntu 可安装：

```bash
sudo apt update
sudo apt install -y build-essential cmake libpcap-dev iproute2 ethtool \
  netcat-openbsd curl tcpdump
```

## 快速开始（Ubuntu）

### 1. 克隆仓库

已配置 GitHub SSH 密钥的情况下：

```bash
git clone git@github.com:MOm0-000/onmget-luanbderstand.git
cd onmget-luanbderstand
```

如果已经在本地目录 `/home/zxx/internet/net-lab` 中开发，则直接进入该目录即可：

```bash
cd /home/zxx/internet/net-lab
```

### 2. 编译

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

或使用仓库内提供的快捷命令：

```bash
make build
```

> **注意：** 仓库当前包含历史 `build/` 构建产物。如果 CMake 报出缓存目录与当前机器路径不匹配，请在**确认无需保留本地构建结果后**先删除旧构建目录，再重新配置：
>
> ```bash
> rm -rf build
> cmake -S . -B build
> cmake --build build -j"$(nproc)"
> ```

构建配置会生成以下程序：

| 类别 | 可执行程序 |
| --- | --- |
| 示例应用 | `build/udp_server`、`build/tcp_server`、`build/web_server` |
| 组件测试 | `build/eth_in`、`build/eth_out`、`build/arp_test`、`build/ip_test`、`build/ip_frag_test`、`build/icmp_test`、`build/udp_test`、`build/tcp_test` |

### 3. 运行离线测试（建议先做）

```bash
ctest --test-dir build -N
ctest --test-dir build --output-on-failure
```

CTest 使用 `testing/data/` 内的测试数据和 `testing/faker/driver.c` 提供的离线 PCAP 驱动，**不需要先创建 veth 接口**。由于项目仍有未完成模块，测试不一定全部通过；以上命令只代表运行方式，不代表本仓库已通过全部测试。

## 运行示例与网络配置

### 1. 创建 Linux veth 虚拟网络

仓库的 `setup_veth.sh` 使用以下默认配置：

| 对象 | 地址或标识 |
| --- | --- |
| 用户态协议栈 IP | `192.168.254.2` |
| 用户态协议栈 MAC | `00:11:22:33:44:55` |
| `veth0` | `192.168.254.3/24` |
| `veth1` | `192.168.254.1/24` |
| 测试子网 | `192.168.254.0/24` |

先确保地址范围没有与真实网络冲突，再在**实验环境**执行：

```bash
sudo bash setup_veth.sh
ip addr show veth0
ip addr show veth1
```

脚本会创建/重建 `veth0` 与 `veth1`、调整相应地址和路由，并尝试关闭校验和卸载功能。这些是**主机网络配置更改**，在共享服务器或已有重要路由的机器上不要直接执行。

其中协议栈 IP/MAC 在 `include/config.h` 中定义；如果修改了协议栈 IP，记得同步调整脚本配置并重新编译。

### 2. UDP Echo 示例

在终端 A 中运行：

```bash
sudo ./build/udp_server
```

在终端 B 中向协议栈发送 UDP 数据：

```bash
printf 'hello net-lab\n' | nc -u -w 2 192.168.254.2 60000
```

UDP 服务端通过 `udp_open(60000, ...)` 注册处理函数，收到报文后调用 `udp_send()` 回送数据。若未收到响应，可结合 `ip route`、ARP、抓包与日志检查。

### 3. TCP / HTTP 示例（待完成）

项目定义了以下入口：

```bash
# TCP Echo：60000 端口（依赖待实现的 TCP 输出/状态机逻辑）
sudo ./build/tcp_server

# HTTP：80 端口（依赖待实现的 TCP 和 HTTP 响应逻辑）
sudo ./build/web_server
```

以上仅用于标示入口程序，**当前版本不保证能够完成 TCP 握手或正确返回网页**。在补全 `src/tcp.c` 和 `app/web_server.c` 的 TODO 后，可再使用 `nc` / `curl` 进行端到端验证。

### 4. 网络抓包

可在另一个终端运行：

```bash
sudo tcpdump -i veth0 -n -e 'arp or icmp or udp or tcp'
```

或使用 Wireshark 观察 Ethernet、ARP、IPv4、UDP/TCP 报文。实验结束后，如果不再需要虚拟网卡，可以执行 `sudo ip link del veth0` 删除该 veth 对（会同时删除其 peer）。

## 测试与验证

`CMakeLists.txt` 定义了以下 **8 项 CTest 测试**：

| 测试名 | 对应重点 |
| --- | --- |
| `eth_in` | 以太网帧输入与分发 |
| `eth_out` | 以太网帧输出与封装 |
| `arp_test` | ARP 缓存、请求与应答 |
| `ip_test` | IPv4 输入输出与协议分发 |
| `ip_frag_test` | IPv4 **发送方向**分片 |
| `icmp_test` | ICMP 回显及差错报文 |
| `udp_test` | UDP 报文解析、校验和与发送 |
| `tcp_test` | TCP 模块（当前仍有 TODO） |

运行单项测试：

```bash
ctest --test-dir build -R '^udp_test$' --output-on-failure
```

或直接给测试程序传入数据目录：

```bash
./build/udp_test testing/data/udp_test
```

离线测试会读取相应目录中的 `in.pcap`、`demo_out.pcap`、`demo_log` 等参考文件，并可能写入 `out.pcap`、`log` 等文件。因此测试后出现工作区文件变化属于可能的正常现象，提交前请检查 `git status`。

## 核心数据流

**接收路径：**

```text
网卡 / veth
  └── driver_recv()                 [src/driver.c]
       └── ethernet_in()            [src/ethernet.c]
            └── net_in()            [src/net.c：按协议号分发]
                 ├── arp_in()       [src/arp.c]
                 └── ip_in()        [src/ip.c]
                      └── net_in()  [按 IP 上层协议号分发]
                           ├── icmp_in()
                           ├── udp_in() → UDP 端口回调
                           └── tcp_in() → TCP 连接/应用回调（未完成）
```

**发送路径（以 UDP 为例）：**

```text
应用数据
  └── udp_send() / udp_out()
       └── ip_out()                 [添加 IPv4 头；必要时发送分片]
            └── arp_out()           [查 IP→MAC 映射；必要时请求 ARP]
                 └── ethernet_out() [封装以太网帧]
                      └── driver_send()
                           └── 网卡 / veth
```

这也是阅读代码时推荐的顺序：先看 `net.c`、`driver.c` 和 `ethernet.c`，再按 `arp.c → ip.c → icmp.c / udp.c → tcp.c` 逐步理解。

## 常见问题与注意事项

1. **`pcap.h: No such file or directory`**：通常缺少 `libpcap-dev`；安装后重新配置并编译。
2. **`Error in driver find` / 无法选择网卡**：检查 `setup_veth.sh` 是否成功执行，`veth0` / `veth1` 是否存在，以及 `include/config.h` 的协议栈 IP 是否与虚拟网络匹配。
3. **权限不足无法打开网卡**：实验环境中可使用 `sudo` 运行示例程序；避免在不可信网络直接抓取或注入报文。
4. **CMake 缓存路径冲突**：仓库中已经包含 `build/`；清理旧构建产物后重新生成（参见上方编译步骤）。
5. **TCP/HTTP 运行但没有预期响应**：优先查看 `src/tcp.c` 的 `tcp_out()` 和状态处理 TODO，以及 `app/web_server.c` 中 HTTP 响应相关 TODO。
6. **`make diff` 失败**：当前 `Makefile` 的 `diff` 目标引用了 `tools/export_patch.cmake`，但当前仓库文件树中没有该脚本；该命令暂不可直接使用。
7. **提交前清理生成文件**：`build/`、`Testing/Temporary/` 以及测试生成的 `log` / `out.pcap` 通常不需要持续纳入版本控制。可根据实验要求配置 `.gitignore`；对**已被 Git 跟踪**的文件，单独添加忽略规则不会解除跟踪。请勿误删参考数据 `demo_*` 和输入文件 `in.pcap`。
8. **功能与安全边界**：网络栈和 HTTP 服务主要用于教学；当前代码不具备完整的 TCP 可靠传输、IPv4 分片接收重组或生产级 HTTP 请求校验等保障，不建议暴露到公共网络。

## 参考资料

- [计算机网络实验指导书](https://comp-network.p.cs-lab.top/)
- [libpcap 项目](https://www.tcpdump.org/)
- [CMake 文档](https://cmake.org/documentation/)
- [Wireshark](https://www.wireshark.org/)

---

本 README 依据仓库当前目录、构建脚本和源码编写；**未在实际 Ubuntu/veth 环境中执行编译或测试**。如后续补全 TCP/HTTP 实现，建议同步更新上面的实现状态与验证说明。
