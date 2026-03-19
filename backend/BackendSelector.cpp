#include "backend/BackendSelector.h"

#include "backend/StandaloneBackend.h"
#include "backend/TwinCATBackend.h"
#include "standalone/Log.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")
#endif

namespace adslite
{
    namespace backend
    {
        namespace
        {
            struct BackendState
            {
                StandaloneBackend standalone;
                TwinCATBackend twincat;
                IAdsBackend *selected = nullptr;
                const char *name = "standalone";
                std::string reason = "default standalone";
            };

            std::string ToLower(std::string value)
            {
                std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                               { return static_cast<char>(std::tolower(c)); });
                return value;
            }

            std::string GetBackendMode()
            {
#ifdef _WIN32
                // Windows 下使用 _dupenv_s，避免 MSVC 对 getenv 的安全告警。
                char *value = nullptr;
                size_t len = 0;
                if (_dupenv_s(&value, &len, "ADSLITE_BACKEND") != 0 || !value)
                {
                    return "auto";
                }

                const std::string mode = ToLower(value);
                free(value);
                return mode.empty() ? "auto" : mode;
#else
                const char *value = std::getenv("ADSLITE_BACKEND");
                if (!value)
                {
                    return "auto";
                }
                const std::string mode = ToLower(value);
                return mode.empty() ? "auto" : mode;
#endif
            }

#ifdef _WIN32
            bool TryGetListeningProcessNameOnPort(uint16_t port, std::string &processName)
            {
                DWORD tableSize = 0;
                if (GetExtendedTcpTable(nullptr, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0) != ERROR_INSUFFICIENT_BUFFER)
                {
                    return false;
                }

                std::string process;
                auto buffer = std::string(tableSize, '\0');
                auto table = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(&buffer[0]);
                if (GetExtendedTcpTable(table, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0) != NO_ERROR)
                {
                    return false;
                }

                for (DWORD i = 0; i < table->dwNumEntries; ++i)
                {
                    const auto &row = table->table[i];
                    const uint16_t rowPort = ntohs(static_cast<u_short>(row.dwLocalPort));
                    if (rowPort != port)
                    {
                        continue;
                    }

                    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, row.dwOwningPid);
                    if (!handle)
                    {
                        processName = "pid=" + std::to_string(row.dwOwningPid);
                        return true;
                    }

                    char imagePath[MAX_PATH] = {0};
                    DWORD len = MAX_PATH;
                    if (!QueryFullProcessImageNameA(handle, 0, imagePath, &len))
                    {
                        CloseHandle(handle);
                        processName = "pid=" + std::to_string(row.dwOwningPid);
                        return true;
                    }

                    CloseHandle(handle);
                    process = imagePath;
                    const auto sep = process.find_last_of("\\/");
                    processName = (sep == std::string::npos) ? process : process.substr(sep + 1);
                    return true;
                }

                return false;
            }

            bool IsTwinCATEnvironmentDetected(std::string &details)
            {
                std::string processName;
                if (!TryGetListeningProcessNameOnPort(48898, processName))
                {
                    details = "no process listening on tcp/48898";
                    return false;
                }

                const auto lowered = ToLower(processName);
                if (lowered == "tcatsyssrv.exe")
                {
                    details = "tcp/48898 listener is " + processName;
                    return true;
                }

                details = "tcp/48898 listener is " + processName;
                return false;
            }
#endif

            BackendState &State()
            {
                static BackendState state;
                static bool initialized = false;
                if (initialized)
                {
                    return state;
                }

                initialized = true;
                state.selected = &state.standalone;

                const std::string mode = GetBackendMode();

#if defined(_WIN32)
                std::string detectDetails;
                const bool twinCatDetected = IsTwinCATEnvironmentDetected(detectDetails);

                if (mode == "standalone")
                {
                    state.reason = "ADSLITE_BACKEND=standalone";
                    LOG_INFO("Backend selected: standalone (forced)");
                    return state;
                }

                if (mode == "twincat")
                {
                    if (!twinCatDetected)
                    {
                        state.reason = "ADSLITE_BACKEND=twincat but TwinCAT runtime is not detected (" + detectDetails + ")";
                        LOG_WARN("Backend requested: twincat, runtime not detected, fallback to standalone (" << detectDetails << ")");
                        return state;
                    }

                    if (state.twincat.IsAvailable())
                    {
                        state.selected = &state.twincat;
                        state.name = "twincat";
                        state.reason = "ADSLITE_BACKEND=twincat";
                        LOG_INFO("Backend selected: twincat (forced)");
                        return state;
                    }

                    state.reason = std::string("ADSLITE_BACKEND=twincat but unavailable: ") + state.twincat.AvailabilityReason();
                    LOG_WARN("Backend requested: twincat, fallback to standalone: " << state.twincat.AvailabilityReason());
                    return state;
                }

                if (twinCatDetected)
                {
                    if (state.twincat.IsAvailable())
                    {
                        state.selected = &state.twincat;
                        state.name = "twincat";
                        state.reason = "auto mode detected TwinCAT environment: " + detectDetails;
                        LOG_INFO("Backend auto-detect: twincat (" << detectDetails << ")");
                        return state;
                    }

                    state.reason = "auto mode detected TwinCAT environment but twincat backend unavailable: " + std::string(state.twincat.AvailabilityReason());
                    LOG_WARN("Backend auto-detect: TwinCAT detected but backend unavailable (" << state.twincat.AvailabilityReason() << "), using standalone");
                }
                else
                {
                    state.reason = "auto mode: no TwinCAT environment detected (" + detectDetails + ")";
                    LOG_INFO("Backend auto-detect: standalone (" << detectDetails << ")");
                }
#else
                (void)mode;
                state.reason = "non-Windows platform, standalone only";
                LOG_INFO("Backend selected: standalone (non-Windows)");
#endif

                return state;
            }
        } // namespace

        IAdsBackend &GetBackend()
        {
            return *State().selected;
        }

        const char *GetBackendName()
        {
            return State().name;
        }

        const char *GetBackendSelectionReason()
        {
            return State().reason.c_str();
        }

    } // namespace backend
} // namespace adslite
