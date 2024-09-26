using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;

namespace SQLiteDecryptor
{
    class Program
    {
        // Constants
        const int IV_SIZE = 16;
        const int HMAC_SHA1_SIZE = 20;
        const int KEY_SIZE = 32;
        const int DEFAULT_PAGESIZE = 4096; // PC default
        const int DEFAULT_ITER = 64000;
        const string SQLITE_FILE_HEADER = "SQLite format 3";


        static byte[] HexStringToByteArray(string hex)
        {
            if (hex.Length % 2 != 0)
                throw new ArgumentException("Invalid length of hex string");

            byte[] bytes = new byte[hex.Length / 2];
            for (int i = 0; i < hex.Length; i += 2)
            {
                bytes[i / 2] = Convert.ToByte(hex.Substring(i, 2), 16);
            }
            return bytes;
        }
        static void Main(string[] args)
        {
            string dbFilename;

            if (args.Length >= 1)
            {
                dbFilename = args[0];
            }
            else
            {
                Console.Write("请输入文件名: ");
                dbFilename = Console.ReadLine();
            }

            if (string.IsNullOrEmpty(dbFilename) || !File.Exists(dbFilename))
            {
                Console.WriteLine("文件不存在或文件名为空!");
                return;
            }

            try
            {
                DecryptDb(dbFilename);
                Console.WriteLine("解密成功!");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解密失败: {ex.Message}");
            }
        }

        static void DecryptDb(string dbFilename)
        {
            byte[] dbBuffer = File.ReadAllBytes(dbFilename);
            long fileSize = dbBuffer.Length;

            if (fileSize < IV_SIZE)
                throw new Exception("文件大小不足以包含IV.");

            // Extract salt (first 16 bytes)
            byte[] salt = new byte[16];
            Array.Copy(dbBuffer, 0, salt, 0, 16);

            // Prepare MAC salt by XOR-ing with 0x3a
            byte[] macSalt = new byte[16];
            Array.Copy(salt, macSalt, 16);
            for (int i = 0; i < macSalt.Length; i++)
            {
                macSalt[i] ^= 0x3a;
            }
            var pass = HexStringToByteArray("3287815B38014EDB9F0D7C5771AE4B3813F2699504F44676AA27383BCD1B76B1");
            // Derive encryption key
            byte[] key = PBKDF2(pass, salt, DEFAULT_ITER, KEY_SIZE);

            // Derive MAC key
            byte[] macKey = PBKDF2(key, macSalt, 2, KEY_SIZE);

            // Calculate reserve size
            int reserve = IV_SIZE + HMAC_SHA1_SIZE;
            reserve = (reserve % 16 == 0) ? reserve : ((reserve / 16) + 1) * 16;

            // Prepare output file
            string decFile = $"dec_{dbFilename}";
            using (FileStream fsOut = new FileStream(decFile, FileMode.Create, FileAccess.Write))
            {
                int nPage = 1;
                int offset = 16; // Initial offset after salt

                for (long p = 0; p < fileSize; p += DEFAULT_PAGESIZE)
                {
                    if (p + DEFAULT_PAGESIZE > fileSize)
                        throw new Exception($"文件大小不是{DEFAULT_PAGESIZE}的倍数.");

                    byte[] page = new byte[DEFAULT_PAGESIZE];
                    Array.Copy(dbBuffer, p, page, 0, DEFAULT_PAGESIZE);

                    Console.WriteLine($"解密数据页: {nPage}/{fileSize / DEFAULT_PAGESIZE}");

                    // Verify HMAC
                    byte[] computedHmac = ComputeHMAC(page, offset, DEFAULT_PAGESIZE - reserve - offset, macKey, nPage);
                    byte[] storedHmac = new byte[HMAC_SHA1_SIZE];
                    Array.Copy(page, DEFAULT_PAGESIZE - reserve + IV_SIZE, storedHmac, 0, HMAC_SHA1_SIZE);

                    if (!CompareBytes(computedHmac, storedHmac))
                        throw new Exception($"第 {nPage} 页的哈希值错误!");

                    // Decrypt the page
                    byte[] iv = new byte[IV_SIZE];
                    Array.Copy(page, DEFAULT_PAGESIZE - reserve, iv, 0, IV_SIZE);

                    byte[] encryptedData = new byte[DEFAULT_PAGESIZE - reserve - offset];
                    Array.Copy(page, offset, encryptedData, 0, encryptedData.Length);

                    byte[] decryptedData = DecryptAES256CBC(encryptedData, key, iv);

                    // Prepare decrypted page buffer
                    byte[] decryptedPage = new byte[DEFAULT_PAGESIZE];
                    if (nPage == 1)
                    {
                        byte[] headerBytes = Encoding.ASCII.GetBytes(SQLITE_FILE_HEADER);
                        Array.Copy(headerBytes, decryptedPage, headerBytes.Length);
                    }

                    Array.Copy(decryptedData, 0, decryptedPage, offset, decryptedData.Length);
                    Array.Copy(page, DEFAULT_PAGESIZE - reserve, decryptedPage, DEFAULT_PAGESIZE - reserve, reserve);

                    // Write decrypted page to output file
                    fsOut.Write(decryptedPage, 0, decryptedPage.Length);

                    nPage++;
                }
            }
        }

        static byte[] PBKDF2(byte[] password, byte[] salt, int iterations, int keyLength)
        {
            using (var pbkdf2 = new Rfc2898DeriveBytes(password, salt, iterations, HashAlgorithmName.SHA1))
            {
                return pbkdf2.GetBytes(keyLength);
            }
        }

        static byte[] ComputeHMAC(byte[] page, int dataOffset, int dataLength, byte[] macKey, int pageNumber)
        {
            using (var hmac = new HMACSHA1(macKey))
            {
                // Data to hash: encrypted data + page number (little endian)
                hmac.TransformBlock(page, dataOffset, dataLength, null, 0);

                byte[] pageNumberBytes = BitConverter.GetBytes(pageNumber);
                if (!BitConverter.IsLittleEndian)
                    Array.Reverse(pageNumberBytes);
                hmac.TransformFinalBlock(pageNumberBytes, 0, pageNumberBytes.Length);

                return hmac.Hash;
            }
        }

        static bool CompareBytes(byte[] a, byte[] b)
        {
            if (a.Length != b.Length)
                return false;
            for (int i = 0; i < a.Length; i++)
                if (a[i] != b[i])
                    return false;
            return true;
        }

        static byte[] DecryptAES256CBC(byte[] cipherText, byte[] key, byte[] iv)
        {
            using (Aes aes = Aes.Create())
            {
                aes.KeySize = 256;
                aes.BlockSize = 128;
                aes.Mode = CipherMode.CBC;
                aes.Padding = PaddingMode.None;
                aes.Key = key;
                aes.IV = iv;

                using (var decryptor = aes.CreateDecryptor(aes.Key, aes.IV))
                {
                    return decryptor.TransformFinalBlock(cipherText, 0, cipherText.Length);
                }
            }
        }
    }
}
