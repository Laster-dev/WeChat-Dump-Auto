using System.IO.Compression;

namespace WeChat_Dump_Auto
{
    public class Config
    {
        //FTP服务器
        public static string ftpurl = "";

        //用户名
        public static string ftp_username = "";

        //密码
        public static string ftp_password = "password";

        //压缩等级
        public static CompressionLevel compressionLevel = CompressionLevel.Fastest;

        //是否排除路径中包含“FileStorage\File”的文件。
        public static bool excludeFileStorageFiles = true;

    }
}
