#pragma once

#include <Windows.h>
#include <TlHelp32.h>

namespace Util {
    inline static DWORD getProcessIdByName(const char* processName) {
        PROCESSENTRY32 processEntry{ sizeof(PROCESSENTRY32) };
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return 0;

        DWORD processId = 0;
        if (Process32First(snapshot, &processEntry)) {
            do {
                if (_stricmp(processEntry.szExeFile, processName) == 0) {
                    processId = processEntry.th32ProcessID;
                    break;
                }
            } while (Process32Next(snapshot, &processEntry));
        }
        CloseHandle(snapshot);
        return processId;
    }

    inline static const bool isRunningAsAdmin() {
        BOOL isAdmin = FALSE;
        PSID adminGroup = NULL;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

        if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
            CheckTokenMembership(NULL, adminGroup, &isAdmin);
            FreeSid(adminGroup);
        }

        return isAdmin;
    }

    inline static void fail(const char* msg) {
        MessageBox(nullptr, msg, "Error", MB_ICONERROR);
        exit(1);
    }
}