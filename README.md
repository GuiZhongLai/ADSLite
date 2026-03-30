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
uint16_t port = AdsLitePortOpen();
if (port == 0) {
    return -1;
}

AdsLiteSyncSetTimeout(port, 2000);

AmsNetId target{};
if (AdsLiteInitRouting("192.168.1.1", &target) != 0) {
    AdsLitePortClose(port);
    return -1;
}

AmsAddr addr{target, AMSPORT_R0_PLC_TC3};

int32_t value = 0;
uint32_t bytesRead = 0;
AdsLiteReadByName(port, &addr, "GVL.nCounter", &value, sizeof(value), &bytesRead);

AdsLitePortClose(port);
AdsLiteShutdownRouting(&target);
```

### 3. 文件服务示例

```cpp
uint16_t port = AdsLitePortOpen();
if (port == 0) {
    return -1;
}

AdsLiteSyncSetTimeout(port, 2000);

AmsNetId target{};
if (AdsLiteInitRouting("192.168.1.1", &target) != 0) {
    AdsLitePortClose(port);
    return -1;
}

AmsAddr addr{target, 851};

const uint32_t writeFlags = ADSLITE_FOPEN_WRITE |
                            ADSLITE_FOPEN_BINARY |
                            ADSLITE_FOPEN_PLUS |
                            ADSLITE_FOPEN_ENSURE_DIR;
const uint32_t readFlags = ADSLITE_FOPEN_READ |
                           ADSLITE_FOPEN_BINARY;

uint32_t handle = 0;
int64_t ret = AdsLiteFileOpen(port,
                              &addr,
                              "C:/test/AdsLite/example_file_api/demo/result.bin",
                              writeFlags,
                              &handle);
if (ret == 0) {
    const uint8_t payload[] = {0x11, 0x22, 0x33};
    AdsLiteFileWrite(port, &addr, handle, payload, static_cast<uint32_t>(sizeof(payload)));
    AdsLiteFileClose(port, &addr, handle);
}

handle = 0;
ret = AdsLiteFileOpen(port,
                      &addr,
                      "C:/test/AdsLite/example_file_api/demo/result.bin",
                      readFlags,
                      &handle);
if (ret == 0) {
    uint8_t buffer[16] = {0};
    uint32_t bytesRead = 0;
    AdsLiteFileRead(port, &addr, handle, sizeof(buffer), buffer, &bytesRead);
    AdsLiteFileClose(port, &addr, handle);
}

char listBuffer[1024] = {0};
uint32_t bytesRequired = 0;
uint32_t itemCount = 0;
AdsLiteFileList(port,
                &addr,
                "C:/test/AdsLite/example_file_api/demo/*",
                listBuffer,
                static_cast<uint32_t>(sizeof(listBuffer)),
                &bytesRequired,
                &itemCount);

AdsLiteShutdownRouting(&target);
AdsLitePortClose(port);
```

### 4. AdsLiteFileOpen 的 openFlags 说明

`AdsLiteFileOpen` 的 `openFlags` 用于描述“以什么方式打开远端文件”。常用组合如下：

- 读取已有文件

```cpp
const uint32_t readFlags = ADSLITE_FOPEN_READ |
                           ADSLITE_FOPEN_BINARY;
```

- 覆盖写入文件，不存在时自动补建父目录

```cpp
const uint32_t writeFlags = ADSLITE_FOPEN_WRITE |
                            ADSLITE_FOPEN_BINARY |
                            ADSLITE_FOPEN_PLUS |
                            ADSLITE_FOPEN_ENSURE_DIR;
```

- 追加写入文件，不覆盖已有内容

```cpp
const uint32_t appendFlags = ADSLITE_FOPEN_APPEND |
                             ADSLITE_FOPEN_BINARY |
                             ADSLITE_FOPEN_PLUS |
                             ADSLITE_FOPEN_ENSURE_DIR;
```

各标志位含义：

- `ADSLITE_FOPEN_READ`：以读方式打开文件
- `ADSLITE_FOPEN_WRITE`：以覆盖写方式打开文件
- `ADSLITE_FOPEN_APPEND`：以追加写方式打开文件
- `ADSLITE_FOPEN_PLUS`：允许同一句柄后续读写组合操作
- `ADSLITE_FOPEN_BINARY`：按二进制模式处理文件
- `ADSLITE_FOPEN_TEXT`：按文本模式处理文件
- `ADSLITE_FOPEN_ENSURE_DIR`：当父目录不存在时，尝试自动补建父目录

使用建议：

- 读文件：`READ | BINARY`
- 写新文件或覆盖部署文件：`WRITE | BINARY | PLUS | ENSURE_DIR`
- 追加日志文件：`APPEND | BINARY | PLUS | ENSURE_DIR`

说明：

- `AdsLiteFileList` 不使用 `openFlags`。它直接通过 `pathPattern` 遍历目录，例如 `"C:/test/AdsLite/example_file_api/demo/*"`
- `AdsLiteFileDelete` 和 `AdsLiteDirDelete` 也不对外暴露 `openFlags`，删除语义由 API 自身封装
- `ADSLITE_FOPEN_ENABLE_DIR` 和 `ADSLITE_FOPEN_SHIFT_OPENPATH` 属于底层文件服务控制位，通常不建议外部业务代码直接传入 `AdsLiteFileOpen`

### 5. 文件夹操作示例

```cpp
uint16_t port = AdsLitePortOpen();
if (port == 0) {
    return -1;
}

AdsLiteSyncSetTimeout(port, 2000);

AmsNetId target{};
if (AdsLiteInitRouting("192.168.1.1", &target) != 0) {
    AdsLitePortClose(port);
    return -1;
}

AmsAddr addr{target, 851};

// 1) 创建多级目录
AdsLiteDirCreate(port,
                 &addr,
                 "C:/test/AdsLite/example_file_api/case_dir_create/l1/l2");

// 2) 删除空目录本身
AdsLiteDirCreate(port,
                 &addr,
                 "C:/test/AdsLite/example_file_api/case_dir_delete");
AdsLiteDirDelete(port,
                 &addr,
                 "C:/test/AdsLite/example_file_api/case_dir_delete",
                 true);

// 3) 递归删除整个目录树
const uint32_t writeFlags = ADSLITE_FOPEN_WRITE |
                            ADSLITE_FOPEN_BINARY |
                            ADSLITE_FOPEN_PLUS |
                            ADSLITE_FOPEN_ENSURE_DIR;

uint32_t handle = 0;
if (AdsLiteFileOpen(port,
                    &addr,
                    "C:/test/AdsLite/example_file_api/r1/r2/r3/delete_recursive.bin",
                    writeFlags,
                    &handle) == 0) {
    const uint8_t payload[] = {0x01, 0x02, 0x03};
    AdsLiteFileWrite(port, &addr, handle, payload, static_cast<uint32_t>(sizeof(payload)));
    AdsLiteFileClose(port, &addr, handle);
}
AdsLiteDirDelete(port,
                 &addr,
                 "C:/test/AdsLite/example_file_api",
                 true);

// 4) 仅清空目录内容，保留根目录本身
AdsLiteDirCreate(port,
                 &addr,
                 "C:/test/AdsLite/example_file_api/reg_keep_root");

handle = 0;
if (AdsLiteFileOpen(port,
                    &addr,
                    "C:/test/AdsLite/example_file_api/reg_keep_root/l1/l2/data.bin",
                    writeFlags,
                    &handle) == 0) {
    const uint8_t payload[] = {0xAA, 0xBB, 0xCC};
    AdsLiteFileWrite(port, &addr, handle, payload, static_cast<uint32_t>(sizeof(payload)));
    AdsLiteFileClose(port, &addr, handle);
}

AdsLiteDirDelete(port,
                 &addr,
                 "C:/test/AdsLite/example_file_api/reg_keep_root",
                 false);

AdsLiteShutdownRouting(&target);
AdsLitePortClose(port);
```

目录接口的语义区别：

- `AdsLiteDirCreate`：保证目录存在，可用于预创建多级目录
- `AdsLiteDirDelete(..., true)`：删除目录内容，并删除目录本身
- `AdsLiteDirDelete(..., false)`：只清空目录内容，保留目录本身

使用建议：

- 部署前预建目录：直接调用 `AdsLiteDirCreate`
- 清理整棵目录树：调用 `AdsLiteDirDelete(path, true)`
- 保留根目录结构，只删除内部产物：调用 `AdsLiteDirDelete(path, false)`

### 6. 控制后端（可选）

```powershell
$env:ADSLITE_BACKEND = "twincat"     # 或 standalone / auto
```

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
