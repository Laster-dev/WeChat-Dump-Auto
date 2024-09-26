using System;
using System.IO;
using System.Net;

class FTPHelper
{
    /// <summary>
    /// 上传文件到FTP服务器并显示上传进度。
    /// </summary>
    /// <param name="ftpUrl">FTP服务器的URL，例如 "ftp://yourftpserver.com/path/to/upload/"</param>
    /// <param name="localFilePath">本地文件的完整路径，例如 @"C:\path\to\your\file.txt"</param>
    /// <param name="username">FTP用户名</param>
    /// <param name="password">FTP密码</param>
    public static void UploadFileWithProgress(string ftpUrl, string localFilePath, string username, string password)
    {
        string fileName = Path.GetFileName(localFilePath);
        string uploadUrl = $"{ftpUrl}{fileName}";

        try
        {
            FileInfo fileInfo = new FileInfo(localFilePath);
            long totalBytes = fileInfo.Length;
            long uploadedBytes = 0;

            // 创建FTP请求
            FtpWebRequest request = (FtpWebRequest)WebRequest.Create(uploadUrl);
            request.Method = WebRequestMethods.Ftp.UploadFile;
            request.Credentials = new NetworkCredential(username, password);
            request.UseBinary = true;
            request.ContentLength = totalBytes;
            request.UsePassive = true;
            int bufferSize = 2048;
            byte[] buffer = new byte[bufferSize];
            int bytesRead = 0;

            using (FileStream fs = new FileStream(localFilePath, FileMode.Open, FileAccess.Read))
            using (Stream requestStream = request.GetRequestStream())
            {
                Console.WriteLine("开始上传文件...");
                while ((bytesRead = fs.Read(buffer, 0, buffer.Length)) > 0)
                {
                    requestStream.Write(buffer, 0, bytesRead);
                    uploadedBytes += bytesRead;
                    double progress = (double)uploadedBytes / totalBytes * 100;
                    Console.Write($"\r上传进度: {progress:F2}%");
                }
            }

            // 获取响应以确保上传成功
            using (FtpWebResponse response = (FtpWebResponse)request.GetResponse())
            {
                Console.WriteLine($"\n上传完成，服务器响应: {response.StatusDescription}");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"\n上传失败: {ex.Message}");
        }
    }
}
