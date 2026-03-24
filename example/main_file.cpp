#include "AdsLiteAPI.h"
#include "standalone/AmsNetId.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

static const char *TEST_REMOTE_ADDR = "192.168.11.88";
static const uint16_t TEST_PORT = 851;
static const char *FILE_ROOT = "C:/Temp/AdsLite/example_file_api";

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

static bool ResetRoot(TestContext &ctx)
{
    int64_t ret = AdsLiteDirDelete(ctx.port, &ctx.addr, FILE_ROOT, true);
    PrintApiResult("AdsLiteDirDelete(pre-clean)", ret);
    return ret == 0;
}

static bool EnsureRoot(TestContext &ctx)
{
    int64_t ret = AdsLiteDirCreate(ctx.port, &ctx.addr, FILE_ROOT);
    PrintApiResult("AdsLiteDirCreate(root)", ret);
    return ret == 0;
}

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

static bool TestAdsLiteDirCreate(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteDirCreate" << std::endl;
    if (!ResetRoot(ctx))
    {
        return false;
    }

    const char *dirPath = "C:/Temp/AdsLite/example_file_api/case_dir_create";
    int64_t ret = AdsLiteDirCreate(ctx.port, &ctx.addr, dirPath);
    PrintApiResult("AdsLiteDirCreate(case_dir_create)", ret);
    return ret == 0;
}

static bool TestAdsLiteDirDelete(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteDirDelete" << std::endl;
    if (!ResetRoot(ctx))
    {
        return false;
    }

    const char *dirPath = "C:/Temp/AdsLite/example_file_api/case_dir_delete";
    int64_t ret = AdsLiteDirCreate(ctx.port, &ctx.addr, dirPath);
    PrintApiResult("AdsLiteDirCreate(case_dir_delete)", ret);
    if (ret != 0)
    {
        return false;
    }

    ret = AdsLiteDirDelete(ctx.port, &ctx.addr, dirPath, true);
    PrintApiResult("AdsLiteDirDelete(case_dir_delete)", ret);
    return ret == 0;
}

static bool TestAdsLiteFileOpen(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteFileOpen" << std::endl;
    if (!ResetRoot(ctx) || !EnsureRoot(ctx))
    {
        return false;
    }

    const char *path = "C:/Temp/AdsLite/example_file_api/case_open.txt";
    uint32_t handle = 0;
    int64_t ret = AdsLiteFileOpen(ctx.port, &ctx.addr, path, ctx.writeFlags, &handle);
    PrintApiResult("AdsLiteFileOpen(write)", ret);
    std::cout << "  handle=" << handle << std::endl;
    if (ret != 0)
    {
        return false;
    }

    ret = AdsLiteFileClose(ctx.port, &ctx.addr, handle);
    PrintApiResult("AdsLiteFileClose(after open)", ret);
    return ret == 0;
}

static bool TestAdsLiteFileClose(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteFileClose" << std::endl;
    if (!ResetRoot(ctx) || !EnsureRoot(ctx))
    {
        return false;
    }

    const char *path = "C:/Temp/AdsLite/example_file_api/case_close.txt";
    uint32_t handle = 0;
    int64_t ret = AdsLiteFileOpen(ctx.port, &ctx.addr, path, ctx.writeFlags, &handle);
    PrintApiResult("AdsLiteFileOpen(for close)", ret);
    if (ret != 0)
    {
        return false;
    }

    ret = AdsLiteFileClose(ctx.port, &ctx.addr, handle);
    PrintApiResult("AdsLiteFileClose", ret);
    return ret == 0;
}

static bool TestAdsLiteFileWrite(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteFileWrite" << std::endl;
    if (!ResetRoot(ctx) || !EnsureRoot(ctx))
    {
        return false;
    }

    const char *path = "C:/Temp/AdsLite/example_file_api/case_write.txt";
    const char payload[] = "write-api-payload\n";
    uint32_t handle = 0;
    int64_t ret = AdsLiteFileOpen(ctx.port, &ctx.addr, path, ctx.writeFlags, &handle);
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

static bool TestAdsLiteFileRead(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteFileRead" << std::endl;
    if (!ResetRoot(ctx))
    {
        return false;
    }

    const char *path = "C:/Temp/AdsLite/example_file_api/case_read.txt";
    const char payload[] = "read-api-payload\n";
    int64_t ret = WriteWholeFile(ctx,
                                 path,
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
                        path,
                        buffer.data(),
                        static_cast<uint32_t>(buffer.size() - 1),
                        &bytesRead);
    PrintApiResult("ReadWholeFile", ret);
    std::cout << "  bytesRead=" << bytesRead << ", content=" << buffer.data() << std::endl;
    return ret == 0;
}

static bool TestAdsLiteFileDelete(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteFileDelete" << std::endl;
    if (!ResetRoot(ctx))
    {
        return false;
    }

    const char *path = "C:/Temp/AdsLite/example_file_api/case_delete.txt";
    const char payload[] = "delete-api-payload\n";
    int64_t ret = WriteWholeFile(ctx,
                                 path,
                                 payload,
                                 static_cast<uint32_t>(sizeof(payload) - 1));
    PrintApiResult("WriteWholeFile(prepare delete)", ret);
    if (ret != 0)
    {
        return false;
    }

    ret = AdsLiteFileDelete(ctx.port, &ctx.addr, path);
    PrintApiResult("AdsLiteFileDelete", ret);
    return ret == 0;
}

static bool TestAdsLiteFileRename(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteFileRename" << std::endl;
    if (!ResetRoot(ctx))
    {
        return false;
    }

    const char *sourcePath = "C:/Temp/AdsLite/example_file_api/case_rename_src.txt";
    const char *targetPath = "C:/Temp/AdsLite/example_file_api/case_rename_dst.txt";
    const char payload[] = "rename-api-payload\n";

    int64_t ret = WriteWholeFile(ctx,
                                 sourcePath,
                                 payload,
                                 static_cast<uint32_t>(sizeof(payload) - 1));
    PrintApiResult("WriteWholeFile(prepare rename)", ret);
    if (ret != 0)
    {
        return false;
    }

    ret = AdsLiteFileRename(ctx.port, &ctx.addr, sourcePath, targetPath);
    PrintApiResult("AdsLiteFileRename", ret);
    return ret == 0;
}

static bool TestAdsLiteFileList(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteFileList" << std::endl;
    if (!ResetRoot(ctx))
    {
        return false;
    }

    const char *pathA = "C:/Temp/AdsLite/example_file_api/case_list_a.txt";
    const char *pathB = "C:/Temp/AdsLite/example_file_api/case_list_b.txt";
    const char payload[] = "list-api-payload\n";

    int64_t ret = WriteWholeFile(ctx,
                                 pathA,
                                 payload,
                                 static_cast<uint32_t>(sizeof(payload) - 1));
    PrintApiResult("WriteWholeFile(list a)", ret);
    if (ret != 0)
    {
        return false;
    }

    ret = WriteWholeFile(ctx,
                         pathB,
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
                          "C:/Temp/AdsLite/example_file_api/*",
                          listBuffer,
                          static_cast<uint32_t>(sizeof(listBuffer)),
                          &required,
                          &itemCount);
    PrintApiResult("AdsLiteFileList", ret);
    std::cout << "  itemCount=" << itemCount << ", required=" << required << std::endl;
    PrintList(listBuffer);
    return ret == 0;
}

static bool TestAdsLiteDirDeleteRecursive(TestContext &ctx)
{
    std::cout << "\n[CASE] AdsLiteDirDelete(recursive)" << std::endl;
    if (!ResetRoot(ctx))
    {
        return false;
    }

    const char *nestedFile = "C:/Temp/AdsLite/example_file_api/r1/r2/r3/delete_recursive.bin";
    const uint8_t payload[] = {0x01, 0x02, 0x03};
    int64_t ret = WriteWholeFile(ctx,
                                 nestedFile,
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
    AdsLiteSyncSetTimeout(ctx.port, 2000);
    ctx.writeFlags = ADSLITE_FOPEN_WRITE |
                     ADSLITE_FOPEN_BINARY |
                     ADSLITE_FOPEN_PLUS |
                     ADSLITE_FOPEN_ENSURE_DIR;
    ctx.readFlags = ADSLITE_FOPEN_READ |
                    ADSLITE_FOPEN_BINARY;
    return true;
}

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

    std::cout << "\n[SUMMARY] passed=" << passed << ", failed=" << failed << std::endl;
    Shutdown(ctx);
    return failed == 0 ? 0 : 1;
}
