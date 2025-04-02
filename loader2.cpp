#include <windows.h>
#include <iostream>
#include <string>

bool InjectDLL(DWORD processID, const std::string& dllPath) {
    // Open the target process
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
    if (hProcess == NULL) {
        std::cerr << "Failed to open process. Error: " << GetLastError() << std::endl;
        return false;
    }

    // Allocate memory in the target process for the DLL path
    LPVOID remoteMemory = VirtualAllocEx(hProcess, NULL, dllPath.size() + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remoteMemory == NULL) {
        std::cerr << "Failed to allocate memory in target process. Error: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return false;
    }

    // Write the DLL path to the allocated memory
    if (!WriteProcessMemory(hProcess, remoteMemory, dllPath.c_str(), dllPath.size() + 1, NULL)) {
        std::cerr << "Failed to write DLL path to target process. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Get the address of LoadLibraryA from kernel32.dll
    LPVOID loadLibraryAddr = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    if (loadLibraryAddr == NULL) {
        std::cerr << "Failed to get LoadLibraryA address. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Create a remote thread in the target process to load the DLL
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, remoteMemory, 0, NULL);
    if (hThread == NULL) {
        std::cerr << "Failed to create remote thread. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Wait for the thread to finish
    WaitForSingleObject(hThread, INFINITE);

    // Clean up
    VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);

    std::cout << "DLL injected successfully!" << std::endl;
    return true;
}

int main() {
    // Paths to the game executable and the DLL
    std::string gamePath = "C:\\Path\\To\\Your\\Game.exe"; // Replace with your game executable path
    std::string dllPath = "C:\\Path\\To\\Your\\Custom.dll"; // Replace with your DLL path

    // Set up the process creation
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    // Launch the game in a suspended state
    if (!CreateProcessA(gamePath.c_str(), NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        std::cerr << "Failed to start game. Error: " << GetLastError() << std::endl;
        return 1;
    }

    std::cout << "Game process started with PID: " << pi.dwProcessId << std::endl;

    // Inject the DLL
    if (InjectDLL(pi.dwProcessId, dllPath)) {
        // Resume the game process after injection
        ResumeThread(pi.hThread);
        std::cout << "Game resumed with DLL injected." << std::endl;
    } else {
        std::cerr << "DLL injection failed. Terminating process..." << std::endl;
        TerminateProcess(pi.hProcess, 1);
    }

    // Clean up process handles
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return 0;
}