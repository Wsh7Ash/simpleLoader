#include <windows.h>
#include <tlhelp32.h>
#include <iostream>

DWORD GetProcessID(const char* processName) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(snap, &entry)) {
        do {
            if (!_stricmp(entry.szExeFile, processName)) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32Next(snap, &entry));
    }
    CloseHandle(snap);
    return pid;
}

bool InjectDLL(DWORD processID, const char* dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
    if (!hProcess) {
        std::cerr << "Failed to open process\n";
        return false;
    }

    void* allocMem = VirtualAllocEx(hProcess, NULL, strlen(dllPath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!allocMem) {
        std::cerr << "Memory allocation failed\n";
        CloseHandle(hProcess);
        return false;
    }

    WriteProcessMemory(hProcess, allocMem, dllPath, strlen(dllPath) + 1, NULL);
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryA"), allocMem, 0, NULL);

    if (!hThread) {
        std::cerr << "Failed to create remote thread\n";
        VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    VirtualFreeEx(hProcess, allocMem, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);
    return true;
}

int main() {
    const char* gameExe = "game.exe";  // Replace with actual game executable name
    const char* dllPath = "C:\\Path\\To\\YourDLL.dll";  // Replace with actual DLL path

    DWORD pid = GetProcessID(gameExe);
    if (!pid) {
        std::cerr << "Game not running!\n";
        return 1;
    }

    if (InjectDLL(pid, dllPath)) {
        std::cout << "DLL successfully injected!\n";
    } else {
        std::cerr << "DLL injection failed.\n";
    }

    return 0;
}

