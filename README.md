# ADSLite

ADSLite 是一个轻量级 ADS 通信库，提供统一 C API，并在运行时按环境自动选择后端：

- `standalone` 后端：使用项目内 `standalone/` 协议栈。
- `twincat` 后端：在 Windows 下通过 `TcAdsDll` 导入库调用 TwinCAT ADS API。

## 主要能力

- 统一 API：`AdsLiteAPI.h`
- 路由辅助：`AdsLiteInitRouting` / `AdsLiteShutdownRouting`
- 同步读写：`AdsLiteSyncReadReq` / `AdsLiteSyncWriteReq` / `AdsLiteSyncReadWriteReq`
- 变量按名/按句柄读写
- 运行时后端自动选择与强制覆盖

## 后端选择规则

- 环境变量 `ADSLITE_BACKEND`：`auto`(默认) / `standalone` / `twincat`
- `auto` 模式：
- Windows 下检测 `48898` 监听者为 `TCATSysSrv.exe` 且 TwinCAT 后端可用时，选择 `twincat`
- 否则选择 `standalone`
- 非 Windows：仅 `standalone`

## 构建

### 通用构建

```powershell
mkdir build
cd build
cmake ..
cmake --build .
```

### Windows + TcAdsDll 构建说明

项目当前使用仓库内固定相对路径查找 TwinCAT SDK 文件：

- 头文件：`TcAdsDll/Include/TcAdsAPI.h`
- 32 位导入库：`TcAdsDll/Lib/TcAdsDll.lib`
- 64 位导入库：`TcAdsDll/x64/lib/TcAdsDll.lib`

`CMakeLists.txt` 中的行为：

- 选项 `ADSLITE_ENABLE_TWINCAT` 默认 `ON`
- 若头文件和对应位数导入库都存在，编译宏 `ADSLITE_TWINCAT_ENABLED=1`
- 否则自动降级为 `ADSLITE_TWINCAT_ENABLED=0`（仅 standalone）

可用如下命令显式控制：

```powershell
cmake -S . -B build -DADSLITE_ENABLE_TWINCAT=ON
cmake --build build
```

## TcAdsDll 的安装与运行依赖

### 编译期检查

满足以下任一位数组合即可启用 TwinCAT 后端：

- x86: `TcAdsDll/Include/TcAdsAPI.h` + `TcAdsDll/Lib/TcAdsDll.lib`
- x64: `TcAdsDll/Include/TcAdsAPI.h` + `TcAdsDll/x64/lib/TcAdsDll.lib`

### 运行期检查

启用 TwinCAT 后端时，进程运行需要可加载 `TcAdsDll.dll`。

建议方式：

- 将匹配位数的 `TcAdsDll.dll` 放到可执行文件同目录
- 或将 DLL 所在目录加入 `PATH`
- 或确保 TwinCAT 安装目录在系统 DLL 搜索路径中

如果运行期找不到 DLL，Windows 会在加载时失败（即使编译和链接通过）。

## 使用者如何接入

### 1. 引入头文件

```cpp
#include "AdsLiteAPI.h"
#include "AdsLiteDef.h"
```

### 2. 初始化与通信

```cpp
AmsNetId target{};
if (AdsLiteInitRouting("192.168.1.1", &target) != 0) {
    return -1;
}

uint16_t port = AdsLitePortOpen();
AmsAddr addr{target, AMSPORT_R0_PLC_TC3};

int32_t value = 0;
uint32_t bytesRead = 0;
AdsLiteReadByName(port, &addr, "GVL.nCounter", &value, sizeof(value), &bytesRead);

AdsLitePortClose(port);
AdsLiteShutdownRouting(&target);
```

### 3. 控制后端（可选）

```powershell
$env:ADSLITE_BACKEND = "twincat"     # 或 standalone / auto
```

## API 说明（摘要）

- 设备识别：`AdsLiteGetDeviceNetId`（统一走 standalone 发现逻辑）
- 路由管理：`AdsLiteInitRouting` / `AdsLiteShutdownRouting`
- 端口管理：`AdsLitePortOpen` / `AdsLitePortClose`
- 同步请求：`AdsLiteSyncReadReq` / `AdsLiteSyncWriteReq` / `AdsLiteSyncReadWriteReq`
- 状态控制：`AdsLiteSyncReadStateReq` / `AdsLiteSyncWriteControlReq`
- 变量读写：`AdsLiteReadByName` / `AdsLiteWriteByName` / `AdsLiteReadByHandle` / `AdsLiteWriteByHandle`


## 项目结构

```text
AdsLiteAPI.h
AdsLiteAPI.cpp
AdsLiteDef.h
backend/
standalone/
TcAdsDll/
example/
CMakeLists.txt
```

## 许可证

MIT，见 `LICENSE`。
