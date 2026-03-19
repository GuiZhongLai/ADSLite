/**
 * @file main.cpp
 * @brief AdsLite API 测试套件 - 包含基本功能测试、数组读写测试和性能测试
 *
 * 本文件演示了 AdsLite 库的各种功能测试：
 * 1. 基础通信测试（端口打开、路由初始化）
 * 2. 内存读写测试（通过索引组/偏移访问PLC内存）
 * 3. 符号名读写测试（通过变量名访问PLC变量）
 * 4. 句柄读写测试（获取变量句柄后进行高效读写）
 * 5. 设备状态读取
 * 6. 数组读写测试（32字节和256字节数组）
 * 7. 性能测试（测量各操作的响应时间）
 *
 * 使用方法：
 *   - 修改 TEST_REMOTE_ADDR 和 TEST_PORT 配置目标PLC
 *   - 确保测试变量已定义在PLC中：GVL.TestVar (INT), GVL.Perf_32B (ARRAY [0..31] OF BYTE), GVL.Perf_256B (ARRAY [0..255] OF BYTE)
 *   - 编译运行：.\example.exe
 */

#include "AdsLiteAPI.h"
#include "standalone/AmsNetId.h"

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <thread>

// ============================================================================
// 配置参数 - 请根据实际环境修改
// ============================================================================

/// 目标PLC的IP地址
static const char *TEST_REMOTE_ADDR = "192.168.11.88";

/// PLC运行时端口（TC3默认851，TC2默认801）
static const uint16_t TEST_PORT = 851;

/// 测试用内存区域索引组（0x4020 = PLC内存区域）
static const uint32_t INDEX_GROUP = 0x4020;

/// 测试用内存偏移地址 (PLC内存区域起始地址)
static const uint32_t OFFSET = 1000;

/// 性能测试迭代次数
static const int PERF_ITERATIONS = 10;

/// 32字节数组大小
static const size_t SIZE_32B = 32;

/// 256字节数组大小
static const size_t SIZE_256B = 256;

/// LREAL浮点数数组大小 (33个元素 × 8字节 = 264字节)
static const size_t SIZE_32F = 33 * 8;

// ============================================================================
// 全局变量
// ============================================================================

/// ADS通信端口号
static uint16_t g_port = 0;

/// 目标设备地址
static AmsAddr g_targetAddr;

/**
 * @brief 打印性能测试结果
 * @param name   测试名称
 * @param avgMs  平均耗时（毫秒）
 * @param minMs  最小耗时（毫秒）
 * @param maxMs  最大耗时（毫秒）
 */
void PrintPerfResult(const char *name, double avgMs, double minMs, double maxMs)
{
    std::cout << "  " << std::left << std::setw(24) << name
              << "avg=" << std::fixed << std::setprecision(3) << avgMs << "ms  "
              << "min=" << minMs << "ms  "
              << "max=" << maxMs << "ms" << std::endl;
}

// ============================================================================
// 测试用例
// ============================================================================

/**
 * @brief 测试1：ADS端口打开和路由初始化
 *
 * 测试流程：
 *   1. 打开ADS通信端口
 *   2. 初始化到目标设备的路由
 *   3. 设置通信超时时间
 *
 * @return 测试通过返回true，失败返回false
 */
bool TestPortAndRouting()
{
    std::cout << "\n========== Test 1: Port Open and Routing ==========" << std::endl;

    // Step 1: Open ADS port
    g_port = AdsLitePortOpen();
    if (g_port == 0)
    {
        std::cerr << "  [FAIL] Cannot open ADS port" << std::endl;
        return false;
    }
    std::cout << "  [PASS] Port: " << g_port << std::endl;

    // Step 2: Initialize routing
    AmsNetId remoteNetId = {{0}};
    int64_t ret = AdsLiteInitRouting(TEST_REMOTE_ADDR, &remoteNetId);
    if (ret != 0)
    {
        std::cerr << "  [FAIL] Routing error: " << ret << std::endl;
        AdsLitePortClose(g_port);
        return false;
    }
    std::cout << "  [PASS] Routing initialized" << std::endl;

    // Step 3: Set target address and timeout
    g_targetAddr.netId = remoteNetId;
    g_targetAddr.port = TEST_PORT;
    AdsLiteSyncSetTimeout(g_port, 1000); // Set 1 second timeout

    std::cout << "  [PASS] Test passed" << std::endl;
    return true;
}

/**
 * @brief 测试2：通过索引组/偏移读取PLC内存
 *
 * 使用 AdsLiteSyncReadReq 函数直接访问PLC内存区域。
 * 适用于访问没有符号名的内存区域。
 *
 * @return 测试通过返回true，失败返回false
 */
bool TestMemoryRead()
{
    std::cout << "\n========== Test 2: Memory Read (SyncReadReq) ==========" << std::endl;

    std::vector<uint8_t> buffer(4);
    uint32_t bytesRead = 0;

    int64_t ret = AdsLiteSyncReadReq(g_port, &g_targetAddr,
                                     INDEX_GROUP, OFFSET, 4,
                                     buffer.data(), &bytesRead);
    if (ret == 0)
    {
        std::cout << "  [PASS] Read " << bytesRead << " bytes: ";
        for (size_t i = 0; i < bytesRead; i++)
            std::cout << std::hex << (int)buffer[i] << " ";
        std::cout << std::dec << std::endl;
        return true;
    }
    std::cerr << "  [FAIL] Read error: " << ret << std::endl;
    return false;
}

/**
 * @brief 测试3：通过索引组/偏移写入PLC内存
 *
 * 使用 AdsLiteSyncWriteReq 函数直接写入PLC内存区域。
 *
 * @return 测试通过返回true，失败返回false
 */
bool TestMemoryWrite()
{
    std::cout << "\n========== Test 3: Memory Write (SyncWriteReq) ==========" << std::endl;

    // Delay to avoid InvokeId conflict with previous read
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Write test data
    std::vector<uint8_t> buffer = {0x11, 0x22, 0x33, 0x44};

    int64_t ret = AdsLiteSyncWriteReq(g_port, &g_targetAddr,
                                      INDEX_GROUP, OFFSET, 4,
                                      buffer.data());
    if (ret == 0)
    {
        std::cout << "  [PASS] Write 4 bytes: 11 22 33 44" << std::endl;
        return true;
    }
    std::cerr << "  [FAIL] Write error: " << ret << std::endl;
    return false;
}

/**
 * @brief 测试4：通过变量名读取数据
 *
 * 使用 AdsLiteReadByName 函数，通过符号名直接读取PLC变量。
 * 适用于读取单个变量或小型数据结构。
 *
 * 前提条件：PLC中需存在变量 GVL.TestVar (INT类型)
 *
 * @return 测试通过返回true，失败返回false
 */
bool TestReadByName()
{
    std::cout << "\n========== Test 4: Read By Name (ReadByName) ==========" << std::endl;
    std::cout << "  Variable: GVL.TestVar (INT = 2 bytes)" << std::endl;

    std::vector<uint8_t> buffer(2);
    uint32_t bytesRead = 0;

    int64_t ret = AdsLiteReadByName(g_port, &g_targetAddr,
                                    "GVL.TestVar",
                                    buffer.data(), 2,
                                    &bytesRead);
    if (ret == 0)
    {
        // Convert bytes to INT16
        int16_t value = *reinterpret_cast<int16_t *>(buffer.data());
        std::cout << "  [PASS] Value: " << value << std::endl;
        return true;
    }
    std::cerr << "  [FAIL] Error: " << ret << " (variable not found or inaccessible)" << std::endl;
    return false;
}

/**
 * @brief 测试5：通过变量名写入数据
 *
 * 使用 AdsLiteWriteByName 函数，通过符号名直接写入PLC变量。
 *
 * 前提条件：PLC中需存在变量 GVL.TestVar (INT类型)
 *
 * @return 测试通过返回true，失败返回false
 */
bool TestWriteByName()
{
    std::cout << "\n========== Test 5: Write By Name (WriteByName) ==========" << std::endl;
    std::cout << "  Variable: GVL.TestVar (INT = 2 bytes)" << std::endl;

    int16_t value = 9999;
    std::vector<uint8_t> buffer(2);
    buffer[0] = static_cast<uint8_t>(value & 0xFF);        // Low byte
    buffer[1] = static_cast<uint8_t>((value >> 8) & 0xFF); // High byte

    int64_t ret = AdsLiteWriteByName(g_port, &g_targetAddr,
                                     "GVL.TestVar",
                                     buffer.data(), 2);
    if (ret == 0)
    {
        std::cout << "  [PASS] Value: " << value << std::endl;
        return true;
    }
    std::cerr << "  [FAIL] Error: " << ret << " (variable not found or inaccessible)" << std::endl;
    return false;
}

/**
 * @brief 测试6：通过句柄读取数据
 *
 * 使用 AdsLiteGetSymbolHandle 获取变量句柄，然后用句柄读取数据。
 * 适用于需要频繁读取同一变量的场景（比按名读取更高效）。
 *
 * 流程：
 *   1. 获取变量句柄
 *   2. 用句柄读取数据
 *   3. 释放句柄
 *
 * @return 测试通过返回true，失败返回false
 */
bool TestReadByHandle()
{
    std::cout << "\n========== Test 6: Read By Handle (ReadByHandle) ==========" << std::endl;

    // Step 1: Get handle
    uint32_t handle = 0;
    int64_t ret = AdsLiteGetSymbolHandle(g_port, &g_targetAddr,
                                         "GVL.TestVar", &handle);
    if (ret != 0)
    {
        std::cerr << "  [FAIL] Get handle error: " << ret << std::endl;
        return false;
    }
    std::cout << "  [PASS] Handle: " << handle << std::endl;

    // Step 2: Read data using handle
    std::vector<uint8_t> buffer(2);
    uint32_t bytesRead = 0;
    ret = AdsLiteReadByHandle(g_port, &g_targetAddr, handle,
                              buffer.data(), 2, &bytesRead);
    if (ret == 0)
    {
        int16_t value = *reinterpret_cast<int16_t *>(buffer.data());
        std::cout << "  [PASS] Value: " << value << std::endl;
    }
    else
    {
        std::cerr << "  [FAIL] Read error: " << ret << std::endl;
    }

    // Step 3: Release handle
    AdsLiteReleaseSymbolHandle(g_port, &g_targetAddr, handle);

    return (ret == 0);
}

/**
 * @brief 测试7：通过句柄写入数据
 *
 * 使用句柄写入数据，比按名称写入更高效。
 *
 * @return 测试通过返回true，失败返回false
 */
bool TestWriteByHandle()
{
    std::cout << "\n========== Test 7: Write By Handle (WriteByHandle) ==========" << std::endl;

    // Step 1: Get handle
    uint32_t handle = 0;
    int64_t ret = AdsLiteGetSymbolHandle(g_port, &g_targetAddr,
                                         "GVL.TestVar", &handle);
    if (ret != 0)
    {
        std::cerr << "  [FAIL] Get handle error: " << ret << std::endl;
        return false;
    }
    std::cout << "  [PASS] Handle: " << handle << std::endl;

    // Step 2: Write data using handle
    int16_t value = 11111;
    std::vector<uint8_t> buffer(2);
    buffer[0] = static_cast<uint8_t>(value & 0xFF);
    buffer[1] = static_cast<uint8_t>((value >> 8) & 0xFF);

    ret = AdsLiteWriteByHandle(g_port, &g_targetAddr, handle,
                               buffer.data(), 2);
    if (ret == 0)
    {
        std::cout << "  [PASS] Value: " << value << std::endl;
    }
    else
    {
        std::cerr << "  [FAIL] Write error: " << ret << std::endl;
    }

    // Step 3: Release handle
    AdsLiteReleaseSymbolHandle(g_port, &g_targetAddr, handle);

    return (ret == 0);
}

/**
 * @brief 测试8：读取ADS设备状态
 *
 * 使用 AdsLiteSyncReadStateReq 读取ADS设备的状态信息。
 *
 * 返回值说明：
 *   - ADS State: 0=无效, 1=空闲, 4=运行, 5=停止等
 *   - Device State: 设备特定状态
 *
 * @return 测试通过返回true，失败返回false
 */
bool TestReadState()
{
    std::cout << "\n========== Test 8: Read Device State (ReadState) ==========" << std::endl;

    uint16_t adsState = 0, deviceState = 0;

    int64_t ret = AdsLiteSyncReadStateReq(g_port, &g_targetAddr,
                                          &adsState, &deviceState);
    if (ret == 0)
    {
        std::cout << "  [PASS] ADS State: " << adsState
                  << ", Device State: " << deviceState << std::endl;
        std::cout << "  Note: ADS State 5=Run" << std::endl;
        return true;
    }
    std::cerr << "  [FAIL] Read state error: " << ret << std::endl;
    return false;
}

// ============================================================================
// 数组读写测试
// ============================================================================

/**
 * @brief 测试9：读取32字节数组
 *
 * 读取PLC中的32字节数组变量。
 *
 * 前提条件：PLC中需存在变量 GVL.Perf_32B (ARRAY [0..31] OF BYTE)
 *
 * @return 测试通过返回true，失败返回false
 */
bool TestReadArray32B()
{
    std::cout << "\n========== Test 9: Read 32-Byte Array ==========" << std::endl;
    std::cout << "  Variable: GVL.Perf_32B (ARRAY [0..31] OF BYTE)" << std::endl;
    std::cout << "  Size: 32 bytes" << std::endl;

    std::vector<uint8_t> buffer(SIZE_32B);
    uint32_t bytesRead = 0;

    int64_t ret = AdsLiteReadByName(g_port, &g_targetAddr,
                                    "GVL.Perf_32B",
                                    buffer.data(), SIZE_32B,
                                    &bytesRead);
    if (ret == 0)
    {
        std::cout << "  [PASS] Read " << bytesRead << " bytes" << std::endl;
        std::cout << "  First 8 bytes: ";
        for (size_t i = 0; i < std::min(bytesRead, (uint32_t)8); i++)
        {
            std::cout << std::hex << (int)buffer[i] << " ";
        }
        std::cout << std::dec << "..." << std::endl;
        return true;
    }
    std::cerr << "  [FAIL] Error: " << ret << " (variable not found)" << std::endl;
    return false;
}

/**
 * @brief 测试10：写入32字节数组
 *
 * 向PLC中的32字节数组写入测试数据。
 *
 * @return 测试通过返回true，失败返回false
 */
bool TestWriteArray32B()
{
    std::cout << "\n========== Test 10: Write 32-Byte Array ==========" << std::endl;
    std::cout << "  Variable: GVL.Perf_32B (ARRAY [0..31] OF BYTE)" << std::endl;
    std::cout << "  Data: 0x00-0x1F (incremental)" << std::endl;

    std::vector<uint8_t> buffer(SIZE_32B);
    for (size_t i = 0; i < SIZE_32B; i++)
    {
        buffer[i] = static_cast<uint8_t>(i); // Fill with 0x00, 0x01, 0x02, ... 0x1F
    }

    int64_t ret = AdsLiteWriteByName(g_port, &g_targetAddr,
                                     "GVL.Perf_32B",
                                     buffer.data(), SIZE_32B);
    if (ret == 0)
    {
        std::cout << "  [PASS] Write 32 bytes" << std::endl;
        return true;
    }
    std::cerr << "  [FAIL] Error: " << ret << " (variable not found)" << std::endl;
    return false;
}

/**
 * @brief 测试11：读取256字节数组
 *
 * 读取PLC中的256字节数组变量，测试大数据量传输。
 *
 * 前提条件：PLC中需存在变量 GVL.Perf_256B (ARRAY [0..255] OF BYTE)
 *
 * @return 测试通过返回true，失败返回false
 */
bool TestReadArray256B()
{
    std::cout << "\n========== Test 11: Read 256-Byte Array ==========" << std::endl;
    std::cout << "  Variable: GVL.Perf_256B (ARRAY [0..255] OF BYTE)" << std::endl;
    std::cout << "  Size: 256 bytes" << std::endl;

    std::vector<uint8_t> buffer(SIZE_256B);
    uint32_t bytesRead = 0;

    int64_t ret = AdsLiteReadByName(g_port, &g_targetAddr,
                                    "GVL.Perf_256B",
                                    buffer.data(), SIZE_256B,
                                    &bytesRead);
    if (ret == 0)
    {
        std::cout << "  [PASS] Read " << bytesRead << " bytes" << std::endl;
        std::cout << "  First 8 bytes: ";
        for (size_t i = 0; i < std::min(bytesRead, (uint32_t)8); i++)
        {
            std::cout << std::hex << (int)buffer[i] << " ";
        }
        std::cout << std::dec << "..." << std::endl;
        return true;
    }
    std::cerr << "  [FAIL] Error: " << ret << " (variable not found)" << std::endl;
    return false;
}

/**
 * @brief 测试12：写入256字节数组
 *
 * 向PLC中的256字节数组写入测试数据。
 *
 * @return 测试通过返回true，失败返回false
 */
bool TestWriteArray256B()
{
    std::cout << "\n========== Test 12: Write 256-Byte Array ==========" << std::endl;
    std::cout << "  Variable: GVL.Perf_256B (ARRAY [0..255] OF BYTE)" << std::endl;
    std::cout << "  Data: 0x00, 0x02, 0x04... (even values)" << std::endl;

    std::vector<uint8_t> buffer(SIZE_256B);
    for (size_t i = 0; i < SIZE_256B; i++)
    {
        buffer[i] = static_cast<uint8_t>(i * 2); // Fill with 0x00, 0x02, 0x04, ... 0x1FE
    }

    int64_t ret = AdsLiteWriteByName(g_port, &g_targetAddr,
                                     "GVL.Perf_256B",
                                     buffer.data(), SIZE_256B);
    if (ret == 0)
    {
        std::cout << "  [PASS] Write 256 bytes" << std::endl;
        return true;
    }
    std::cerr << "  [FAIL] Error: " << ret << " (variable not found)" << std::endl;
    return false;
}

/**
 * @brief 测试13：读取LREAL浮点数数组
 *
 * 读取PLC中的33元素LREAL浮点数数组。
 * LREAL是64位双精度浮点数，每个元素8字节，总共264字节。
 *
 * 前提条件：PLC中需存在变量 GVL.Perf_32F (ARRAY [0..32] OF LREAL)
 *
 * @return 测试通过返回true，失败返回false
 */
bool TestReadArray32F()
{
    std::cout << "\n========== Test 13: Read LREAL Array (32F) ==========" << std::endl;
    std::cout << "  Variable: GVL.Perf_32F (ARRAY [0..32] OF LREAL)" << std::endl;
    std::cout << "  Size: 33 elements x 8 bytes = 264 bytes" << std::endl;

    std::vector<uint8_t> buffer(SIZE_32F);
    uint32_t bytesRead = 0;

    int64_t ret = AdsLiteReadByName(g_port, &g_targetAddr,
                                    "GVL.Perf_32F",
                                    buffer.data(), SIZE_32F,
                                    &bytesRead);
    if (ret == 0)
    {
        std::cout << "  [PASS] Read " << bytesRead << " bytes" << std::endl;
        // Print first 3 floating point values
        std::cout << "  First 3 values: ";
        double *values = reinterpret_cast<double *>(buffer.data());
        std::cout << std::fixed << std::setprecision(6);
        for (int i = 0; i < std::min(3, static_cast<int>(bytesRead / 8)); i++)
        {
            std::cout << values[i] << " ";
        }
        std::cout << std::dec << "..." << std::endl;
        return true;
    }
    std::cerr << "  [FAIL] Error: " << ret << " (variable not found)" << std::endl;
    return false;
}

/**
 * @brief 测试14：写入LREAL浮点数数组
 *
 * 向PLC中的33元素LREAL浮点数数组写入测试数据。
 *
 * @return 测试通过返回true，失败返回false
 */
bool TestWriteArray32F()
{
    std::cout << "\n========== Test 14: Write LREAL Array (32F) ==========" << std::endl;
    std::cout << "  Variable: GVL.Perf_32F (ARRAY [0..32] OF LREAL)" << std::endl;
    std::cout << "  Data: 0.0, 1.0, 2.0, ... 32.0" << std::endl;

    std::vector<uint8_t> buffer(SIZE_32F);
    double *values = reinterpret_cast<double *>(buffer.data());
    for (size_t i = 0; i < SIZE_32F / 8; i++)
    {
        values[i] = static_cast<double>(i); // Fill with 0.0, 1.0, 2.0, ... 32.0
    }

    int64_t ret = AdsLiteWriteByName(g_port, &g_targetAddr,
                                     "GVL.Perf_32F",
                                     buffer.data(), SIZE_32F);
    if (ret == 0)
    {
        std::cout << "  [PASS] Write 264 bytes (33 LREAL values)" << std::endl;
        return true;
    }
    std::cerr << "  [FAIL] Error: " << ret << " (variable not found)" << std::endl;
    return false;
}

// ============================================================================
// 性能测试
// ============================================================================

/**
 * @brief 性能测试1：内存读取速度
 *
 * 多次读取PLC内存，测量平均响应时间。
 */
bool TestPerfMemoryRead()
{
    std::cout << "\n========== Performance: Memory Read (4B) ==========" << std::endl;
    std::cout << "  Iterations: " << PERF_ITERATIONS << std::endl;

    std::vector<double> times;
    times.reserve(PERF_ITERATIONS);
    std::vector<uint8_t> buffer(4);
    uint32_t bytesRead = 0;

    // Formal test
    for (int i = 0; i < PERF_ITERATIONS; i++)
    {
        auto start = std::chrono::high_resolution_clock::now();
        int64_t ret = AdsLiteSyncReadReq(g_port, &g_targetAddr, INDEX_GROUP, OFFSET, 4, buffer.data(), &bytesRead);
        auto end = std::chrono::high_resolution_clock::now();

        if (ret != 0)
        {
            std::cerr << "  [FAIL] Read error: " << ret << std::endl;
            return false;
        }

        double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
        times.push_back(elapsed);

        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Avoid InvokeId conflict
    }

    // Statistics
    double minTime = *std::min_element(times.begin(), times.end());
    double maxTime = *std::max_element(times.begin(), times.end());
    double avgTime = std::accumulate(times.begin(), times.end(), 0.0) / times.size();

    PrintPerfResult("SyncReadReq (4B)", avgTime, minTime, maxTime);
    std::cout << "  [PASS]" << std::endl;
    return true;
}

/**
 * @brief 性能测试2：内存写入速度
 */
bool TestPerfMemoryWrite()
{
    std::cout << "\n========== Performance: Memory Write (4B) ==========" << std::endl;
    std::cout << "  Iterations: " << PERF_ITERATIONS << std::endl;

    std::vector<double> times;
    times.reserve(PERF_ITERATIONS);
    std::vector<uint8_t> buffer = {0xAA, 0xBB, 0xCC, 0xDD};

    // Formal test
    for (int i = 0; i < PERF_ITERATIONS; i++)
    {
        auto start = std::chrono::high_resolution_clock::now();
        int64_t ret = AdsLiteSyncWriteReq(g_port, &g_targetAddr, INDEX_GROUP, OFFSET, 4, buffer.data());
        auto end = std::chrono::high_resolution_clock::now();

        if (ret != 0)
        {
            std::cerr << "  [FAIL] Write error: " << ret << std::endl;
            return false;
        }

        double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
        times.push_back(elapsed);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    double minTime = *std::min_element(times.begin(), times.end());
    double maxTime = *std::max_element(times.begin(), times.end());
    double avgTime = std::accumulate(times.begin(), times.end(), 0.0) / times.size();

    PrintPerfResult("SyncWriteReq (4B)", avgTime, minTime, maxTime);
    std::cout << "  [PASS]" << std::endl;
    return true;
}

/**
 * @brief 性能测试3：按名称读取速度
 */
bool TestPerfReadByName()
{
    std::cout << "\n========== Performance: Read By Name (2B INT) ==========" << std::endl;
    std::cout << "  Variable: GVL.TestVar" << std::endl;
    std::cout << "  Iterations: " << PERF_ITERATIONS << std::endl;

    std::vector<double> times;
    times.reserve(PERF_ITERATIONS);
    std::vector<uint8_t> buffer(2);
    uint32_t bytesRead = 0;

    // Formal test
    for (int i = 0; i < PERF_ITERATIONS; i++)
    {
        auto start = std::chrono::high_resolution_clock::now();
        int64_t ret = AdsLiteReadByName(g_port, &g_targetAddr, "GVL.TestVar", buffer.data(), 2, &bytesRead);
        auto end = std::chrono::high_resolution_clock::now();

        if (ret != 0)
        {
            std::cerr << "  [FAIL] Read error: " << ret << std::endl;
            return false;
        }

        double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
        times.push_back(elapsed);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    double minTime = *std::min_element(times.begin(), times.end());
    double maxTime = *std::max_element(times.begin(), times.end());
    double avgTime = std::accumulate(times.begin(), times.end(), 0.0) / times.size();

    PrintPerfResult("ReadByName (2B INT)", avgTime, minTime, maxTime);
    std::cout << "  [PASS]" << std::endl;
    return true;
}

/**
 * @brief 性能测试4：按名称写入速度
 */
bool TestPerfWriteByName()
{
    std::cout << "\n========== Performance: Write By Name (2B INT) ==========" << std::endl;
    std::cout << "  Variable: GVL.TestVar" << std::endl;
    std::cout << "  Iterations: " << PERF_ITERATIONS << std::endl;

    std::vector<double> times;
    times.reserve(PERF_ITERATIONS);
    int16_t value = 12345;
    std::vector<uint8_t> buffer(2);
    buffer[0] = static_cast<uint8_t>(value & 0xFF);
    buffer[1] = static_cast<uint8_t>((value >> 8) & 0xFF);

    // Formal test
    for (int i = 0; i < PERF_ITERATIONS; i++)
    {
        auto start = std::chrono::high_resolution_clock::now();
        int64_t ret = AdsLiteWriteByName(g_port, &g_targetAddr, "GVL.TestVar", buffer.data(), 2);
        auto end = std::chrono::high_resolution_clock::now();

        if (ret != 0)
        {
            std::cerr << "  [FAIL] Write error: " << ret << std::endl;
            return false;
        }

        double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
        times.push_back(elapsed);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    double minTime = *std::min_element(times.begin(), times.end());
    double maxTime = *std::max_element(times.begin(), times.end());
    double avgTime = std::accumulate(times.begin(), times.end(), 0.0) / times.size();

    PrintPerfResult("WriteByName (2B INT)", avgTime, minTime, maxTime);
    std::cout << "  [PASS]" << std::endl;
    return true;
}

// ============================================================================
// 高级功能测试
// ============================================================================

/**
 * @brief 测试16：异步读取操作
 *
 * 使用 AdsLiteAsyncReadReq 和 AdsLiteAsyncWait 函数进行异步读取。
 * 异步操作允许在等待响应时执行其他任务。
 *
 * @return 测试通过返回true，失败返回false
 */
bool TestAsyncRead()
{
    std::cout << "\n========== Test 16: Async Read (AsyncReadReq) ==========" << std::endl;

    std::vector<uint8_t> buffer(2);
    uint32_t bytesRead = 0;
    uint32_t invokeId = 0;

    // Step 1: Submit async read request
    int64_t ret = AdsLiteAsyncReadReq(g_port, &g_targetAddr,
                                      0x4020, 0, // IndexGroup, IndexOffset
                                      2,         // Read 2 bytes
                                      buffer.data(),
                                      &bytesRead,
                                      &invokeId);
    if (ret != 0)
    {
        std::cerr << "  [FAIL] Async read request error: " << ret << std::endl;
        return false;
    }
    std::cout << "  [PASS] Async request submitted, invokeId: " << invokeId << std::endl;

    // Step 2: Wait for completion
    ret = AdsLiteAsyncWait(g_port, invokeId, 1000); // 1 second timeout
    if (ret == 0)
    {
        std::cout << "  [PASS] Async read completed, bytes: " << bytesRead << std::endl;
        return true;
    }
    std::cerr << "  [FAIL] Async wait error: " << ret << std::endl;
    return false;
}

/**
 * @brief 测试17：关闭ADS路由
 *
 * 使用 AdsLiteShutdownRouting 清理路由表资源。
 * 这是程序退出前的必要清理步骤。
 *
 * @return 测试通过返回true，失败返回false
 */
bool TestShutdownRouting()
{
    std::cout << "\n========== Test 17: Shutdown Routing (ShutdownRouting) ==========" << std::endl;

    // Shutdown routing
    AdsLiteShutdownRouting(&g_targetAddr.netId);
    std::cout << "  [PASS] Routing shutdown completed" << std::endl;

    // Note: After shutdown, port should still be open but routing is cleaned up
    std::cout << "  Note: Port " << g_port << " still open (will be closed in cleanup)" << std::endl;

    return true;
}

// ============================================================================
// 主函数
// ============================================================================

/**
 * @brief 程序入口
 *
 * 执行所有测试用例并输出测试结果汇总。
 */
int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "     AdsLite API Test Suite v1.0" << std::endl;
    std::cout << "     Target: " << TEST_REMOTE_ADDR << ":" << TEST_PORT << std::endl;
    std::cout << "========================================" << std::endl;

    int passed = 0, failed = 0;

    // ========== Basic function tests ==========
    if (TestPortAndRouting())
        passed++;
    else
        failed++;

    if (TestMemoryRead())
        passed++;
    else
        failed++;

    if (TestMemoryWrite())
        passed++;
    else
        failed++;

    if (TestReadByName())
        passed++;
    else
        failed++;

    if (TestWriteByName())
        passed++;
    else
        failed++;

    if (TestReadByHandle())
        passed++;
    else
        failed++;

    if (TestWriteByHandle())
        passed++;
    else
        failed++;

    if (TestReadState())
        passed++;
    else
        failed++;

    // ========== Array read/write tests ==========
    if (TestReadArray32B())
        passed++;
    else
        failed++;

    if (TestWriteArray32B())
        passed++;
    else
        failed++;

    if (TestReadArray256B())
        passed++;
    else
        failed++;

    if (TestWriteArray256B())
        passed++;
    else
        failed++;

    // LREAL floating-point array tests
    if (TestReadArray32F())
        passed++;
    else
        failed++;

    if (TestWriteArray32F())
        passed++;
    else
        failed++;

    // ========== Performance tests ==========
    if (TestPerfMemoryRead())
        passed++;
    else
        failed++;

    if (TestPerfMemoryWrite())
        passed++;
    else
        failed++;

    if (TestPerfReadByName())
        passed++;
    else
        failed++;

    if (TestPerfWriteByName())
        passed++;
    else
        failed++;

    // ========== Advanced function tests ==========
    if (TestAsyncRead())
        passed++;
    else
        failed++;

    if (TestShutdownRouting())
        passed++;
    else
        failed++;

    // ========== Cleanup ==========
    if (g_port != 0)
    {
        AdsLitePortClose(g_port);
        std::cout << "\nPort closed: " << g_port << std::endl;
    }

    // ========== Summary ==========
    std::cout << "\n========================================" << std::endl;
    std::cout << "     Results: " << passed << "/" << (passed + failed) << " passed" << std::endl;
    std::cout << "========================================" << std::endl;

    return (failed > 0) ? 1 : 0;
}
