#include "AdsLiteAPI.h"
#include "standalone/AmsNetId.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static const char *TEST_REMOTE_ADDR = "192.168.11.88";
static const uint16_t TEST_PORT = 851;
static const char *FILE_ROOT = "C:/test/AdsLite/example_file_api";
static const char *REG_KEEP_ROOT_NAME = "reg_keep_root";
static const char *REG_KEEP_PROBE_NAME = "probe.txt";

struct TestContext
{
    uint16_t port;
    AmsAddr addr;
    uint32_t writeFlags;
    uint32_t readFlags;
};

static void PrintApiResult(const char *apiName, int64_t ret)
{
    std::cout << "  " << apiName << " => ret=" << ret << std::endl;
}

// 按换行拆分并输出 FileList 返回的文件名列表。
static void PrintList(const char *listBuffer)
{
    std::cout << "  File names:" << std::endl;
    if (!listBuffer || listBuffer[0] == '\0')
    {
        std::cout << "    (empty)" << std::endl;
        return;
    }

    const char *start = listBuffer;
    while (*start != '\0')
    {
        const char *end = start;
        while (*end != '\0' && *end != '\n')
        {
            ++end;
        }

        if (end > start)
        {
            std::cout << "    ";
            std::cout.write(start, static_cast<std::streamsize>(end - start));
            std::cout << std::endl;
        }

        if (*end == '\0')
        {
            break;
        }
        start = end + 1;
    }
}

static bool ContainsListEntry(const char *listBuffer, const char *entryName)
{
    if (!listBuffer || !entryName || entryName[0] == '\0')
    {
        return false;
    }

    const char *start = listBuffer;
    while (*start != '\0')
    {
        const char *end = start;
        while (*end != '\0' && *end != '\n')
        {
            ++end;
        }

        const size_t entryLength = static_cast<size_t>(end - start);
        if (std::strlen(entryName) == entryLength &&
            std::strncmp(start, entryName, entryLength) == 0)
        {
            return true;
        }

        if (*end == '\0')
        {
            break;
        }
        start = end + 1;
    }

    return false;
}

static bool ListPath(TestContext &ctx,
                     const char *apiName,
                     const char *pathPattern,
                     char *listBuffer,
                     uint32_t bufferLength,
                     uint32_t *pRequired,
                     uint32_t *pItemCount)
{
    const int64_t ret = AdsLiteFileList(ctx.port,
                                        &ctx.addr,
                                        pathPattern,
                                        listBuffer,
                                        bufferLength,
                                        pRequired,
                                        pItemCount);
    PrintApiResult(apiName, ret);
    if (pRequired && pItemCount)
    {
        std::cout << "  itemCount=" << *pItemCount << ", required=" << *pRequired << std::endl;
    }
    PrintList(listBuffer);
    return ret == 0;
}

static std::string PathUnderRoot(const char *relativePath)
{
    std::string path = FILE_ROOT;
    if (relativePath && relativePath[0] != '\0')
    {
        path += "/";
        path += relativePath;
    }
    return path;
}

static std::string PatternUnderRoot(const char *relativePath)
{
    return PathUnderRoot(relativePath) + "/*";
}

static std::string PathUnderKeepRoot(const char *relativePath)
{
    std::string path = PathUnderRoot(REG_KEEP_ROOT_NAME);
    if (relativePath && relativePath[0] != '\0')
    {
        path += "/";
        path += relativePath;
    }
    return path;
}

// 在每个用例开始前清理测试根目录。
static bool ResetRoot(TestContext &ctx)
{
    int64_t ret = AdsLiteDirDelete(ctx.port, &ctx.addr, FILE_ROOT, true);
    PrintApiResult("AdsLiteDirDelete(pre-clean)", ret);
    if (ret == 0 || ret == ADSERR_DEVICE_NOTFOUND || ret == ADSERR_DEVICE_INVALIDOFFSET)
    {
        if (ret != 0)
        {
            std::cout << "  pre-clean ignored because test root is not present yet" << std::endl;
        }
        return true;
    }
    return false;
}

// 显式创建测试根目录，供简单文件场景复用。
static bool EnsureRoot(TestContext &ctx)
{
    int64_t ret = AdsLiteDirCreate(ctx.port, &ctx.addr, FILE_ROOT);
    PrintApiResult("AdsLiteDirCreate(root)", ret);
    return ret == 0;
}

// 以整文件方式写入测试数据，隐藏打开、写入和关闭细节。
static int64_t WriteWholeFile(TestContext &ctx,
                              const char *path,
                              const void *data,
                              uint32_t length)
{
    uint32_t handle = 0;
    int64_t ret = AdsLiteFileOpen(ctx.port, &ctx.addr, path, ctx.writeFlags, &handle);
    if (ret != 0)
    {
        return ret;
    }

    ret = AdsLiteFileWrite(ctx.port, &ctx.addr, handle, data, length);
    const int64_t closeRet = AdsLiteFileClose(ctx.port, &ctx.addr, handle);
    if (ret != 0)
    {
        return ret;
    }
    return closeRet;
}

// 以整文件方式读取测试数据，并在缓冲区不足时返回长度错误。
static int64_t ReadWholeFile(TestContext &ctx,
                             const char *path,
                             void *data,
                             uint32_t bufferLength,
                             uint32_t *pBytesRead)
{
    if (!data || !pBytesRead)
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }

    *pBytesRead = 0;

    uint32_t handle = 0;
    int64_t ret = AdsLiteFileOpen(ctx.port, &ctx.addr, path, ctx.readFlags, &handle);
    if (ret != 0)
    {
        return ret;
    }

    uint8_t *writePtr = static_cast<uint8_t *>(data);
    while (true)
    {
        if (*pBytesRead >= bufferLength)
        {
            uint8_t dummy = 0;
            uint32_t probeRead = 0;
            ret = AdsLiteFileRead(ctx.port, &ctx.addr, handle, 1, &dummy, &probeRead);
            if (ret == 0 && probeRead > 0)
            {
                ret = ADSERR_DEVICE_INVALIDSIZE;
            }
            break;
        }

        uint32_t chunkRead = 0;
        ret = AdsLiteFileRead(ctx.port,
                              &ctx.addr,
                              handle,
                              bufferLength - *pBytesRead,
                              writePtr + *pBytesRead,
                              &chunkRead);
        if (ret != 0)
        {
            break;
        }
        if (chunkRead == 0)
        {
            break;
        }
        *pBytesRead += chunkRead;
    }

    const int64_t closeRet = AdsLiteFileClose(ctx.port, &ctx.addr, handle);
    if (ret != 0)
    {
        return ret;
    }
    return closeRet;
}

// 验证目录创建接口在多级空目录场景下的表现。
static bool TestAdsLiteDirCreate(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteDirCreate" << std::endl;
    if (!ResetRoot(ctx))
    {
        return false;
    }

    const std::string dirPath = PathUnderRoot("case_dir_create/l1/l2");
    int64_t ret = AdsLiteDirCreate(ctx.port, &ctx.addr, dirPath.c_str());
    PrintApiResult("AdsLiteDirCreate(case_dir_create)", ret);
    return ret == 0;
}

// 验证目录删除接口可删除空目录。
static bool TestAdsLiteDirDelete(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteDirDelete" << std::endl;
    if (!ResetRoot(ctx))
    {
        return false;
    }

    const std::string dirPath = PathUnderRoot("case_dir_delete");
    int64_t ret = AdsLiteDirCreate(ctx.port, &ctx.addr, dirPath.c_str());
    PrintApiResult("AdsLiteDirCreate(case_dir_delete)", ret);
    if (ret != 0)
    {
        return false;
    }

    ret = AdsLiteDirDelete(ctx.port, &ctx.addr, dirPath.c_str(), true);
    PrintApiResult("AdsLiteDirDelete(case_dir_delete)", ret);
    return ret == 0;
}

// 验证 ENSURE_DIR 在删除最后一个文件后，是否会留下可疑的空目录状态。
static bool TestAdsLiteFileOpen(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteFileOpen" << std::endl;
    if (!ResetRoot(ctx))
    {
        return false;
    }

    // 这里仍然只依赖 ENSURE_DIR 触发多级目录补建，但在成功后立刻删掉文件，
    // 让目标机上最终留下“空目录”，更贴近 access denied 现场。
    const std::string path = PathUnderRoot("case_open/l1/l2/case_open.txt");
    uint32_t handle = 0;
    int64_t ret = AdsLiteFileOpen(ctx.port, &ctx.addr, path.c_str(), ctx.writeFlags, &handle);
    PrintApiResult("AdsLiteFileOpen(write)", ret);
    std::cout << "  handle=" << handle << std::endl;
    if (ret != 0)
    {
        return false;
    }

    ret = AdsLiteFileClose(ctx.port, &ctx.addr, handle);
    PrintApiResult("AdsLiteFileClose(after open)", ret);
    if (ret != 0)
    {
        return false;
    }

    ret = AdsLiteFileDelete(ctx.port, &ctx.addr, path.c_str());
    PrintApiResult("AdsLiteFileDelete(after open)", ret);
    return ret == 0;
}

// 验证文件关闭接口可正确释放句柄。
static bool TestAdsLiteFileClose(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteFileClose" << std::endl;
    if (!ResetRoot(ctx) || !EnsureRoot(ctx))
    {
        return false;
    }

    const std::string path = PathUnderRoot("case_close.txt");
    uint32_t handle = 0;
    int64_t ret = AdsLiteFileOpen(ctx.port, &ctx.addr, path.c_str(), ctx.writeFlags, &handle);
    PrintApiResult("AdsLiteFileOpen(for close)", ret);
    if (ret != 0)
    {
        return false;
    }

    ret = AdsLiteFileClose(ctx.port, &ctx.addr, handle);
    PrintApiResult("AdsLiteFileClose", ret);
    return ret == 0;
}

// 验证文件写接口可写入指定 payload。
static bool TestAdsLiteFileWrite(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteFileWrite" << std::endl;
    if (!ResetRoot(ctx) || !EnsureRoot(ctx))
    {
        return false;
    }

    const std::string path = PathUnderRoot("case_write.txt");
    const char payload[] = "write-api-payload\n";
    uint32_t handle = 0;
    int64_t ret = AdsLiteFileOpen(ctx.port, &ctx.addr, path.c_str(), ctx.writeFlags, &handle);
    PrintApiResult("AdsLiteFileOpen(for write)", ret);
    if (ret != 0)
    {
        return false;
    }

    ret = AdsLiteFileWrite(ctx.port,
                           &ctx.addr,
                           handle,
                           payload,
                           static_cast<uint32_t>(sizeof(payload) - 1));
    PrintApiResult("AdsLiteFileWrite", ret);

    int64_t closeRet = AdsLiteFileClose(ctx.port, &ctx.addr, handle);
    PrintApiResult("AdsLiteFileClose(after write)", closeRet);
    return ret == 0 && closeRet == 0;
}

// 验证文件读接口可读取完整 payload。
static bool TestAdsLiteFileRead(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteFileRead" << std::endl;
    if (!ResetRoot(ctx))
    {
        return false;
    }

    const std::string path = PathUnderRoot("case_read.txt");
    const char payload[] = "read-api-payload\n";
    int64_t ret = WriteWholeFile(ctx,
                                 path.c_str(),
                                 payload,
                                 static_cast<uint32_t>(sizeof(payload) - 1));
    PrintApiResult("WriteWholeFile(prepare read)", ret);
    if (ret != 0)
    {
        return false;
    }

    std::vector<char> buffer(sizeof(payload), 0);
    uint32_t bytesRead = 0;
    ret = ReadWholeFile(ctx,
                        path.c_str(),
                        buffer.data(),
                        static_cast<uint32_t>(buffer.size() - 1),
                        &bytesRead);
    PrintApiResult("ReadWholeFile", ret);
    std::cout << "  bytesRead=" << bytesRead << ", content=" << buffer.data() << std::endl;
    return ret == 0;
}

// 验证文件删除接口可移除已存在文件。
static bool TestAdsLiteFileDelete(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteFileDelete" << std::endl;
    if (!ResetRoot(ctx))
    {
        return false;
    }

    const std::string path = PathUnderRoot("case_delete.txt");
    const char payload[] = "delete-api-payload\n";
    int64_t ret = WriteWholeFile(ctx,
                                 path.c_str(),
                                 payload,
                                 static_cast<uint32_t>(sizeof(payload) - 1));
    PrintApiResult("WriteWholeFile(prepare delete)", ret);
    if (ret != 0)
    {
        return false;
    }

    ret = AdsLiteFileDelete(ctx.port, &ctx.addr, path.c_str());
    PrintApiResult("AdsLiteFileDelete", ret);
    return ret == 0;
}

// 验证重命名接口可完成复制并删除旧文件。
static bool TestAdsLiteFileRename(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteFileRename" << std::endl;
    if (!ResetRoot(ctx))
    {
        return false;
    }

    const std::string sourcePath = PathUnderRoot("case_rename_src.txt");
    const std::string targetPath = PathUnderRoot("case_rename_dst.txt");
    const char payload[] = "rename-api-payload\n";

    int64_t ret = WriteWholeFile(ctx,
                                 sourcePath.c_str(),
                                 payload,
                                 static_cast<uint32_t>(sizeof(payload) - 1));
    PrintApiResult("WriteWholeFile(prepare rename)", ret);
    if (ret != 0)
    {
        return false;
    }

    ret = AdsLiteFileRename(ctx.port, &ctx.addr, sourcePath.c_str(), targetPath.c_str());
    PrintApiResult("AdsLiteFileRename", ret);
    return ret == 0;
}

// 验证文件列表接口可返回当前目录项。
static bool TestAdsLiteFileList(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteFileList" << std::endl;
    if (!ResetRoot(ctx))
    {
        return false;
    }

    const std::string pathA = PathUnderRoot("case_list_a.txt");
    const std::string pathB = PathUnderRoot("case_list_b.txt");
    const char payload[] = "list-api-payload\n";

    int64_t ret = WriteWholeFile(ctx,
                                 pathA.c_str(),
                                 payload,
                                 static_cast<uint32_t>(sizeof(payload) - 1));
    PrintApiResult("WriteWholeFile(list a)", ret);
    if (ret != 0)
    {
        return false;
    }

    ret = WriteWholeFile(ctx,
                         pathB.c_str(),
                         payload,
                         static_cast<uint32_t>(sizeof(payload) - 1));
    PrintApiResult("WriteWholeFile(list b)", ret);
    if (ret != 0)
    {
        return false;
    }

    char listBuffer[2048] = {0};
    uint32_t required = 0;
    uint32_t itemCount = 0;
    ret = AdsLiteFileList(ctx.port,
                          &ctx.addr,
                          PatternUnderRoot("").c_str(),
                          listBuffer,
                          static_cast<uint32_t>(sizeof(listBuffer)),
                          &required,
                          &itemCount);
    PrintApiResult("AdsLiteFileList", ret);
    std::cout << "  itemCount=" << itemCount << ", required=" << required << std::endl;
    PrintList(listBuffer);
    return ret == 0;
}

// 验证目录递归删除可清空深层子目录。
static bool TestAdsLiteDirDeleteRecursive(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteDirDelete(recursive)" << std::endl;
    if (!ResetRoot(ctx))
    {
        return false;
    }

    const std::string nestedFile = PathUnderRoot("r1/r2/r3/delete_recursive.bin");
    const uint8_t payload[] = {0x01, 0x02, 0x03};
    int64_t ret = WriteWholeFile(ctx,
                                 nestedFile.c_str(),
                                 payload,
                                 static_cast<uint32_t>(sizeof(payload)));
    PrintApiResult("WriteWholeFile(prepare recursive delete)", ret);
    if (ret != 0)
    {
        return false;
    }

    ret = AdsLiteDirDelete(ctx.port, &ctx.addr, FILE_ROOT, true);
    PrintApiResult("AdsLiteDirDelete(recursive)", ret);
    return ret == 0;
}

// 验证混合路径分隔符输入会被统一标准化处理。
static bool TestRegressionMixedSeparators(TestContext &ctx)
{
    std::cout << "\n[CASE] Regression.MixedSeparators" << std::endl;
    if (!ResetRoot(ctx))
    {
        return false;
    }

    const std::string mixedPath = "C://test\\AdsLite//example_file_api//reg_sep\\sub//mixed.txt";
    const std::string readPath = PathUnderRoot("reg_sep/sub/mixed.txt");
    const char payload[] = "mixed-separators-ok\n";

    int64_t ret = WriteWholeFile(ctx,
                                 mixedPath.c_str(),
                                 payload,
                                 static_cast<uint32_t>(sizeof(payload) - 1));
    PrintApiResult("WriteWholeFile(mixed separators)", ret);
    if (ret != 0)
    {
        return false;
    }

    char buffer[128] = {0};
    uint32_t bytesRead = 0;
    ret = ReadWholeFile(ctx,
                        readPath.c_str(),
                        buffer,
                        static_cast<uint32_t>(sizeof(buffer) - 1),
                        &bytesRead);
    PrintApiResult("ReadWholeFile(normalized path)", ret);
    if (ret != 0)
    {
        return false;
    }

    std::cout << "  bytesRead=" << bytesRead << ", content=" << buffer << std::endl;
    return std::strcmp(buffer, payload) == 0;
}

// 验证仅清空目录内容时可保留根目录本身。
static bool TestRegressionDirDeleteKeepRoot(TestContext &ctx)
{
    std::cout << "\n[CASE] Regression.DirDeleteKeepRoot" << std::endl;
    if (!ResetRoot(ctx))
    {
        return false;
    }

    const std::string rootDir = PathUnderRoot(REG_KEEP_ROOT_NAME);
    const std::string nestedFile = PathUnderKeepRoot("l1/l2/data.bin");
    const std::string probeFile = PathUnderKeepRoot(REG_KEEP_PROBE_NAME);
    const uint8_t payload[] = {0xAA, 0xBB, 0xCC};

    int64_t ret = AdsLiteDirCreate(ctx.port, &ctx.addr, rootDir.c_str());
    PrintApiResult("AdsLiteDirCreate(reg_keep_root)", ret);
    if (ret != 0)
    {
        return false;
    }

    ret = WriteWholeFile(ctx,
                         nestedFile.c_str(),
                         payload,
                         static_cast<uint32_t>(sizeof(payload)));
    PrintApiResult("WriteWholeFile(reg_keep_root)", ret);
    if (ret != 0)
    {
        return false;
    }

    ret = AdsLiteDirDelete(ctx.port, &ctx.addr, rootDir.c_str(), false);
    PrintApiResult("AdsLiteDirDelete(deleteDirSelf=false)", ret);
    if (ret != 0)
    {
        return false;
    }

    const std::string rootPattern = rootDir + "/*";
    char listBuffer[1024] = {0};
    uint32_t required = 0;
    uint32_t itemCount = 0;
    if (!ListPath(ctx,
                  "AdsLiteFileList(reg_keep_root after delete)",
                  rootPattern.c_str(),
                  listBuffer,
                  static_cast<uint32_t>(sizeof(listBuffer)),
                  &required,
                  &itemCount) ||
        itemCount != 0)
    {
        return false;
    }

    const uint32_t noEnsureWriteFlags = ADSLITE_FOPEN_WRITE |
                                        ADSLITE_FOPEN_BINARY |
                                        ADSLITE_FOPEN_PLUS;
    uint32_t handle = 0;
    ret = AdsLiteFileOpen(ctx.port, &ctx.addr, probeFile.c_str(), noEnsureWriteFlags, &handle);
    PrintApiResult("AdsLiteFileOpen(probe keep root)", ret);
    if (ret != 0)
    {
        return false;
    }

    ret = AdsLiteFileClose(ctx.port, &ctx.addr, handle);
    PrintApiResult("AdsLiteFileClose(probe keep root)", ret);
    if (ret != 0)
    {
        return false;
    }

    std::memset(listBuffer, 0, sizeof(listBuffer));
    required = 0;
    itemCount = 0;
    if (!ListPath(ctx,
                  "AdsLiteFileList(reg_keep_root final)",
                  rootPattern.c_str(),
                  listBuffer,
                  static_cast<uint32_t>(sizeof(listBuffer)),
                  &required,
                  &itemCount) ||
        itemCount != 1 ||
        !ContainsListEntry(listBuffer, REG_KEEP_PROBE_NAME))
    {
        return false;
    }

    return true;
}

static bool TestFinalDirectoryState(TestContext &ctx)
{
    std::cout << "\n[CASE] FinalDirectoryState" << std::endl;

    if (!ResetRoot(ctx))
    {
        return false;
    }

    const std::string rootDir = PathUnderRoot(REG_KEEP_ROOT_NAME);
    const std::string probeFile = PathUnderKeepRoot(REG_KEEP_PROBE_NAME);
    const char payload[] = "final-state\n";

    int64_t ret = AdsLiteDirCreate(ctx.port, &ctx.addr, rootDir.c_str());
    PrintApiResult("AdsLiteDirCreate(final root)", ret);
    if (ret != 0)
    {
        return false;
    }

    ret = WriteWholeFile(ctx,
                         probeFile.c_str(),
                         payload,
                         static_cast<uint32_t>(sizeof(payload) - 1));
    PrintApiResult("WriteWholeFile(final probe)", ret);
    if (ret != 0)
    {
        return false;
    }

    const std::string rootPattern = std::string(FILE_ROOT) + "/*";
    char listBuffer[1024] = {0};
    uint32_t required = 0;
    uint32_t itemCount = 0;
    if (!ListPath(ctx,
                  "AdsLiteFileList(FILE_ROOT final)",
                  rootPattern.c_str(),
                  listBuffer,
                  static_cast<uint32_t>(sizeof(listBuffer)),
                  &required,
                  &itemCount) ||
        itemCount != 1 ||
        !ContainsListEntry(listBuffer, REG_KEEP_ROOT_NAME))
    {
        return false;
    }

    return true;
}

// 验证 FileList 在小缓冲区场景下返回所需长度和错误码。
static bool TestRegressionFileListSmallBuffer(TestContext &ctx)
{
    std::cout << "\n[CASE] Regression.FileListSmallBuffer" << std::endl;
    if (!ResetRoot(ctx))
    {
        return false;
    }

    const std::string pathA = PathUnderRoot("reg_list_small_a.txt");
    const std::string pathB = PathUnderRoot("reg_list_small_b.txt");
    const char payload[] = "list-small-buffer\n";

    int64_t ret = WriteWholeFile(ctx,
                                 pathA.c_str(),
                                 payload,
                                 static_cast<uint32_t>(sizeof(payload) - 1));
    PrintApiResult("WriteWholeFile(reg list a)", ret);
    if (ret != 0)
    {
        return false;
    }

    ret = WriteWholeFile(ctx,
                         pathB.c_str(),
                         payload,
                         static_cast<uint32_t>(sizeof(payload) - 1));
    PrintApiResult("WriteWholeFile(reg list b)", ret);
    if (ret != 0)
    {
        return false;
    }

    char tinyBuffer[4] = {0};
    uint32_t required = 0;
    uint32_t itemCount = 0;
    ret = AdsLiteFileList(ctx.port,
                          &ctx.addr,
                          PatternUnderRoot("").c_str(),
                          tinyBuffer,
                          static_cast<uint32_t>(sizeof(tinyBuffer)),
                          &required,
                          &itemCount);
    PrintApiResult("AdsLiteFileList(tiny buffer)", ret);
    if (ret == ADSERR_DEVICE_INVALIDSIZE)
    {
        std::cout << "  expected invalid size for intentionally undersized buffer" << std::endl;
    }
    std::cout << "  required=" << required << ", itemCount=" << itemCount << std::endl;
    return ret == ADSERR_DEVICE_INVALIDSIZE;
}

// 初始化路由、端口和用例默认读写参数。
static bool Initialize(TestContext &ctx)
{
    ctx.port = AdsLitePortOpen();
    if (ctx.port == 0)
    {
        std::cerr << "[FAIL] AdsLitePortOpen failed" << std::endl;
        return false;
    }

    AmsNetId netId = {{0}};
    int64_t ret = AdsLiteInitRouting(TEST_REMOTE_ADDR, &netId);
    PrintApiResult("AdsLiteInitRouting", ret);
    if (ret != 0)
    {
        AdsLitePortClose(ctx.port);
        return false;
    }

    ctx.addr = {netId, TEST_PORT};

    ADSPLATFORMID platformId = ADSLITE_PLATFORM_ID_UNKNOWN;
    ret = AdsLiteGetTargetPlatformId(ctx.port, &ctx.addr, &platformId);
    PrintApiResult("AdsLiteGetTargetPlatformId", ret);

    char systemId[ADSLITE_SYSTEM_ID_BUFFER_LENGTH] = {0};
    const int64_t systemIdRet = AdsLiteGetTargetSystemId(ctx.port,
                                                         &ctx.addr,
                                                         systemId,
                                                         ADSLITE_SYSTEM_ID_BUFFER_LENGTH);
    PrintApiResult("AdsLiteGetTargetSystemId", systemIdRet);

    if (ret == 0 && systemIdRet == 0)
    {
        std::cout << "  platformId=" << static_cast<int>(platformId)
                  << ", systemId=" << systemId << std::endl;
    }

    AdsLiteSyncSetTimeout(ctx.port, 2000);
    ctx.writeFlags = ADSLITE_FOPEN_WRITE |
                     ADSLITE_FOPEN_BINARY |
                     ADSLITE_FOPEN_PLUS |
                     ADSLITE_FOPEN_ENSURE_DIR;
    ctx.readFlags = ADSLITE_FOPEN_READ |
                    ADSLITE_FOPEN_BINARY;
    return true;
}

// 统一释放本轮测试占用的路由和端口资源。
static void Shutdown(TestContext &ctx)
{
    AdsLiteShutdownRouting(&ctx.addr.netId);
    AdsLitePortClose(ctx.port);
}

int main()
{
    TestContext ctx = {};
    if (!Initialize(ctx))
    {
        return 1;
    }

    int passed = 0;
    int failed = 0;

    std::cout << "\n[INFO] Running full regression set" << std::endl;

    if (TestAdsLiteDirCreate(ctx))
        passed++;
    else
        failed++;

    if (TestAdsLiteDirDelete(ctx))
        passed++;
    else
        failed++;

    if (TestAdsLiteFileOpen(ctx))
        passed++;
    else
        failed++;

    if (TestAdsLiteFileClose(ctx))
        passed++;
    else
        failed++;

    if (TestAdsLiteFileWrite(ctx))
        passed++;
    else
        failed++;

    if (TestAdsLiteFileRead(ctx))
        passed++;
    else
        failed++;

    if (TestAdsLiteFileDelete(ctx))
        passed++;
    else
        failed++;

    if (TestAdsLiteFileRename(ctx))
        passed++;
    else
        failed++;

    if (TestAdsLiteFileList(ctx))
        passed++;
    else
        failed++;

    if (TestAdsLiteDirDeleteRecursive(ctx))
        passed++;
    else
        failed++;

    if (TestRegressionMixedSeparators(ctx))
        passed++;
    else
        failed++;

    if (TestRegressionDirDeleteKeepRoot(ctx))
        passed++;
    else
        failed++;

    if (TestRegressionFileListSmallBuffer(ctx))
        passed++;
    else
        failed++;

    if (TestFinalDirectoryState(ctx))
        passed++;
    else
        failed++;

    std::cout << "\n[SUMMARY] passed=" << passed << ", failed=" << failed << std::endl;
    Shutdown(ctx);
    return failed == 0 ? 0 : 1;
}
