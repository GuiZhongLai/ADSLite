# ADSLite

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-cross--platform-brightgreen.svg)](https://github.com/yourusername/ADSLite)
[![Tested](https://img.shields.io/badge/tested-Beckhoff%20C6015--0010-success)](https://www.beckhoff.com)

:rocket: 一个轻量级、跨平台的ADS通信库，无需安装额外的ADS通信包，直接与倍福(TwinCAT)控制器进行高效通信。

## :sparkles: 特性

- :rocket: **零依赖**：无需安装TwinCAT ADS通信包
- :globe_with_meridians: **跨平台支持**：Windows、Linux、嵌入式系统
- :package: **32位库支持**：兼容各种嵌入式平台
- :mag: **自动AmsNetId获取**：无需手动配置网络标识
- :zap: **高性能**：微秒级通信延迟
- :wrench: **简单易用**：简洁的API接口设计

## :clipboard: 已验证平台

| 平台类型 | 具体型号 | 状态 | 备注 |
|---------|---------|------|------|
| **控制器端** | 倍福PLC C6015-0010 (TwinCAT 3) | :white_check_mark: 已验证 | 稳定运行 |
| **客户端端** | Windows 10 (x86/x64) | :white_check_mark: 已验证 | 完整支持 |
| **客户端端** | ARM64 (瑞芯微RK35xx) | :white_check_mark: 已验证 | 嵌入式优化 |
| **客户端端** | Linux发行版 | :white_check_mark: 已验证 | 通用支持 |

## :rocket: 性能表现

基于1000字节缓冲区压力测试（800次迭代）：

| 测试类型 | 平均耗时(us) | 平均速度(KB/s) | 成功率 | 数据量 |
|---------|-------------|---------------|--------|--------|
| **读取测试** | 958.09 | 1031-1360 | 100% | 800KB |
| **写入测试** | 989.79 | 755-1346 | 100% | 800KB |
| **混合测试** | 914.13 | 875-1328 | 100% | 1.6MB |
| **总计** | **953.67** | **~1100** | **100%** | **2.4MB** |

## :package: 安装

### 从源码编译

```bash
git clone https://github.com/GuiZhongLai/ADSLite.git
cd ADSLite
mkdir build && cd build
cmake ..
make
sudo make install
