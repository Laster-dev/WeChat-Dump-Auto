#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 4096

// 初始化Winsock
int initialize_winsock()
{
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
}

// 清理Winsock
void cleanup_winsock()
{
    WSACleanup();
}

// 读取服务器响应
int read_response(SOCKET sock, char* response, int size)
{
    int received = recv(sock, response, size - 1, 0);
    if (received == SOCKET_ERROR || received == 0)
        return -1;
    response[received] = '\0';
    return received;
}

// 发送命令并读取响应
int send_command(SOCKET sock, const char* cmd, char* response, int size)
{
    if (send(sock, cmd, strlen(cmd), 0) == SOCKET_ERROR)
        return -1;
    return read_response(sock, response, size);
}

// 解析FTP URL
int parse_ftp_url(const char* ftp_url, char* host, char* path, char* file)
{
    // 简单解析，假设格式为 ftp://host/path/to/file
    const char* p;
    if (strncmp(ftp_url, "ftp://", 6) != 0)
        return -1;
    p = ftp_url + 6;
    const char* slash = strchr(p, '/');
    if (!slash)
        return -1;
    strncpy(host, p, slash - p);
    host[slash - p] = '\0';
    strcpy(path, slash); // 包含前导 '/'
    const char* last_slash = strrchr(path, '/');
    if (last_slash)
        strcpy(file, last_slash + 1);
    else
        return -1;
    return 0;
}

// 连接到服务器
SOCKET connect_server(const char* host, int port)
{
    struct hostent* he;
    struct sockaddr_in server_addr;
    SOCKET sock;

    // 获取主机信息
    he = gethostbyname(host);
    if (he == NULL)
        return INVALID_SOCKET;

    // 创建套接字
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        return INVALID_SOCKET;

    // 设置服务器地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr = *((struct in_addr*)he->h_addr);
    memset(&(server_addr.sin_zero), 0, 8);

    // 连接服务器
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == SOCKET_ERROR)
    {
        closesocket(sock);
        return INVALID_SOCKET;
    }

    return sock;
}

// 登录到FTP服务器
int login_ftp(SOCKET sock, const char* username, const char* password)
{
    char response[BUFFER_SIZE];
    char cmd[BUFFER_SIZE];

    // 读取欢迎消息
    if (read_response(sock, response, BUFFER_SIZE) < 0)
        return -1;

    // 发送 USER 命令
    snprintf(cmd, sizeof(cmd), "USER %s\r\n", username);
    if (send_command(sock, cmd, response, BUFFER_SIZE) < 0)
        return -1;

    // 发送 PASS 命令
    snprintf(cmd, sizeof(cmd), "PASS %s\r\n", password);
    if (send_command(sock, cmd, response, BUFFER_SIZE) < 0)
        return -1;

    // 检查登录是否成功（响应码 230 表示登录成功）
    if (strncmp(response, "230", 3) != 0 && strncmp(response, "202", 3) != 0) // 202：命令不需要登录
        return -1;

    return 0;
}

// 设置为二进制模式
int set_binary_mode(SOCKET sock)
{
    char response[BUFFER_SIZE];
    char cmd[] = "TYPE I\r\n";
    if (send_command(sock, cmd, response, BUFFER_SIZE) < 0)
        return -1;
    if (strncmp(response, "200", 3) != 0)
        return -1;
    return 0;
}

// 进入被动模式，获取数据连接信息
int enter_passive_mode(SOCKET sock, char* ip, int* port)
{
    char response[BUFFER_SIZE];
    char cmd[] = "PASV\r\n";
    if (send_command(sock, cmd, response, BUFFER_SIZE) < 0)
        return -1;

    // 解析PASV响应，例如: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2).
    char* p1 = strchr(response, '(');
    char* p2 = strchr(response, ')');
    if (!p1 || !p2)
        return -1;
    char nums[128];
    strncpy(nums, p1 + 1, p2 - p1 - 1);
    nums[p2 - p1 - 1] = '\0';

    int h1, h2, h3, h4, p_1, p_2;
    if (sscanf(nums, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p_1, &p_2) != 6)
        return -1;

    sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);
    *port = p_1 * 256 + p_2;

    return 0;
}

// 上传文件到FTP服务器
int upload_file(const char* ftp_url, const char* username, const char* password, const char* local_file_path)
{
    char host[256], path[512], file[256];
    if (parse_ftp_url(ftp_url, host, path, file) != 0)
    {
        fprintf(stderr, "解析FTP URL失败。\n");
        return -1;
    }

    // 连接到FTP服务器
    SOCKET control_sock = connect_server(host, 21);
    if (control_sock == INVALID_SOCKET)
    {
        fprintf(stderr, "无法连接到FTP服务器。\n");
        return -1;
    }

    // 登录
    if (login_ftp(control_sock, username, password) != 0)
    {
        fprintf(stderr, "FTP登录失败。\n");
        closesocket(control_sock);
        return -1;
    }

    // 设置二进制模式
    if (set_binary_mode(control_sock) != 0)
    {
        fprintf(stderr, "无法设置二进制模式。\n");
        closesocket(control_sock);
        return -1;
    }

    // 进入被动模式
    char data_ip[64];
    int data_port;
    if (enter_passive_mode(control_sock, data_ip, &data_port) != 0)
    {
        fprintf(stderr, "进入被动模式失败。\n");
        closesocket(control_sock);
        return -1;
    }

    // 连接到数据端口
    SOCKET data_sock = connect_server(data_ip, data_port);
    if (data_sock == INVALID_SOCKET)
    {
        fprintf(stderr, "无法连接到数据端口。\n");
        closesocket(control_sock);
        return -1;
    }

    // 发送 STOR 命令
    char response[BUFFER_SIZE];
    char cmd[BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "STOR %s\r\n", file);
    if (send_command(control_sock, cmd, response, BUFFER_SIZE) < 0)
    {
        fprintf(stderr, "发送 STOR 命令失败。\n");
        closesocket(data_sock);
        closesocket(control_sock);
        return -1;
    }

    // 检查服务器是否准备好接收数据（响应码 150 或 125）
    if (strncmp(response, "150", 3) != 0 && strncmp(response, "125", 3) != 0)
    {
        fprintf(stderr, "服务器无法接收数据: %s\n", response);
        closesocket(data_sock);
        closesocket(control_sock);
        return -1;
    }

    // 打开本地文件
    FILE* file_fp = fopen(local_file_path, "rb");
    if (!file_fp)
    {
        fprintf(stderr, "无法打开本地文件: %s\n", local_file_path);
        closesocket(data_sock);
        closesocket(control_sock);
        return -1;
    }

    // 获取文件大小
    fseek(file_fp, 0, SEEK_END);
    long file_size = ftell(file_fp);
    fseek(file_fp, 0, SEEK_SET);

    // 上传文件并显示进度
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    long total_sent = 0;
    printf("开始上传文件...\n");
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file_fp)) > 0)
    {
        if (send(data_sock, buffer, bytes_read, 0) == SOCKET_ERROR)
        {
            fprintf(stderr, "发送数据失败。\n");
            fclose(file_fp);
            closesocket(data_sock);
            closesocket(control_sock);
            return -1;
        }
        total_sent += bytes_read;
        double progress = (double)total_sent / file_size * 100;
        printf("\r上传进度: %.2f%%", progress);
        fflush(stdout);
    }
    printf("\n");

    fclose(file_fp);
    closesocket(data_sock);

    // 读取服务器的最终响应
    if (read_response(control_sock, response, BUFFER_SIZE) < 0)
    {
        fprintf(stderr, "读取服务器响应失败。\n");
        closesocket(control_sock);
        return -1;
    }

    if (strncmp(response, "226", 3) != 0 && strncmp(response, "250", 3) != 0)
    {
        fprintf(stderr, "上传可能未成功: %s\n", response);
        closesocket(control_sock);
        return -1;
    }

    printf("文件上传完成！\n");
    closesocket(control_sock);
    return 0;
}

// 封装为静态函数
static int FTP_UploadFile(const char* ftp_url, const char* username, const char* password, const char* local_file_path)
{
    // 初始化Winsock
    if (initialize_winsock() != 0)
    {
        fprintf(stderr, "Winsock初始化失败。\n");
        return -1;
    }

    int result = upload_file(ftp_url, username, password, local_file_path);

    // 清理Winsock
    cleanup_winsock();
    return result;
}

//int main(int argc, char* argv[])
//{
//    if (argc != 5)
//    {
//        printf("用法: %s <ftp_url> <username> <password> <local_file_path>\n", argv[0]);
//        printf("示例: %s ftp://yourftpserver.com/path/to/upload/file.txt yourusername yourpassword C:\\path\\to\\file.txt\n", argv[0]);
//        return 1;
//    }
//
//    const char* ftp_url = argv[1];
//    const char* username = argv[2];
//    const char* password = argv[3];
//    const char* local_file_path = argv[4];
//
//    return FTP_UploadFile(ftp_url, username, password, local_file_path);
//}
