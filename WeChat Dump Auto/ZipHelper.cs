using System;
using System.IO;
using System.IO.Compression;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Linq;

namespace FolderCompressionUtility
{
    /// <summary>
    /// 提供文件夹复制和压缩功能。
    /// </summary>
    public static class FolderCompressor
    {
        /// <summary>
        /// 压缩指定的文件夹到一个ZIP文件。
        /// </summary>
        /// <param name="sourceFolderPath">要压缩的源文件夹路径。</param>
        /// <param name="zipFilePath">目标ZIP文件的路径。</param>
        /// <param name="compressionLevel">压缩等级。</param>
        /// <param name="includeBaseDirectory">是否包括源文件夹的根目录。</param>
        /// <param name="excludeFileStorageFiles">是否排除路径中包含“FileStorage\File”的文件。</param>
        /// <param name="progress">进度报告接口。</param>
        public static async Task CompressFolderAsync(
            string sourceFolderPath,
            string zipFilePath,
            CompressionLevel compressionLevel = CompressionLevel.Optimal,
            bool includeBaseDirectory = false,
            bool excludeFileStorageFiles = true,
            IProgress<double> progress = null)
        {
            // 参数验证
            if (string.IsNullOrWhiteSpace(sourceFolderPath))
                throw new ArgumentException("源文件夹路径不能为空。", nameof(sourceFolderPath));

            if (string.IsNullOrWhiteSpace(zipFilePath))
                throw new ArgumentException("ZIP文件路径不能为空。", nameof(zipFilePath));

            if (!Directory.Exists(sourceFolderPath))
                throw new DirectoryNotFoundException($"源文件夹不存在: {sourceFolderPath}");
            
            // 创建临时文件夹路径
            string tempFolderPath = Path.Combine(Path.GetTempPath(), "FolderCompressorTemp", Guid.NewGuid().ToString());
            init(Path.Combine(Path.GetTempPath(), "FolderCompressorTemp"));
            try
            {
                // 复制文件夹到临时文件夹，排除FileStorage\\File文件
                await Task.Run(() =>
                    CopyDirectory(
                        sourceFolderPath,
                        tempFolderPath,
                        excludeFileStorageFiles ? "FileStorage\\File" : null
                    )
                );

                // 获取所有文件以计算总字节数
                List<string> files = GetAllFiles(tempFolderPath);
                long totalBytes = files.Sum(file => new FileInfo(file).Length);
                long processedBytes = 0;

                // 删除已存在的ZIP文件
                if (File.Exists(zipFilePath))
                {
                    File.Delete(zipFilePath);
                }
                // 创建ZIP归档
                using (FileStream zipToOpen = new FileStream(zipFilePath, FileMode.Create))
                using (ZipArchive archive = new ZipArchive(zipToOpen, ZipArchiveMode.Create))
                {
                    // 设置并行度
                    int maxDegreeOfParallelism = Environment.ProcessorCount;
                    // 创建一个锁对象以确保线程安全地写入ZIP归档
                    object archiveLock = new object();
                    await Task.Run(() =>
                        Parallel.ForEach(files, new ParallelOptions { MaxDegreeOfParallelism = maxDegreeOfParallelism }, filePath =>
                        {
                            try
                            {
                                // 计算在ZIP中的相对路径
                                string entryName = PathHelper.GetRelativePath(tempFolderPath, filePath);
                                if (includeBaseDirectory)
                                {
                                    string folderName = Path.GetFileName(Path.GetFullPath(tempFolderPath).TrimEnd(Path.DirectorySeparatorChar));
                                    entryName = $"{folderName}/{entryName.Replace("\\", "/")}";
                                }
                                else
                                {
                                    entryName = entryName.Replace("\\", "/"); // 使用统一的路径分隔符
                                }

                                lock (archiveLock)
                                {
                                    ZipArchiveEntry entry = archive.CreateEntry(entryName, compressionLevel);
                                    using (FileStream fsInput = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.Read))
                                    using (Stream entryStream = entry.Open())
                                    {
                                        fsInput.CopyTo(entryStream);
                                    }
                                }

                                // 更新进度
                                long fileLength = new FileInfo(filePath).Length;
                                System.Threading.Interlocked.Add(ref processedBytes, fileLength);
                                double currentProgress = (double)processedBytes / totalBytes * 100;
                                progress?.Report(Math.Min(currentProgress, 100));
                            }
                            catch (Exception ex)
                            {
                                Console.WriteLine($"处理文件 '{filePath}' 时发生错误: {ex.Message}");
                            }
                        })
                    );
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"压缩文件夹时发生错误: {ex.Message}");
                throw;
            }
            finally
            {
                init(tempFolderPath);
            }

        }
        private static void init(string tempFolderPath) {
            // 强制删除临时文件夹
            try
            {
                if (Directory.Exists(tempFolderPath))
                {
                    // 递归设置所有文件的属性为正常，以便删除
                    foreach (string filePath in Directory.GetFiles(tempFolderPath, "*", SearchOption.AllDirectories))
                    {
                        try
                        {
                            File.SetAttributes(filePath, FileAttributes.Normal);
                        }
                        catch (Exception ex)
                        {
                            Console.WriteLine($"无法设置文件属性为正常: {filePath}, 错误: {ex.Message}");
                        }
                    }

                    // 递归设置所有子目录的属性为正常
                    foreach (string dirPath in Directory.GetDirectories(tempFolderPath, "*", SearchOption.AllDirectories))
                    {
                        try
                        {
                            File.SetAttributes(dirPath, FileAttributes.Normal);
                        }
                        catch (Exception ex)
                        {
                            Console.WriteLine($"无法设置目录属性为正常: {dirPath}, 错误: {ex.Message}");
                        }
                    }

                    // 尝试删除临时文件夹及其所有内容
                    Directory.Delete(tempFolderPath, true);
                    Console.WriteLine("临时文件夹已删除。");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"删除临时文件夹时发生错误: {ex.Message}");
            }
        }
        /// <summary>
        /// 复制目录及其内容到目标目录，排除路径中包含特定字符串的文件。
        /// </summary>
        /// <param name="sourceDir">源目录路径。</param>
        /// <param name="destDir">目标目录路径。</param>
        /// <param name="excludePathContains">需要排除的路径包含字符串。如果为null，则不排除任何文件。</param>
        private static void CopyDirectory(string sourceDir, string destDir, string excludePathContains = null)
        {
            DirectoryInfo sourceDirectory = new DirectoryInfo(sourceDir);

            if (!sourceDirectory.Exists)
                throw new DirectoryNotFoundException($"源目录不存在: {sourceDir}");

            DirectoryInfo[] directories = sourceDirectory.GetDirectories();

            // 如果目标目录不存在，则创建它
            if (!Directory.Exists(destDir))
            {
                Directory.CreateDirectory(destDir);
            }

            // 复制所有文件
            foreach (FileInfo file in sourceDirectory.GetFiles())
            {
                if (excludePathContains != null && file.FullName.Contains(excludePathContains))
                {
                    Console.WriteLine($"舍弃文件：{Path.GetFileName( file.FullName)}");
                    continue;
                }

                string targetFilePath = Path.Combine(destDir, file.Name);
                file.CopyTo(targetFilePath, overwrite: true);
            }

            // 递归复制所有子目录
            foreach (DirectoryInfo subDir in directories)
            {
                string targetSubDirPath = Path.Combine(destDir, subDir.Name);
                CopyDirectory(subDir.FullName, targetSubDirPath, excludePathContains);
            }
        }

        /// <summary>
        /// 获取文件夹中所有文件的完整路径。
        /// </summary>
        /// <param name="folderPath">文件夹路径。</param>
        /// <returns>文件路径列表。</returns>
        private static List<string> GetAllFiles(string folderPath)
        {
            try
            {
                List<string> files = Directory.GetFiles(folderPath, "*", SearchOption.AllDirectories).ToList();
                Console.WriteLine($"获取文件列表完成，共 {files.Count} 个文件。");
                return files;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"获取文件列表时发生错误: {ex.Message}");
                throw;
            }
        }
    }

    /// <summary>
    /// 提供路径相关的辅助方法。
    /// </summary>
    public static class PathHelper
    {
        /// <summary>
        /// 获取从 basePath 到 targetPath 的相对路径。
        /// </summary>
        /// <param name="basePath">基路径。</param>
        /// <param name="targetPath">目标路径。</param>
        /// <returns>相对路径。</returns>
        public static string GetRelativePath(string basePath, string targetPath)
        {
            if (string.IsNullOrEmpty(basePath))
                throw new ArgumentNullException(nameof(basePath));

            if (string.IsNullOrEmpty(targetPath))
                throw new ArgumentNullException(nameof(targetPath));

            Uri baseUri = new Uri(AppendDirectorySeparatorChar(basePath));
            Uri targetUri = new Uri(targetPath);

            Uri relativeUri = baseUri.MakeRelativeUri(targetUri);
            string relativePath = Uri.UnescapeDataString(relativeUri.ToString());

            // 转换为本地路径分隔符
            return relativePath.Replace('/', Path.DirectorySeparatorChar);
        }

        /// <summary>
        /// 确保路径以目录分隔符结尾。
        /// </summary>
        /// <param name="path">路径字符串。</param>
        /// <returns>确保以目录分隔符结尾的路径。</returns>
        private static string AppendDirectorySeparatorChar(string path)
        {
            if (!path.EndsWith(Path.DirectorySeparatorChar.ToString()))
            {
                return path + Path.DirectorySeparatorChar;
            }
            return path;
        }
    }


}
