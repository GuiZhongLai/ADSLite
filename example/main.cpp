#include "AdsLiteAPI.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <cstring>
#include <functional>
#include <sstream>
#include <ctime>

/*
class AdsLiteTester
{
private:
    AmsNetId netId_;
    AmsAddr amsAddr_;
    uint16_t port_;
    std::atomic<bool> stopTest_{false};
    std::atomic<uint64_t> totalOperations_{0};
    std::atomic<uint64_t> failedOperations_{0};

public:
    AdsLiteTester(const std::string &remoteIp, const std::string &localIp)
    {
        initialize(remoteIp, localIp);
    }
    ~AdsLiteTester()
    {
        cleanup();
    }

private:
    void initialize(const std::string &remoteIp, const std::string &localIp)
    {
        // 获取远程地址
        int64_t nRet = AdsLiteGetRemoteAddress(remoteIp.c_str(), &netId_);
        if (nRet != 0)
        {
            std::cerr << "Failed to get remote address: " << nRet << std::endl;
            return;
        }
        std::cout << "NetId: ";
        for (int i = 0; i < 6; ++i)
        {
            std::cout << static_cast<int>(netId_.b[i]) << " ";
        }
        std::cout << std::endl;
        // 添加路由
        nRet = AdsLiteAddRemoteRoute(remoteIp.c_str(), localIp.c_str(), "adslite");
        if (nRet != 0)
        {
            std::cerr << "Failed to add remote route: " << nRet << std::endl;
        }
        nRet = AdsLiteAddLocalRoute(remoteIp.c_str(), &netId_);
        if (nRet != 0)
        {
            std::cerr << "Failed to add local route: " << nRet << std::endl;
        }
        // 打开端口
        port_ = AdsLitePortOpen();
        if (port_ == 0)
        {
            std::cerr << "Failed to open port" << std::endl;
            return;
        }
        // 设置AMS地址
        amsAddr_.netId = netId_;
        amsAddr_.port = AMSPORT_R0_PLC_TC3;
    }
    void cleanup()
    {
        if (port_ != 0)
        {
            AdsLitePortClose(port_);
        }
        AdsLiteDeleteLocalRoute(&netId_);
    }

public:
    int64_t readData(uint32_t indexGroup, uint32_t indexOffset, std::vector<uint8_t> &buffer)
    {
        uint32_t len = 0;
        int64_t nRet = AdsLiteSyncReadReq(port_, &amsAddr_, indexGroup, indexOffset,
                                          buffer.size(), buffer.data(), &len);

        totalOperations_++;
        if (nRet != 0)
        {
            failedOperations_++;
        }

        return nRet;
    }
    int64_t writeData(uint32_t indexGroup, uint32_t indexOffset, const std::vector<uint8_t> &buffer)
    {
        int64_t nRet = AdsLiteSyncWriteReq(port_, &amsAddr_, indexGroup, indexOffset,
                                           buffer.size(), buffer.data());

        totalOperations_++;
        if (nRet != 0)
        {
            failedOperations_++;
        }

        return nRet;
    }
    void pressureTestRead(uint32_t iterations, uint32_t delayMs = 0)
    {
        std::vector<uint8_t> buffer(100); // 读取缓冲区

        for (uint32_t i = 0; i < iterations && !stopTest_; ++i)
        {
            int64_t result = readData(0x4020, 0x7d0, buffer);

            if (result == 0)
            {
                if (i % 100 == 0)
                { // 每100次输出一次进度
                    std::cout << "Read iteration " << i << " successful" << std::endl;
                }
            }
            else
            {
                std::cerr << "Read failed at iteration " << i << ": " << result << std::endl;
            }

            if (delayMs > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            }
        }
    }
    void pressureTestWrite(uint32_t iterations, uint32_t delayMs = 0)
    {
        std::vector<uint8_t> testData(50); // 测试数据
        for (size_t i = 0; i < testData.size(); ++i)
        {
            testData[i] = static_cast<uint8_t>(i % 256);
        }
        for (uint32_t i = 0; i < iterations && !stopTest_; ++i)
        {
            // 修改测试数据以模拟不同的写入内容
            testData[0] = static_cast<uint8_t>(i % 256);

            int64_t result = writeData(0x4020, 0x7d0, testData);

            if (result == 0)
            {
                if (i % 100 == 0)
                {
                    std::cout << "Write iteration " << i << " successful" << std::endl;
                }
            }
            else
            {
                std::cerr << "Write failed at iteration " << i << ": " << result << std::endl;
            }

            if (delayMs > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            }
        }
    }
    void mixedPressureTest(uint32_t iterations, uint32_t delayMs = 0)
    {
        std::vector<uint8_t> readBuffer(100);
        std::vector<uint8_t> writeBuffer(50);

        for (size_t i = 0; i < writeBuffer.size(); ++i)
        {
            writeBuffer[i] = static_cast<uint8_t>(i % 256);
        }
        for (uint32_t i = 0; i < iterations && !stopTest_; ++i)
        {
            // 交替进行读写操作
            if (i % 2 == 0)
            {
                int64_t result = readData(0x4020, 0x7d0, readBuffer);
                if (result != 0 && i % 10 == 0)
                {
                    std::cerr << "Mixed test read failed at iteration " << i << ": " << result << std::endl;
                }
            }
            else
            {
                writeBuffer[0] = static_cast<uint8_t>(i % 256);
                int64_t result = writeData(0x4020, 0x1388, writeBuffer);
                if (result != 0 && i % 10 == 0)
                {
                    std::cerr << "Mixed test write failed at iteration " << i << ": " << result << std::endl;
                }
            }
            if (i % 200 == 0)
            {
                std::cout << "Mixed test iteration " << i << std::endl;
            }

            if (delayMs > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            }
        }
    }
    void stopTest()
    {
        stopTest_ = true;
    }
    void printStatistics() const
    {
        std::cout << "\n=== Test Statistics ===" << std::endl;
        std::cout << "Total operations: " << totalOperations_ << std::endl;
        std::cout << "Failed operations: " << failedOperations_ << std::endl;
        std::cout << "Success rate: " << std::fixed << std::setprecision(2)
                  << (100.0 * (totalOperations_ - failedOperations_) / totalOperations_)
                  << "%" << std::endl;
    }
};
*/

class AdsLiteTester
{
private:
    AmsNetId netId_;
    AmsAddr amsAddr_;
    uint16_t port_;
    std::atomic<bool> stopTest_{false};
    std::atomic<uint64_t> totalOperations_{0};
    std::atomic<uint64_t> failedOperations_{0};
    std::atomic<uint64_t> totalBytesTransferred_{0};

public:
    AdsLiteTester(const std::string &remoteIp, const std::string &localIp)
    {
        initialize(remoteIp, localIp);
    }
    ~AdsLiteTester()
    {
        cleanup();
    }

public:
    void initialize(const std::string &remoteIp, const std::string &localIp)
    {
        printWithTimestamp("Initializing ADS Lite connection...");

        int64_t nRet = AdsLiteGetRemoteAddress(remoteIp.c_str(), &netId_);
        if (nRet != 0)
        {
            printWithTimestamp("Failed to get remote address: " + std::to_string(nRet));
            throw std::runtime_error("Failed to get remote address");
        }
        std::stringstream netIdStr;
        netIdStr << "NetId: ";
        for (int i = 0; i < 6; ++i)
        {
            netIdStr << static_cast<int>(netId_.b[i]) << " ";
        }
        printWithTimestamp(netIdStr.str());
        nRet = AdsLiteAddRemoteRoute(remoteIp.c_str(), localIp.c_str(), "adslite");
        if (nRet != 0)
        {
            printWithTimestamp("Failed to add remote route: " + std::to_string(nRet));
        }
        nRet = AdsLiteAddLocalRoute(remoteIp.c_str(), &netId_);
        if (nRet != 0)
        {
            printWithTimestamp("Failed to add local route: " + std::to_string(nRet));
        }
        port_ = AdsLitePortOpen();
        if (port_ == 0)
        {
            printWithTimestamp("Failed to open port");
            throw std::runtime_error("Failed to open port");
        }
        amsAddr_.netId = netId_;
        amsAddr_.port = AMSPORT_R0_PLC_TC3;

        printWithTimestamp("Initialization completed successfully");
    }
    void cleanup()
    {
        printWithTimestamp("Cleaning up resources...");
        if (port_ != 0)
        {
            AdsLitePortClose(port_);
        }
        AdsLiteDeleteLocalRoute(&netId_);
    }
    std::string getTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now.time_since_epoch()) %
                      1000;

        std::stringstream ss;

#ifdef _WIN32
        std::tm time_info;
        localtime_s(&time_info, &now_time_t);
        ss << "[" << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S");
#else
        ss << "[" << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");
#endif

        ss << "." << std::setfill('0') << std::setw(3) << now_ms.count() << "]";
        return ss.str();
    }
    void printWithTimestamp(const std::string &message)
    {
        std::cout << getTimestamp() << " " << message << std::endl;
    }
    template <typename Func>
    std::pair<int64_t, std::chrono::microseconds> measureOperation(Func &&operation)
    {
        auto start = std::chrono::high_resolution_clock::now();
        int64_t result = operation();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        return {result, duration};
    }

public:
    std::pair<int64_t, std::chrono::microseconds> readData(uint32_t indexGroup, uint32_t indexOffset,
                                                           std::vector<uint8_t> &buffer)
    {
        uint32_t len = 0;
        auto resultPair = measureOperation([&]()
                                           { return AdsLiteSyncReadReq(port_, &amsAddr_, indexGroup, indexOffset,
                                                                       buffer.size(), buffer.data(), &len); });

        totalOperations_++;
        totalBytesTransferred_ += buffer.size();
        if (resultPair.first != 0)
        {
            failedOperations_++;
        }

        return resultPair;
    }
    std::pair<int64_t, std::chrono::microseconds> writeData(uint32_t indexGroup, uint32_t indexOffset,
                                                            const std::vector<uint8_t> &buffer)
    {
        auto resultPair = measureOperation([&]()
                                           { return AdsLiteSyncWriteReq(port_, &amsAddr_, indexGroup, indexOffset,
                                                                        buffer.size(), buffer.data()); });

        totalOperations_++;
        totalBytesTransferred_ += buffer.size();
        if (resultPair.first != 0)
        {
            failedOperations_++;
        }

        return resultPair;
    }
    void pressureTestRead(uint32_t iterations, size_t bufferSize, uint32_t delayMs = 0)
    {
        std::vector<uint8_t> buffer(bufferSize);
        uint64_t totalReadTime = 0;
        uint64_t successfulReads = 0;
        std::stringstream startMsg;
        startMsg << "Starting read pressure test with "
                 << bufferSize << " bytes buffer, " << iterations << " iterations";
        printWithTimestamp(startMsg.str());
        for (uint32_t i = 0; i < iterations && !stopTest_; ++i)
        {
            auto resultPair = readData(0x4020, 0x7d0, buffer);

            totalReadTime += resultPair.second.count();
            if (resultPair.first == 0)
            {
                successfulReads++;
                if (i % 100 == 0)
                {
                    std::stringstream msg;
                    double speed = (bufferSize * 1e6) / (resultPair.second.count() * 1024.0);
                    msg << "Read iteration " << i
                        << " - Time: " << resultPair.second.count() << "us"
                        << " - Speed: " << std::fixed << std::setprecision(2) << speed << " KB/s";
                    printWithTimestamp(msg.str());
                }
            }
            else
            {
                std::stringstream errMsg;
                errMsg << "Read failed at iteration " << i << ": " << resultPair.first;
                printWithTimestamp(errMsg.str());
            }

            if (delayMs > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            }
        }
        if (successfulReads > 0)
        {
            double avgTime = totalReadTime / static_cast<double>(successfulReads);
            std::stringstream msg;
            msg << "Read test completed - Avg time: " << std::fixed << std::setprecision(2) << avgTime << "us";
            printWithTimestamp(msg.str());
        }
    }
    void pressureTestWrite(uint32_t iterations, size_t bufferSize, uint32_t delayMs = 0)
    {
        std::vector<uint8_t> testData(bufferSize);
        for (size_t i = 0; i < testData.size(); ++i)
        {
            testData[i] = static_cast<uint8_t>(i % 256);
        }
        uint64_t totalWriteTime = 0;
        uint64_t successfulWrites = 0;
        std::stringstream startMsg;
        startMsg << "Starting write pressure test with "
                 << bufferSize << " bytes buffer, " << iterations << " iterations";
        printWithTimestamp(startMsg.str());
        for (uint32_t i = 0; i < iterations && !stopTest_; ++i)
        {
            testData[0] = static_cast<uint8_t>(i % 256);

            auto resultPair = writeData(0x4020, 0x7d0, testData);

            totalWriteTime += resultPair.second.count();
            if (resultPair.first == 0)
            {
                successfulWrites++;
                if (i % 100 == 0)
                {
                    std::stringstream msg;
                    double speed = (bufferSize * 1e6) / (resultPair.second.count() * 1024.0);
                    msg << "Write iteration " << i
                        << " - Time: " << resultPair.second.count() << "us"
                        << " - Speed: " << std::fixed << std::setprecision(2) << speed << " KB/s";
                    printWithTimestamp(msg.str());
                }
            }
            else
            {
                std::stringstream errMsg;
                errMsg << "Write failed at iteration " << i << ": " << resultPair.first;
                printWithTimestamp(errMsg.str());
            }

            if (delayMs > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            }
        }
        if (successfulWrites > 0)
        {
            double avgTime = totalWriteTime / static_cast<double>(successfulWrites);
            std::stringstream msg;
            msg << "Write test completed - Avg time: " << std::fixed << std::setprecision(2) << avgTime << "us";
            printWithTimestamp(msg.str());
        }
    }
    void mixedPressureTest(uint32_t iterations, size_t bufferSize, uint32_t delayMs = 0)
    {
        std::vector<uint8_t> readBuffer(bufferSize);
        std::vector<uint8_t> writeBuffer(bufferSize);

        for (size_t i = 0; i < writeBuffer.size(); ++i)
        {
            writeBuffer[i] = static_cast<uint8_t>(i % 256);
        }
        uint64_t totalReadTime = 0, totalWriteTime = 0;
        uint64_t successfulReads = 0, successfulWrites = 0;
        std::stringstream startMsg;
        startMsg << "Starting mixed test with "
                 << bufferSize << " bytes buffer, " << iterations << " iterations";
        printWithTimestamp(startMsg.str());
        for (uint32_t i = 0; i < iterations && !stopTest_; ++i)
        {
            if (i % 2 == 0)
            {
                auto resultPair = readData(0x4020, 0x7d0, readBuffer);
                totalReadTime += resultPair.second.count();
                if (resultPair.first == 0)
                    successfulReads++;

                if (resultPair.first != 0 && i % 10 == 0)
                {
                    std::stringstream errMsg;
                    errMsg << "Mixed test read failed at iteration " << i << ": " << resultPair.first;
                    printWithTimestamp(errMsg.str());
                }
            }
            else
            {
                writeBuffer[0] = static_cast<uint8_t>(i % 256);
                auto resultPair = writeData(0x4020, 0x7d0, writeBuffer);
                totalWriteTime += resultPair.second.count();
                if (resultPair.first == 0)
                    successfulWrites++;

                if (resultPair.first != 0 && i % 10 == 0)
                {
                    std::stringstream errMsg;
                    errMsg << "Mixed test write failed at iteration " << i << ": " << resultPair.first;
                    printWithTimestamp(errMsg.str());
                }
            }
            if (i % 200 == 0)
            {
                std::stringstream msg;
                msg << "Mixed test iteration " << i;
                printWithTimestamp(msg.str());
            }

            if (delayMs > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            }
        }
        if (successfulReads > 0)
        {
            double avgReadTime = totalReadTime / static_cast<double>(successfulReads);
            std::stringstream msg;
            msg << "Mixed test read avg time: " << std::fixed << std::setprecision(2) << avgReadTime << "us";
            printWithTimestamp(msg.str());
        }
        if (successfulWrites > 0)
        {
            double avgWriteTime = totalWriteTime / static_cast<double>(successfulWrites);
            std::stringstream msg;
            msg << "Mixed test write avg time: " << std::fixed << std::setprecision(2) << avgWriteTime << "us";
            printWithTimestamp(msg.str());
        }
    }
    void stopTest()
    {
        stopTest_ = true;
    }
    void printStatistics()
    {
        std::cout << std::endl;
        printWithTimestamp("=== Test Statistics ===");

        std::stringstream stats;
        stats << "Total operations: " << totalOperations_;
        printWithTimestamp(stats.str());

        stats.str("");
        stats << "Failed operations: " << failedOperations_;
        printWithTimestamp(stats.str());

        stats.str("");
        stats << "Total bytes transferred: " << totalBytesTransferred_ << " bytes";
        printWithTimestamp(stats.str());

        stats.str("");
        double successRate = 100.0 * (totalOperations_ - failedOperations_) / totalOperations_;
        stats << "Success rate: " << std::fixed << std::setprecision(2) << successRate << "%";
        printWithTimestamp(stats.str());
    }
};

int main()
{
    const std::string remoteIp = "192.168.11.77";
    const std::string localIp = "192.168.11.17";

    const size_t largeBufferSize = 1000;

    try
    {
        AdsLiteTester tester(remoteIp, localIp);

        std::stringstream startMsg;
        startMsg << "Starting pressure tests with " << largeBufferSize << " bytes buffer...";
        tester.printWithTimestamp(startMsg.str());

        // 大块数据读取测试
        tester.printWithTimestamp("=== Large Buffer Read Test ===");
        tester.pressureTestRead(800, largeBufferSize, 5);

        // 大块数据写入测试
        tester.printWithTimestamp("=== Large Buffer Write Test ===");
        tester.pressureTestWrite(800, largeBufferSize, 10);

        // 大块数据混合测试
        tester.printWithTimestamp("=== Large Buffer Mixed Test ===");
        tester.mixedPressureTest(800, largeBufferSize, 3);

        tester.printStatistics();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception occurred: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}