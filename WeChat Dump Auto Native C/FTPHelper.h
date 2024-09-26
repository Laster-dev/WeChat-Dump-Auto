#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 4096

// ��ʼ��Winsock
int initialize_winsock()
{
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
}

// ����Winsock
void cleanup_winsock()
{
    WSACleanup();
}

// ��ȡ��������Ӧ
int read_response(SOCKET sock, char* response, int size)
{
    int received = recv(sock, response, size - 1, 0);
    if (received == SOCKET_ERROR || received == 0)
        return -1;
    response[received] = '\0';
    return received;
}

// ���������ȡ��Ӧ
int send_command(SOCKET sock, const char* cmd, char* response, int size)
{
    if (send(sock, cmd, strlen(cmd), 0) == SOCKET_ERROR)
        return -1;
    return read_response(sock, response, size);
}

// ����FTP URL
int parse_ftp_url(const char* ftp_url, char* host, char* path, char* file)
{
    // �򵥽����������ʽΪ ftp://host/path/to/file
    const char* p;
    if (strncmp(ftp_url, "ftp://", 6) != 0)
        return -1;
    p = ftp_url + 6;
    const char* slash = strchr(p, '/');
    if (!slash)
        return -1;
    strncpy(host, p, slash - p);
    host[slash - p] = '\0';
    strcpy(path, slash); // ����ǰ�� '/'
    const char* last_slash = strrchr(path, '/');
    if (last_slash)
        strcpy(file, last_slash + 1);
    else
        return -1;
    return 0;
}

// ���ӵ�������
SOCKET connect_server(const char* host, int port)
{
    struct hostent* he;
    struct sockaddr_in server_addr;
    SOCKET sock;

    // ��ȡ������Ϣ
    he = gethostbyname(host);
    if (he == NULL)
        return INVALID_SOCKET;

    // �����׽���
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        return INVALID_SOCKET;

    // ���÷�������ַ
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr = *((struct in_addr*)he->h_addr);
    memset(&(server_addr.sin_zero), 0, 8);

    // ���ӷ�����
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == SOCKET_ERROR)
    {
        closesocket(sock);
        return INVALID_SOCKET;
    }

    return sock;
}

// ��¼��FTP������
int login_ftp(SOCKET sock, const char* username, const char* password)
{
    char response[BUFFER_SIZE];
    char cmd[BUFFER_SIZE];

    // ��ȡ��ӭ��Ϣ
    if (read_response(sock, response, BUFFER_SIZE) < 0)
        return -1;

    // ���� USER ����
    snprintf(cmd, sizeof(cmd), "USER %s\r\n", username);
    if (send_command(sock, cmd, response, BUFFER_SIZE) < 0)
        return -1;

    // ���� PASS ����
    snprintf(cmd, sizeof(cmd), "PASS %s\r\n", password);
    if (send_command(sock, cmd, response, BUFFER_SIZE) < 0)
        return -1;

    // ����¼�Ƿ�ɹ�����Ӧ�� 230 ��ʾ��¼�ɹ���
    if (strncmp(response, "230", 3) != 0 && strncmp(response, "202", 3) != 0) // 202�������Ҫ��¼
        return -1;

    return 0;
}

// ����Ϊ������ģʽ
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

// ���뱻��ģʽ����ȡ����������Ϣ
int enter_passive_mode(SOCKET sock, char* ip, int* port)
{
    char response[BUFFER_SIZE];
    char cmd[] = "PASV\r\n";
    if (send_command(sock, cmd, response, BUFFER_SIZE) < 0)
        return -1;

    // ����PASV��Ӧ������: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2).
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

// �ϴ��ļ���FTP������
int upload_file(const char* ftp_url, const char* username, const char* password, const char* local_file_path)
{
    char host[256], path[512], file[256];
    if (parse_ftp_url(ftp_url, host, path, file) != 0)
    {
        fprintf(stderr, "����FTP URLʧ�ܡ�\n");
        return -1;
    }

    // ���ӵ�FTP������
    SOCKET control_sock = connect_server(host, 21);
    if (control_sock == INVALID_SOCKET)
    {
        fprintf(stderr, "�޷����ӵ�FTP��������\n");
        return -1;
    }

    // ��¼
    if (login_ftp(control_sock, username, password) != 0)
    {
        fprintf(stderr, "FTP��¼ʧ�ܡ�\n");
        closesocket(control_sock);
        return -1;
    }

    // ���ö�����ģʽ
    if (set_binary_mode(control_sock) != 0)
    {
        fprintf(stderr, "�޷����ö�����ģʽ��\n");
        closesocket(control_sock);
        return -1;
    }

    // ���뱻��ģʽ
    char data_ip[64];
    int data_port;
    if (enter_passive_mode(control_sock, data_ip, &data_port) != 0)
    {
        fprintf(stderr, "���뱻��ģʽʧ�ܡ�\n");
        closesocket(control_sock);
        return -1;
    }

    // ���ӵ����ݶ˿�
    SOCKET data_sock = connect_server(data_ip, data_port);
    if (data_sock == INVALID_SOCKET)
    {
        fprintf(stderr, "�޷����ӵ����ݶ˿ڡ�\n");
        closesocket(control_sock);
        return -1;
    }

    // ���� STOR ����
    char response[BUFFER_SIZE];
    char cmd[BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "STOR %s\r\n", file);
    if (send_command(control_sock, cmd, response, BUFFER_SIZE) < 0)
    {
        fprintf(stderr, "���� STOR ����ʧ�ܡ�\n");
        closesocket(data_sock);
        closesocket(control_sock);
        return -1;
    }

    // ���������Ƿ�׼���ý������ݣ���Ӧ�� 150 �� 125��
    if (strncmp(response, "150", 3) != 0 && strncmp(response, "125", 3) != 0)
    {
        fprintf(stderr, "�������޷���������: %s\n", response);
        closesocket(data_sock);
        closesocket(control_sock);
        return -1;
    }

    // �򿪱����ļ�
    FILE* file_fp = fopen(local_file_path, "rb");
    if (!file_fp)
    {
        fprintf(stderr, "�޷��򿪱����ļ�: %s\n", local_file_path);
        closesocket(data_sock);
        closesocket(control_sock);
        return -1;
    }

    // ��ȡ�ļ���С
    fseek(file_fp, 0, SEEK_END);
    long file_size = ftell(file_fp);
    fseek(file_fp, 0, SEEK_SET);

    // �ϴ��ļ�����ʾ����
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    long total_sent = 0;
    printf("��ʼ�ϴ��ļ�...\n");
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file_fp)) > 0)
    {
        if (send(data_sock, buffer, bytes_read, 0) == SOCKET_ERROR)
        {
            fprintf(stderr, "��������ʧ�ܡ�\n");
            fclose(file_fp);
            closesocket(data_sock);
            closesocket(control_sock);
            return -1;
        }
        total_sent += bytes_read;
        double progress = (double)total_sent / file_size * 100;
        printf("\r�ϴ�����: %.2f%%", progress);
        fflush(stdout);
    }
    printf("\n");

    fclose(file_fp);
    closesocket(data_sock);

    // ��ȡ��������������Ӧ
    if (read_response(control_sock, response, BUFFER_SIZE) < 0)
    {
        fprintf(stderr, "��ȡ��������Ӧʧ�ܡ�\n");
        closesocket(control_sock);
        return -1;
    }

    if (strncmp(response, "226", 3) != 0 && strncmp(response, "250", 3) != 0)
    {
        fprintf(stderr, "�ϴ�����δ�ɹ�: %s\n", response);
        closesocket(control_sock);
        return -1;
    }

    printf("�ļ��ϴ���ɣ�\n");
    closesocket(control_sock);
    return 0;
}

// ��װΪ��̬����
static int FTP_UploadFile(const char* ftp_url, const char* username, const char* password, const char* local_file_path)
{
    // ��ʼ��Winsock
    if (initialize_winsock() != 0)
    {
        fprintf(stderr, "Winsock��ʼ��ʧ�ܡ�\n");
        return -1;
    }

    int result = upload_file(ftp_url, username, password, local_file_path);

    // ����Winsock
    cleanup_winsock();
    return result;
}

//int main(int argc, char* argv[])
//{
//    if (argc != 5)
//    {
//        printf("�÷�: %s <ftp_url> <username> <password> <local_file_path>\n", argv[0]);
//        printf("ʾ��: %s ftp://yourftpserver.com/path/to/upload/file.txt yourusername yourpassword C:\\path\\to\\file.txt\n", argv[0]);
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
