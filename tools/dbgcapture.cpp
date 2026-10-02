// ============================================================================
// Module:  tools/dbgcapture.cpp
// Purpose: Capture the mod's Wh_Log output. The Windhawk engine emits mod logs
//          with OutputDebugStringW, so this is a plain DBWIN listener - the same
//          mechanism DebugView uses - with a substring filter and a time limit
//          so it can be driven from a script.
//
// Usage:   dbgcapture.exe [--filter <text>] [--seconds <n>] [--lines <n>]
//          Runs in the caller's session; no elevation needed for messages from
//          processes in the same session, which includes explorer.exe.
//          Only one DBWIN listener can exist at a time: close DebugView first.
// Deps:    kernel32 only.
// ============================================================================

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {

struct DbWinMessage {
    DWORD processId;
    char data[4096 - sizeof(DWORD)];
};

std::string ProcessNameFor(DWORD pid) {
    HANDLE process =
        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return "?";
    }
    char path[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    std::string name = "?";
    if (QueryFullProcessImageNameA(process, 0, path, &size)) {
        const char* slash = strrchr(path, '\\');
        name = slash ? slash + 1 : path;
    }
    CloseHandle(process);
    return name;
}

}  // namespace

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);

    std::string filter;
    int seconds = 0;
    int maxLines = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--filter") == 0 && i + 1 < argc) {
            filter = argv[++i];
        } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            seconds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--lines") == 0 && i + 1 < argc) {
            maxLines = atoi(argv[++i]);
        }
    }

    HANDLE bufferReady = CreateEventW(nullptr, FALSE, TRUE, L"DBWIN_BUFFER_READY");
    HANDLE dataReady = CreateEventW(nullptr, FALSE, FALSE, L"DBWIN_DATA_READY");
    if (!bufferReady || !dataReady) {
        printf("failed to create DBWIN events: %lu\n", GetLastError());
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        printf("another debug output listener is already running "
               "(close DebugView and retry)\n");
        return 1;
    }

    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
                                        PAGE_READWRITE, 0, sizeof(DbWinMessage),
                                        L"DBWIN_BUFFER");
    if (!mapping) {
        printf("failed to create DBWIN_BUFFER: %lu\n", GetLastError());
        return 1;
    }
    auto* shared = static_cast<DbWinMessage*>(
        MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(DbWinMessage)));
    if (!shared) {
        printf("failed to map DBWIN_BUFFER: %lu\n", GetLastError());
        return 1;
    }

    printf("capturing debug output%s%s",
           filter.empty() ? "" : (" filtered on \"" + filter + "\"").c_str(),
           seconds > 0 ? "" : " (ctrl+c to stop)");
    if (seconds > 0) {
        printf(" for %d seconds", seconds);
    }
    printf("\n");

    const ULONGLONG deadline =
        seconds > 0 ? GetTickCount64() + static_cast<ULONGLONG>(seconds) * 1000 : 0;
    int lines = 0;

    for (;;) {
        SetEvent(bufferReady);
        DWORD waitFor = INFINITE;
        if (deadline) {
            const ULONGLONG now = GetTickCount64();
            if (now >= deadline) {
                break;
            }
            waitFor = static_cast<DWORD>(deadline - now);
        }
        if (WaitForSingleObject(dataReady, waitFor) != WAIT_OBJECT_0) {
            break;
        }

        const DWORD pid = shared->processId;
        char text[sizeof(shared->data) + 1];
        memcpy(text, shared->data, sizeof(shared->data));
        text[sizeof(shared->data)] = '\0';

        // Trim the trailing newline the emitter usually includes.
        size_t len = strlen(text);
        while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) {
            text[--len] = '\0';
        }

        if (!filter.empty() && strstr(text, filter.c_str()) == nullptr) {
            continue;
        }

        SYSTEMTIME st;
        GetLocalTime(&st);
        printf("%02d:%02d:%02d.%03d %5lu %-14s %s\n", st.wHour, st.wMinute,
               st.wSecond, st.wMilliseconds, pid, ProcessNameFor(pid).c_str(), text);

        if (maxLines > 0 && ++lines >= maxLines) {
            break;
        }
    }

    UnmapViewOfFile(shared);
    CloseHandle(mapping);
    CloseHandle(dataReady);
    CloseHandle(bufferReady);
    return 0;
}
