#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <psapi.h>
#include <shlobj_core.h>
#include <shlwapi.h>
#include <iomanip>
#include <tlhelp32.h>

#pragma comment(lib, "Shlwapi.lib")
class MemoryScanner
{
public:
    // 用于获取模块的基地址和大小
    static HMODULE GetModuleBaseAddress(DWORD processID, const std::string& moduleName)
    {
        HMODULE hMods[1024];
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
        if (hProcess == NULL)
            return NULL;

        HMODULE hModule = NULL;
        DWORD cbNeeded;
        if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
        {
            for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
            {
                char szModName[MAX_PATH];
                if (GetModuleBaseNameA(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(char)))
                {
                    if (_stricmp(szModName, moduleName.c_str()) == 0)
                    {
                        hModule = hMods[i];
                        break;
                    }
                }
            }
        }
        CloseHandle(hProcess);
        return hModule;
    }
    // ConvertToGB2312 函数定义
    static std::string ConvertToGB2312(const std::vector<uint8_t>& input) {
        // 将 vector 数据转换为 const char*，因为它实际是 GB2312 编码的数据
        const char* inputData = reinterpret_cast<const char*>(input.data());
        int length = input.size();

        // 计算需要的宽字符数
        int wcharNum = MultiByteToWideChar(CP_UTF8, 0, inputData, length, NULL, 0);
        wchar_t* wstr = new wchar_t[wcharNum + 1];
        // UTF-8转宽字符
        MultiByteToWideChar(CP_UTF8, 0, inputData, length, wstr, wcharNum);
        wstr[wcharNum] = L'\0'; // 添加宽字符字符串结束符

        // 计算GB2312需要的字符数
        int charNum = WideCharToMultiByte(936, 0, wstr, -1, NULL, 0, NULL, NULL);
        char* gb2312 = new char[charNum + 1];
        // 宽字符转GB2312
        WideCharToMultiByte(936, 0, wstr, -1, gb2312, charNum, NULL, NULL);
        gb2312[charNum] = '\0'; // 添加字符串结束符

        std::string result(gb2312);
        delete[] wstr;
        delete[] gb2312;
        return result;
    }
    static std::vector<uint8_t> GetWechatUserNameBytes(const std::string& wxid, const std::string& filePath)
    {
        std::vector<uint8_t> wxidBytes(wxid.begin(), wxid.end());

        std::ifstream file(filePath, std::ios::binary);
        std::vector<uint8_t> fileBytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        int index = FindByteArray(fileBytes, wxidBytes);
        if (index != -1)
        {
            std::vector<uint8_t> rearBytes(fileBytes.begin() + index + wxidBytes.size() + 6, fileBytes.end());

            auto it = std::find(rearBytes.begin(), rearBytes.end(), 0x1A);
            if (it != rearBytes.end())
            {
                rearBytes.erase(it, rearBytes.end());
            }
            // 调用 ConvertToGB2312 函数进行转换
            std::string username = ConvertToGB2312(rearBytes);

            std::cout << "微信用户名：" << username << std::endl;
            // 将 rearBytes 转换为十六进制字符串
            std::ostringstream oss;
            for (uint8_t byte : rearBytes)
            {
                oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
            }
            std::string hexString = oss.str();

            std::cout << "微信用户名（十六进制）：" << hexString << std::endl;
            return rearBytes;
        }
        else
        {
            std::cout << "未找到指定字符串" << std::endl;
            return std::vector<uint8_t>();
        }
    }

    static int FindByteArray(const std::vector<uint8_t>& source, const std::vector<uint8_t>& pattern)
    {
        auto it = std::search(source.begin(), source.end(), pattern.begin(), pattern.end());
        if (it != source.end())
        {
            return std::distance(source.begin(), it);
        }
        return -1;
    }

    static std::string GetHex(HANDLE hProcess, LPVOID lpBaseAddress)
    {
        uint8_t array[8];
        SIZE_T bytesRead;
        if (!ReadProcessMemory(hProcess, lpBaseAddress, array, sizeof(array), &bytesRead))
        {
            return "1";
        }

        uint64_t baseAddress2 = *reinterpret_cast<uint64_t*>(array);

        const int num = 32;
        uint8_t array2[num];
        if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(baseAddress2), array2, sizeof(array2), &bytesRead))
        {
            DWORD error = GetLastError();
            std::ostringstream oss;
            oss << "2; Error Code: " << error;
            return oss.str();
        }

        return BytesToHex(array2, num);
    }

    static std::string GetFolderPath()
    {
        char username[256];
        DWORD size = sizeof(username);
        if (GetUserNameA(username, &size) == 0)
        {
            std::cerr << "无法获取用户名" << std::endl;
            return "";
        }

        std::string configPath = "C:\\Users\\" + std::string(username) + "\\AppData\\Roaming\\Tencent\\WeChat\\All Users\\config\\3ebffe94.ini";

        std::ifstream configFile(configPath);
        if (configFile)
        {
            std::string fileContent;
            std::getline(configFile, fileContent);
            if (_stricmp(fileContent.c_str(), "MyDocument:") == 0)
            {
                char myDocuments[MAX_PATH];
                SHGetFolderPathA(NULL, CSIDL_MYDOCUMENTS, NULL, 0, myDocuments);
                return std::string(myDocuments) + "\\WeChat Files";
            }
            else
            {
                return fileContent + "\\WeChat Files";
            }
        }
        else
        {
            std::cerr << "无法打开文件 " << configPath << ", 尝试从注册表读取" << std::endl;

            HKEY hKey;
            if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Tencent\\WeChat", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
            {
                char folderPath[MAX_PATH];
                DWORD pathSize = sizeof(folderPath);
                if (RegQueryValueExA(hKey, "FileSavePath", NULL, NULL, reinterpret_cast<LPBYTE>(folderPath), &pathSize) == ERROR_SUCCESS)
                {
                    RegCloseKey(hKey);
                    return std::string(folderPath);
                }
                else
                {
                    std::cerr << "无法读取注册表项中的 FileSavePath" << std::endl;
                    RegCloseKey(hKey);
                    return "";
                }
            }
            else
            {
                std::cerr << "无法打开注册表项" << std::endl;
                return "";
            }
        }
    }
    static void run(const std::string& wxid, const std::string& filepath)
    {
        std::vector<uint8_t> userNameBytes = GetWechatUserNameBytes(wxid, filepath);

        // 查找WeChat.exe进程
        DWORD processID = 0;
        HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hProcessSnap == INVALID_HANDLE_VALUE)
        {
            std::cerr << "无法获取进程快照" << std::endl;
            return;
        }

        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(hProcessSnap, &pe32))
        {
            do
            {
                if (_stricmp(pe32.szExeFile, "WeChat.exe") == 0)
                {
                    processID = pe32.th32ProcessID;
                    break;
                }
            } while (Process32Next(hProcessSnap, &pe32));
        }
        CloseHandle(hProcessSnap);

        if (processID == 0)
        {
            std::cout << "未找到WeChat进程" << std::endl;
            return;
        }

        HANDLE hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, processID);
        if (hProcess == NULL)
        {
            std::cerr << "无法打开进程" << std::endl;
            return;
        }

        // 获取WeChatWin.dll模块基地址
        HMODULE hMod = GetModuleBaseAddress(processID, "WeChatWin.dll");
        if (hMod == NULL)
        {
            std::cerr << "未找到WeChatWin.dll模块" << std::endl;
            CloseHandle(hProcess);
            return;
        }

        MODULEINFO modInfo;
        if (GetModuleInformation(hProcess, hMod, &modInfo, sizeof(modInfo)))
        {
            LPBYTE moduleBaseAddress = reinterpret_cast<LPBYTE>(modInfo.lpBaseOfDll);
            SIZE_T moduleSize = modInfo.SizeOfImage;

            std::vector<uint8_t> buffer(moduleSize);
            SIZE_T bytesRead;
            if (ReadProcessMemory(hProcess, moduleBaseAddress, buffer.data(), buffer.size(), &bytesRead))
            {
                int matchIndex = FindPattern(buffer, userNameBytes);
                if (matchIndex != -1)
                {
                    LPBYTE userNameAddress = moduleBaseAddress + matchIndex;
                    std::cout << "找到匹配字节，用户名地址: WeChatWin.dll+0x" << std::hex << matchIndex << std::endl;

                    // 根据已知的偏移量关系计算其他字段的地址
                    int weChatIDOffset = 1336; // 微信ID相对于用户名的偏移量
                    int phoneNumberOffset = -192; // 手机号相对于用户名的偏移量
                    int keyOffset = 1272; // Key相对于用户名的偏移量

                    // 获取微信ID地址
                    LPBYTE weChatIDAddress = userNameAddress + weChatIDOffset;
                    std::cout << "微信ID地址: WeChatWin.dll+0x" << std::hex << (matchIndex + weChatIDOffset) << std::endl;

                    // 获取手机号地址
                    LPBYTE phoneNumberAddress = userNameAddress + phoneNumberOffset;
                    std::cout << "手机号地址: WeChatWin.dll+0x" << std::hex << (matchIndex + phoneNumberOffset) << std::endl;

                    // 获取Key地址
                    LPBYTE keyAddress = userNameAddress + keyOffset;
                    std::cout << "Key地址: WeChatWin.dll+0x" << std::hex << (matchIndex + keyOffset) << std::endl;

                    // 读取微信ID
                    char weChatIDBuffer[32];
                    if (ReadProcessMemory(hProcess, weChatIDAddress, weChatIDBuffer, sizeof(weChatIDBuffer), &bytesRead))
                    {
                        std::string weChatID(weChatIDBuffer);
                        std::cout << "微信ID (文本): " << weChatID << std::endl;
                    }
                    else
                    {
                        std::cerr << "无法读取微信ID" << std::endl;
                    }

                    // 读取手机号
                    char phoneNumberBuffer[32];
                    if (ReadProcessMemory(hProcess, phoneNumberAddress, phoneNumberBuffer, sizeof(phoneNumberBuffer), &bytesRead))
                    {
                        std::string phoneNumber(phoneNumberBuffer);
                        std::cout << "手机号 (文本): " << phoneNumber << std::endl;
                    }
                    else
                    {
                        std::cerr << "无法读取手机号" << std::endl;
                    }

                    std::cout << "Key (十六进制): " << GetHex(hProcess, keyAddress) << std::endl;
                }
                else
                {
                    std::cout << "未找到匹配字节" << std::endl;
                }
            }
        }

        CloseHandle(hProcess);
    }


private:
    static int FindPattern(const std::vector<uint8_t>& buffer, const std::vector<uint8_t>& pattern)
    {
        auto it = std::search(buffer.begin(), buffer.end(), pattern.begin(), pattern.end());
        if (it != buffer.end())
        {
            return std::distance(buffer.begin(), it);
        }
        return -1;
    }

    static std::string BytesToHex(const uint8_t* bytes, size_t length)
    {
        std::ostringstream oss;
        for (size_t i = 0; i < length; i++)
        {
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)bytes[i];
        }
        return oss.str();
    }
};

int main(int argc, char* argv[])
{
    std::string path = MemoryScanner::GetFolderPath();
    if (path.empty())
    {
        std::cerr << "指定路径不存在。" << std::endl;
        return 1;
    }

    std::vector<std::string> subdirectories;
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA((path + "\\*").c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0)
            {
                subdirectories.push_back(findData.cFileName);
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }

    for (const auto& subdir : subdirectories)
    {
        std::string msgFolderPath = path + "\\" + subdir + "\\Msg";
        if (PathFileExistsA(msgFolderPath.c_str()))
        {
            MemoryScanner::run(subdir, path + "\\" + subdir + "\\config\\AccInfo.dat");
        }
    }

    return 0;
}
