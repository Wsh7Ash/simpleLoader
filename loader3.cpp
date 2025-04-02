#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>

// Function to find process ID by name
DWORD FindProcessId(const std::wstring& processName) {
    PROCESSENTRY32 processInfo;
    processInfo.dwSize = sizeof(processInfo);

    HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (processesSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    Process32First(processesSnapshot, &processInfo);
    if (!processName.compare(processInfo.szExeFile)) {
        CloseHandle(processesSnapshot);
        return processInfo.th32ProcessID;
    }

    while (Process32Next(processesSnapshot, &processInfo)) {
        if (!processName.compare(processInfo.szExeFile)) {
            CloseHandle(processesSnapshot);
            return processInfo.th32ProcessID;
        }
    }

    CloseHandle(processesSnapshot);
    return 0;
}

// Function to inject DLL into target process
bool InjectDLL(DWORD processId, const std::wstring& dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (hProcess == NULL) {
        std::cerr << "Failed to open target process." << std::endl;
        return false;
    }

    // Allocate memory in the target process for the DLL path
    LPVOID pDllPath = VirtualAllocEx(hProcess, NULL, (dllPath.size() + 1) * sizeof(wchar_t), MEM_COMMIT, PAGE_READWRITE);
    if (pDllPath == NULL) {
        std::cerr << "Failed to allocate memory in target process." << std::endl;
        CloseHandle(hProcess);
        return false;
    }

    // Write the DLL path to the allocated memory
    if (!WriteProcessMemory(hProcess, pDllPath, dllPath.c_str(), (dllPath.size() + 1) * sizeof(wchar_t), NULL)) {
        std::cerr << "Failed to write to target process memory." << std::endl;
        VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Get the address of LoadLibraryW in kernel32.dll
    LPVOID pLoadLibrary = (LPVOID)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryW");
    if (pLoadLibrary == NULL) {
        std::cerr << "Failed to get address of LoadLibraryW." << std::endl;
        VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Create remote thread to call LoadLibraryW with our DLL path
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pDllPath, 0, NULL);
    if (hThread == NULL) {
        std::cerr << "Failed to create remote thread." << std::endl;
        VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Wait for the remote thread to complete
    WaitForSingleObject(hThread, INFINITE);

    // Clean up
    VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);

    return true;
}

int main() {
    // Game executable path (change this to your game's path)
    std::wstring gamePath = L"C:\\Path\\To\\YourGame.exe";
    
    // DLL to inject (change this to your DLL's path)
    std::wstring dllPath = L"C:\\Path\\To\\YourDLL.dll";

    // Startup info and process info for CreateProcess
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Start the game
    if (!CreateProcess(
        gamePath.c_str(),   // Application path
        NULL,              // Command line
        NULL,             // Process handle not inheritable
        NULL,             // Thread handle not inheritable
        FALSE,            // Set handle inheritance to FALSE
        CREATE_SUSPENDED, // Create in suspended state
        NULL,             // Use parent's environment block
        NULL,             // Use parent's starting directory 
        &si,             // Pointer to STARTUPINFO structure
        &pi)             // Pointer to PROCESS_INFORMATION structure
        ) {
        std::cerr << "CreateProcess failed (" << GetLastError() << ")." << std::endl;
        return 1;
    }

    std::wcout << L"Game started with PID: " << pi.dwProcessId << std::endl;

    // Inject the DLL
    if (InjectDLL(pi.dwProcessId, dllPath)) {
        std::wcout << L"DLL injected successfully!" << std::endl;
    } else {
        std::cerr << "DLL injection failed." << std::endl;
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 1;
    }

    // Resume the main thread
    ResumeThread(pi.hThread);

    // Clean up
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return 0;
}