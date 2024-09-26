using System;
using System.Diagnostics;
using System.Text;
using System.Linq;
using System.Runtime.InteropServices;

class Program
{
    [DllImport("kernel32.dll")]
    private static extern IntPtr OpenProcess(int dwDesiredAccess, bool bInheritHandle, int dwProcessId);

    [DllImport("kernel32.dll")]
    private static extern bool ReadProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, byte[] lpBuffer, int dwSize, out int lpNumberOfBytesRead);

    [DllImport("kernel32.dll")]
    private static extern bool CloseHandle(IntPtr hObject);

    private const int PROCESS_WM_READ = 0x0010;

    static void Main()
    {
        string processName = "Wechat";
        IntPtr targetAddress = (IntPtr)0x23C8AC00D0F; // 目标内存地址
        int readSize = 50004; // 读取1004个字节

        Process process = Process.GetProcessesByName(processName).FirstOrDefault();
        if (process == null)
        {
            Console.WriteLine("Process not found.");
            Console.WriteLine("*********************");
            Console.ReadKey();
            return;
        }
        Console.WriteLine("ID:" + process.Id);
        IntPtr processHandle = OpenProcess(PROCESS_WM_READ, false, process.Id);
        if (processHandle == IntPtr.Zero)
        {
            Console.WriteLine("Failed to open process.");
            Console.WriteLine("*********************");
            Console.ReadKey();
            return;
        }
        Console.WriteLine("processHandle:" + processHandle);

        byte[] resultBuffer = new byte[readSize];
        if (ReadProcessMemory(processHandle, targetAddress+4, resultBuffer, readSize, out int bytesRead))
        {
            Console.WriteLine("Hex Dump:");
            Console.WriteLine(BitConverter.ToString(resultBuffer));

            string utf8Text = Encoding.UTF8.GetString(resultBuffer);

            Console.WriteLine("Encoded Text:");
            Console.WriteLine(utf8Text);
        }
        else
        {
            Console.WriteLine("Failed to read memory.");
        }

        CloseHandle(processHandle);
        Console.WriteLine("*********************");
        Console.ReadKey();
    }
}
