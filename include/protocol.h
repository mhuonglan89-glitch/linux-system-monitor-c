#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define SERVER_PORT 8888
#define BUFFER_SIZE 32768
#define MAX_CLIENTS 32

// Bang mau ANSI dinh dang Terminal
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_WHITE   "\033[1;37m"
#define COLOR_BOLD    "\033[1m"

// Nguong canh bao tai nguyen (%)
#define THRESHOLD_CPU_WARN 75.0
#define THRESHOLD_RAM_WARN 80.0

// Mã lệnh xác thực và mật khẩu bí mật
#define CMD_AUTH_LOGIN       99
#define AUTH_PASSWORD        "VKU_MONITOR_2026"

// Mã lệnh kiểm tra mạng
#define CMD_NET_INSPECT      11

// Ma lenh dieu khien giua Server va Client
enum CommandType {
    CMD_TOTAL_CLIENTS    = 1,
    CMD_BASIC_INFO       = 2,
    CMD_RESOURCE_STATUS  = 3,
    CMD_PROCESS_DETAILS  = 4,
    CMD_KILL_PROCESS     = 5,
    CMD_SEND_ALERT       = 6,
    CMD_REMOTE_TERMINAL  = 7,
    CMD_SEND_FILE        = 8,
    CMD_STREAM_MONITOR   = 9,
    CMD_EXPORT_CSV       = 10,
    CMD_DISCONNECT       = 0
};

// Cau truc goi tin mang
typedef struct {
    int client_id;
    int command;
    int data_len;
    int is_warning;
    char payload[BUFFER_SIZE];
} Packet;

#endif
