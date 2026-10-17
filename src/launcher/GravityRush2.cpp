#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <string>
#include <vector>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

static bool FileExists(const std::wstring& path) {
    DWORD dwAttrib = GetFileAttributesW(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

static bool DirExists(const std::wstring& path) {
    DWORD dwAttrib = GetFileAttributesW(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. Determine directory where GravityRush2.exe is located
    wchar_t exePath[MAX_PATH * 2] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH * 2);
    PathRemoveFileSpecW(exePath);
    std::wstring baseDir = exePath;

    // 2. Locate eboot.bin
    std::vector<std::wstring> gameCandidates = {
        baseDir + L"\\game\\eboot.bin",
        baseDir + L"\\game\\merged_game\\eboot.bin",
        baseDir + L"\\gr2_packages\\merged_game\\eboot.bin",
        baseDir + L"\\..\\gr2_packages\\merged_game\\eboot.bin"
    };

    std::wstring foundEboot = L"";
    for (const auto& cand : gameCandidates) {
        if (FileExists(cand)) {
            foundEboot = cand;
            break;
        }
    }

    // If game files not found, show user-friendly guidance
    if (foundEboot.empty()) {
        std::wstring msg = L"Welcome to Gravity Rush 2 PC Port!\n\n"
                           L"The game executable (eboot.bin) was not found.\n\n"
                           L"To play:\n"
                           L"1. Run 'Install_Game_PKG.bat' to install from your PS4 PKG files, OR\n"
                           L"2. Place your decrypted game files into the 'game' folder.\n\n"
                           L"Would you like to open the game directory now?";

        int ret = MessageBoxW(NULL, msg.c_str(), L"Gravity Rush 2 PC Port", MB_ICONINFORMATION | MB_YESNO | MB_DEFBUTTON1);
        if (ret == IDYES) {
            std::wstring gameDir = baseDir + L"\\game";
            if (!DirExists(gameDir)) {
                CreateDirectoryW(gameDir.c_str(), NULL);
            }
            ShellExecuteW(NULL, L"open", baseDir.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
        return 1;
    }

    // 3. Locate core runtime (shadPS4.exe)
    std::vector<std::wstring> coreCandidates = {
        baseDir + L"\\core\\shadPS4.exe",
        baseDir + L"\\exodus_core\\shadPS4.exe",
        baseDir + L"\\shadPS4.exe"
    };

    std::wstring foundCore = L"";
    std::wstring coreWorkingDir = L"";
    for (const auto& cand : coreCandidates) {
        if (FileExists(cand)) {
            foundCore = cand;
            wchar_t tempDir[MAX_PATH * 2] = { 0 };
            wcscpy_s(tempDir, cand.c_str());
            PathRemoveFileSpecW(tempDir);
            coreWorkingDir = tempDir;
            break;
        }
    }

    if (foundCore.empty()) {
        MessageBoxW(NULL, L"Core runtime (shadPS4.exe) was not found in 'core\\' or 'exodus_core\\'.\nPlease ensure all port files are extracted properly.",
                    L"Gravity Rush 2 PC Port - Error", MB_ICONERROR | MB_OK);
        return 2;
    }

    // 4. Locate patch file
    std::vector<std::wstring> patchCandidates = {
        coreWorkingDir + L"\\user\\patches\\GravityDaze2-Orbis.xml",
        baseDir + L"\\core\\user\\patches\\GravityDaze2-Orbis.xml",
        baseDir + L"\\exodus_core\\user\\patches\\GravityDaze2-Orbis.xml"
    };

    std::wstring foundPatch = L"";
    for (const auto& cand : patchCandidates) {
        if (FileExists(cand)) {
            foundPatch = cand;
            break;
        }
    }

    // 5. Construct command line
    std::wstring cmdLine = L"\"" + foundCore + L"\" -g \"" + foundEboot + L"\"";
    if (!foundPatch.empty()) {
        cmdLine += L" -p \"" + foundPatch + L"\"";
    }
    cmdLine += L" --show-fps";

    // 6. Launch silently without a console window
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(0);

    BOOL success = CreateProcessW(
        foundCore.c_str(),
        cmdBuffer.data(),
        NULL,
        NULL,
        FALSE,
        0, // Standard GUI launch, no console window allocated
        NULL,
        coreWorkingDir.c_str(),
        &si,
        &pi
    );

    if (!success) {
        DWORD err = GetLastError();
        wchar_t errMsg[256];
        wsprintfW(errMsg, L"Failed to launch game process. Error code: %lu", err);
        MessageBoxW(NULL, errMsg, L"Gravity Rush 2 PC Port - Error", MB_ICONERROR | MB_OK);
        return 3;
    }

    // 7. Wait for the game process to exit
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (int)exitCode;
}
