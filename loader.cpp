#include <Windows.h>
#include <iostream>
#include <string>
#include <TlHelp32.h>

// Function to get process ID by process name
DWORD GetProcessIdByName(const std::string& processName) {
    DWORD processId = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 processEntry;
        processEntry.dwSize = sizeof(PROCESSENTRY32);
        
        if (Process32First(snapshot, &processEntry)) {
            do {
                // Convert wide char to string for comparison
                std::wstring wProcessName(processEntry.szExeFile);
                std::string currentProcessName(wProcessName.begin(), wProcessName.end());
                
                if (_stricmp(currentProcessName.c_str(), processName.c_str()) == 0) {
                    processId = processEntry.th32ProcessID;
                    break;
                }
            } while (Process32Next(snapshot, &processEntry));
        }
        CloseHandle(snapshot);
    }
    
    return processId;
}

// Function to inject DLL into a process
bool InjectDLL(DWORD processId, const std::string& dllPath) {
    bool success = false;
    
    // Open the process
    HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (processHandle == NULL) {
        std::cerr << "Error: Could not open process (Error code: " << GetLastError() << ")" << std::endl;
        return false;
    }
    
    // Allocate memory for the DLL path in the target process
    LPVOID dllPathAddress = VirtualAllocEx(processHandle, NULL, dllPath.length() + 1, 
                                          MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    
    if (dllPathAddress == NULL) {
        std::cerr << "Error: Could not allocate memory in target process (Error code: " << GetLastError() << ")" << std::endl;
        CloseHandle(processHandle);
        return false;
    }
    
    // Write the DLL path to the allocated memory
    if (!WriteProcessMemory(processHandle, dllPathAddress, dllPath.c_str(), 
                           dllPath.length() + 1, NULL)) {
        std::cerr << "Error: Could not write to process memory (Error code: " << GetLastError() << ")" << std::endl;
        VirtualFreeEx(processHandle, dllPathAddress, 0, MEM_RELEASE);
        CloseHandle(processHandle);
        return false;
    }
    
    // Get the address of LoadLibraryA function
    LPVOID loadLibraryAddress = (LPVOID)GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryA");
    
    if (loadLibraryAddress == NULL) {
        std::cerr << "Error: Could not find LoadLibraryA function (Error code: " << GetLastError() << ")" << std::endl;
        VirtualFreeEx(processHandle, dllPathAddress, 0, MEM_RELEASE);
        CloseHandle(processHandle);
        return false;
    }
    
    // Create a remote thread that calls LoadLibraryA with the DLL path as argument
    HANDLE remoteThread = CreateRemoteThread(processHandle, NULL, 0, 
                                            (LPTHREAD_START_ROUTINE)loadLibraryAddress, 
                                            dllPathAddress, 0, NULL);
    
    if (remoteThread == NULL) {
        std::cerr << "Error: Could not create remote thread (Error code: " << GetLastError() << ")" << std::endl;
        VirtualFreeEx(processHandle, dllPathAddress, 0, MEM_RELEASE);
        CloseHandle(processHandle);
        return false;
    }
    
    // Wait for the thread to finish
    WaitForSingleObject(remoteThread, INFINITE);
    
    // Check if the DLL was loaded successfully
    DWORD exitCode = 0;
    if (GetExitCodeThread(remoteThread, &exitCode) && exitCode != 0) {
        std::cout << "DLL injected successfully!" << std::endl;
        success = true;
    } else {
        std::cerr << "Error: DLL injection failed (Error code: " << GetLastError() << ")" << std::endl;
    }
    
    // Clean up
    CloseHandle(remoteThread);
    VirtualFreeEx(processHandle, dllPathAddress, 0, MEM_RELEASE);
    CloseHandle(processHandle);
    
    return success;
}

// Function to launch a process
DWORD LaunchProcess(const std::string& executablePath) {
    STARTUPINFO startupInfo = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION processInfo = { 0 };
    
    // Create a mutable copy of the path string
    char* cmdLine = _strdup(executablePath.c_str());
    
    // Create the process
    if (CreateProcess(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo)) {
        std::cout << "Successfully launched process: " << executablePath << std::endl;
        std::cout << "Process ID: " << processInfo.dwProcessId << std::endl;
        
        // Clean up handles but don't wait for the process to terminate
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        
        free(cmdLine);
        return processInfo.dwProcessId;
    } else {
        std::cerr << "Error: Failed to launch process (Error code: " << GetLastError() << ")" << std::endl;
        free(cmdLine);
        return 0;
    }
}

int main(int argc, char* argv[]) {
    std::string gameExecutable;
    std::string dllPath;
    bool waitForProcess = false;
    int waitTimeMs = 3000; // Default wait time: 3 seconds
    
    // Check command line arguments
    if (argc < 3) {
        std::cout << "Game Launcher with DLL Injection" << std::endl;
        std::cout << "Usage: " << argv[0] << " [game_executable] [dll_path] [wait_time_ms]" << std::endl;
        std::cout << std::endl;
        
        // If no arguments, prompt for input
        std::cout << "Enter game executable path: ";
        std::getline(std::cin, gameExecutable);
        
        std::cout << "Enter DLL path to inject: ";
        std::getline(std::cin, dllPath);
        
        std::string waitTimeStr;
        std::cout << "Enter wait time in milliseconds (default 3000, 0 for no wait): ";
        std::getline(std::cin, waitTimeStr);
        
        if (!waitTimeStr.empty()) {
            waitTimeMs = std::stoi(waitTimeStr);
        }
    } else {
        gameExecutable = argv[1];
        dllPath = argv[2];
        
        if (argc > 3) {
            waitTimeMs = std::stoi(argv[3]);
        }
    }
    
    // Launch the game process
    DWORD processId = LaunchProcess(gameExecutable);
    
    if (processId == 0) {
        std::cerr << "Failed to launch the game. Exiting." << std::endl;
        return 1;
    }
    
    // Wait if specified
    if (waitTimeMs > 0) {
        std::cout << "Waiting " << waitTimeMs << "ms before injection..." << std::endl;
        Sleep(waitTimeMs);
    }
    
    // Get process information
    std::string processName = gameExecutable.substr(gameExecutable.find_last_of("/\\") + 1);
    
    // If we launched the process, we already know the PID
    // But we could also try to find it again to make sure it's running
    if (processId == 0) {
        processId = GetProcessIdByName(processName);
        if (processId == 0) {
            std::cerr << "Error: Could not find process: " << processName << std::endl;
            return 1;
        }
    }
    
    std::cout << "Found process ID: " << processId << std::endl;
    
    // Inject the DLL
    if (InjectDLL(processId, dllPath)) {
        std::cout << "Successfully injected DLL: " << dllPath << " into process: " << processName << std::endl;
        return 0;
    } else {
        std::cerr << "Failed to inject DLL. Exiting." << std::endl;
        return 1;
    }
}