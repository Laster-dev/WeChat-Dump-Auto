#include <Windows.h>
#include <bcrypt.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cctype>
#include <cstring>

#pragma comment(lib, "bcrypt.lib")

using namespace std;

#undef _UNICODE
#define SQLITE_FILE_HEADER "SQLite format 3"
#define IV_SIZE 16
#define HMAC_SHA1_SIZE 20
#define KEY_SIZE 32
#define AES_BLOCK_SIZE 16  // 定义 AES 块大小

#define SL3SIGNLEN 20

#ifndef ANDROID_WECHAT
#define DEFAULT_PAGESIZE 4096       // 4048数据 + 16IV + 20 HMAC + 12
#define DEFAULT_ITER 64000
#else
#define NO_USE_HMAC_SHA1
#define DEFAULT_PAGESIZE 1024
#define DEFAULT_ITER 4000
#endif

char dbfilename[260]; // 增大缓冲区以防止缓冲区溢出
unsigned char pass[KEY_SIZE] = { 0 }; // 初始化为0
int Decryptdb();

// 辅助函数：检查 BCRYPT 状态
bool CheckNTStatus(NTSTATUS status, const char* message) {
    if (!BCRYPT_SUCCESS(status)) {
        cerr << message << " Error: " << hex << status << endl;
        return false;
    }
    return true;
}

// 辅助函数：将单个十六进制字符转换为其数值
unsigned char HexCharToByte(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = tolower(c);
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0xFF; // 无效字符
}

// 辅助函数：将十六进制字符串转换为字节数组
bool HexStringToByteArray(const string& hexStr, unsigned char* byteArray, size_t byteArraySize) {
    if (hexStr.length() != byteArraySize * 2) {
        cerr << "错误：十六进制字符串长度不正确。需要 " << byteArraySize * 2 << " 个字符。" << endl;
        return false;
    }

    for (size_t i = 0; i < byteArraySize; ++i) {
        unsigned char high = HexCharToByte(hexStr[2 * i]);
        unsigned char low = HexCharToByte(hexStr[2 * i + 1]);
        if (high == 0xFF || low == 0xFF) {
            cerr << "错误：十六进制字符串包含无效字符。" << endl;
            return false;
        }
        byteArray[i] = (high << 4) | low;
    }
    return true;
}

int main(int argc, char* argv[])
{
    string hexPass;

    if (argc >= 3) { // 第一个参数是程序名，第二个是文件名，第三个是pass
        strncpy_s(dbfilename, sizeof(dbfilename), argv[1], _TRUNCATE);  // 安全复制
        hexPass = argv[2];
    }
    else {
        // 提示用户输入文件名
        cout << "请输入文件名: ";
        cin >> dbfilename;

        // 提示用户输入十六进制密码
        cout << "请输入32字节（64个十六进制字符）的密码: ";
        cin >> hexPass;
    }

    // 转换十六进制字符串到字节数组
    if (!HexStringToByteArray(hexPass, pass, KEY_SIZE)) {
        cerr << "密码转换失败！" << endl;
        return 1;
    }

    // 可选：打印转换后的密码以验证（**生产环境中请勿这样做！**）
    /*
    cout << "Converted Password: ";
    for(int i = 0; i < KEY_SIZE; ++i) {
        printf("%02x", pass[i]);
    }
    cout << endl;
    */

    if (Decryptdb() != 0) {
        cerr << "解密失败！" << endl;
        return 1;
    }
    return 0;
}

int Decryptdb()
{
    // 打开文件
    FILE* fpdb;
    fopen_s(&fpdb, dbfilename, "rb+");
    if (!fpdb)
    {
        printf("打开文件错误!\n");
        getchar();
        return -1;
    }

    // 获取文件大小
    fseek(fpdb, 0, SEEK_END);
    long nFileSize = ftell(fpdb);
    fseek(fpdb, 0, SEEK_SET);

    // 读取文件内容
    vector<unsigned char> pDbBuffer(nFileSize);
    size_t readBytes = fread(pDbBuffer.data(), 1, nFileSize, fpdb);
    fclose(fpdb);

    if (readBytes != nFileSize) {
        cerr << "错误：读取文件失败。" << endl;
        return -1;
    }

    // 提取盐值
    if (nFileSize < 16) {
        cerr << "错误：文件大小不足以包含盐值。" << endl;
        return -1;
    }

    unsigned char salt[16] = { 0 };
    memcpy(salt, pDbBuffer.data(), 16);

#ifndef NO_USE_HMAC_SHA1
    unsigned char mac_salt[16] = { 0 };
    memcpy(mac_salt, salt, 16);
    for (int i = 0; i < sizeof(salt); i++)
    {
        mac_salt[i] ^= 0x3a;
    }
#endif

    int reserve = IV_SIZE;      // 校验码长度, PC端每4096字节有48字节
#ifndef NO_USE_HMAC_SHA1
    reserve += HMAC_SHA1_SIZE;
#endif
    reserve = ((reserve % AES_BLOCK_SIZE) == 0) ? reserve : ((reserve / AES_BLOCK_SIZE) + 1) * AES_BLOCK_SIZE;

    unsigned char key[KEY_SIZE] = { 0 };
    unsigned char mac_key[KEY_SIZE] = { 0 };

    NTSTATUS status;
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    PBYTE pbDerivedKey = key;
    DWORD cbDerivedKey = KEY_SIZE;

    // 初始化算法句柄
    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA1_ALGORITHM, NULL, 0);
    if (!CheckNTStatus(status, "无法打开 SHA1 算法提供者")) return -1;

    // 打印 hAlg 以调试
    cout << "hAlg Handle after open: " << hAlg << endl;

    // 使用 PBKDF2 派生密钥
    status = BCryptDeriveKeyPBKDF2(
        hAlg,
        pass,
        sizeof(pass),
        salt,
        sizeof(salt),
        DEFAULT_ITER,
        pbDerivedKey,
        cbDerivedKey,
        0
    );
    if (!CheckNTStatus(status, "PBKDF2 密钥派生失败")) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return -1;
    }

    // 可选：打印派生的密钥以验证（**生产环境中请勿这样做！**）
    /*
    cout << "Derived Key: ";
    for(int i = 0; i < KEY_SIZE; ++i) {
        printf("%02x", key[i]);
    }
    cout << endl;
    */

#ifndef NO_USE_HMAC_SHA1
    // 派生 HMAC 密钥
    status = BCryptDeriveKeyPBKDF2(
        hAlg,
        key,
        sizeof(key),
        mac_salt,
        sizeof(mac_salt),
        2,
        mac_key,
        sizeof(mac_key),
        0
    );
    if (!CheckNTStatus(status, "HMAC 密钥派生失败")) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return -1;
    }
#endif

    BCryptCloseAlgorithmProvider(hAlg, 0);

    unsigned char* pTemp = pDbBuffer.data();
    unsigned char pDecryptPerPageBuffer[DEFAULT_PAGESIZE];
    int nPage = 1;
    int offset = 16;
    int totalPages = nFileSize / DEFAULT_PAGESIZE;

    // 打开输出文件
    char decFile[260] = { 0 };
    sprintf_s(decFile, sizeof(decFile), "dec_%s", dbfilename);
    FILE* fpOut;
    fopen_s(&fpOut, decFile, "wb");
    if (!fpOut) {
        cerr << "无法创建输出文件!" << endl;
        return -1;
    }

    while (pTemp < pDbBuffer.data() + nFileSize)
    {
        printf("解密数据页: %d/%d\n", nPage, totalPages);

#ifndef NO_USE_HMAC_SHA1
        // 计算 HMAC-SHA1
        BCRYPT_ALG_HANDLE hHmacAlg = NULL;
        BCRYPT_HASH_HANDLE hHmacHash = NULL;
        unsigned char hash_mac[HMAC_SHA1_SIZE] = { 0 };
        DWORD cbHash = 0;

        status = BCryptOpenAlgorithmProvider(&hHmacAlg, BCRYPT_SHA1_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG);
        if (!CheckNTStatus(status, "无法打开 HMAC-SHA1 算法提供者")) {

            fclose(fpOut);
            return -1;
        }

        // 创建 HMAC 哈希对象
        status = BCryptCreateHash(hHmacAlg, &hHmacHash, NULL, 0, mac_key, sizeof(mac_key), 0);
        if (!CheckNTStatus(status, "创建 HMAC 哈希失败")) {
            BCryptCloseAlgorithmProvider(hHmacAlg, 0);
            fclose(fpOut);
            return -1;
        }

        // 更新 HMAC 数据
        status = BCryptHashData(hHmacHash, pTemp + offset, DEFAULT_PAGESIZE - reserve - offset + IV_SIZE, 0);
        if (!CheckNTStatus(status, "更新 HMAC 数据失败")) {
            BCryptDestroyHash(hHmacHash);
            BCryptCloseAlgorithmProvider(hHmacAlg, 0);
            fclose(fpOut);
            return -1;
        }

        // 更新页号
        status = BCryptHashData(hHmacHash, (unsigned char*)&nPage, sizeof(nPage), 0);
        if (!CheckNTStatus(status, "更新 HMAC 页号失败")) {
            BCryptDestroyHash(hHmacHash);
            BCryptCloseAlgorithmProvider(hHmacAlg, 0);
            fclose(fpOut);
            return -1;
        }

        // 计算最终 HMAC
        status = BCryptFinishHash(hHmacHash, hash_mac, sizeof(hash_mac), 0);
        if (!CheckNTStatus(status, "完成 HMAC 计算失败")) {
            BCryptDestroyHash(hHmacHash);
            BCryptCloseAlgorithmProvider(hHmacAlg, 0);
            fclose(fpOut);
            return -1;
        }

        BCryptDestroyHash(hHmacHash);
        BCryptCloseAlgorithmProvider(hHmacAlg, 0);

        // 比较 HMAC
        if (memcmp(hash_mac, pTemp + DEFAULT_PAGESIZE - reserve + IV_SIZE, sizeof(hash_mac)) != 0)
        {
            printf("\n哈希值错误!\n");
            fclose(fpOut);
            getchar();
            return -1;
        }
#endif

        // 处理第一页的文件头
        if (nPage == 1)
        {
            memcpy(pDecryptPerPageBuffer, SQLITE_FILE_HEADER, strlen(SQLITE_FILE_HEADER));
            // 填充剩余部分（如果有）
            memset(pDecryptPerPageBuffer + strlen(SQLITE_FILE_HEADER), 0, offset - strlen(SQLITE_FILE_HEADER));
        }

        // AES-256-CBC 解密
        BCRYPT_ALG_HANDLE hAesAlg = NULL;
        BCRYPT_KEY_HANDLE hAesKey = NULL;
        PUCHAR pbIV = pTemp + (DEFAULT_PAGESIZE - reserve); // IV 位于每页的末尾 reserve 字节
        DWORD cbCipherText = DEFAULT_PAGESIZE - reserve - offset;
        PUCHAR pbCipherText = pTemp + offset;
        PUCHAR pbPlainText = pDecryptPerPageBuffer + offset;
        DWORD cbPlainText = cbCipherText;

        // 打开 AES 算法提供者
        status = BCryptOpenAlgorithmProvider(&hAesAlg, BCRYPT_AES_ALGORITHM, NULL, 0);
        if (!CheckNTStatus(status, "无法打开 AES 算法提供者")) {
            fclose(fpOut);
            return -1;
        }

        // 设置链模式为 CBC
        status = BCryptSetProperty(hAesAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
        if (!CheckNTStatus(status, "设置 AES 链模式失败")) {
            BCryptCloseAlgorithmProvider(hAesAlg, 0);
            fclose(fpOut);
            return -1;
        }

        // 生成密钥
        status = BCryptGenerateSymmetricKey(hAesAlg, &hAesKey, NULL, 0, key, sizeof(key), 0);
        if (!CheckNTStatus(status, "生成 AES 密钥失败")) {
            BCryptCloseAlgorithmProvider(hAesAlg, 0);
            fclose(fpOut);
            return -1;
        }

        // 解密
        status = BCryptDecrypt(
            hAesKey,
            pbCipherText,
            cbCipherText,
            NULL,
            pbIV,
            IV_SIZE,
            pbPlainText,
            cbPlainText,
            &cbPlainText,
            0
        );
        if (!CheckNTStatus(status, "AES 解密失败")) {
            BCryptDestroyKey(hAesKey);
            BCryptCloseAlgorithmProvider(hAesAlg, 0);
            fclose(fpOut);
            return -1;
        }

        BCryptDestroyKey(hAesKey);
        BCryptCloseAlgorithmProvider(hAesAlg, 0);

        // 复制保留字段（IV 和 HMAC）
        memcpy(pDecryptPerPageBuffer + DEFAULT_PAGESIZE - reserve, pTemp + DEFAULT_PAGESIZE - reserve, reserve);

        // 写入解密后的数据到输出文件
        fwrite(pDecryptPerPageBuffer, 1, DEFAULT_PAGESIZE, fpOut);

        nPage++;
        offset = 0;
        pTemp += DEFAULT_PAGESIZE;
    }

    fclose(fpOut);
    printf("\n解密成功!\n");
    return 0;
}
