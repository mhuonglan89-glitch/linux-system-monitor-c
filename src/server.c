#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include "protocol.h"

typedef struct {
    int socket_fd;
    int id;
    struct sockaddr_in address;
    int active;
} ClientNode;

ClientNode clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void write_log(const char *level, const char *msg) {
    FILE *f = fopen("logs/server_activity.log", "a");
    if (!f) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    fprintf(f, "[%s] [%s] %s\n", time_str, level, msg);
    fclose(f);
}

void render_master_menu() {
    printf(COLOR_BLUE "\n+=============================================================+\n" COLOR_RESET);
    printf(COLOR_BLUE "|        SYSTEM MONITORING & ANALYSIS CONTROL CENTER          |\n" COLOR_RESET);
    printf(COLOR_BLUE "+=============================================================+\n" COLOR_RESET);
    printf("  " COLOR_CYAN "1." COLOR_RESET " Danh Sach Cac Client Dang Ket Noi (Total Clients)\n");
    printf("  " COLOR_CYAN "2." COLOR_RESET " Xem Thong Tin Phan Cung Co Ban (Hardware & OS)\n");
    printf("  " COLOR_CYAN "3." COLOR_RESET " Xem Trang Thai Tai Nguyen (Snapshot Resource)\n");
    printf("  " COLOR_CYAN "4." COLOR_RESET " Xem Bang Tien Trinh Chi Tiet (Process Table)\n");
    printf("  " COLOR_CYAN "5." COLOR_RESET " Tieu Diet Tien Trinh Tu Xa (Kill Process)\n");
    printf("  " COLOR_CYAN "6." COLOR_RESET " Phat Thong Bao Khan Cap Den Client (Broadcast/Alert)\n");
    printf("  " COLOR_CYAN "7." COLOR_RESET " Thuc Thi Lenh Terminal Tu Xa (Remote Shell)\n");
    printf("  " COLOR_CYAN "8." COLOR_RESET " Truyen Tep Tin Xuong Client (File Transfer)\n");
    printf("  " COLOR_CYAN "9." COLOR_RESET " Che Do Dashboard Tu Dong Cap Nhat (Live Monitor)\n");
    printf("  " COLOR_CYAN "10." COLOR_RESET "Xuat Anh Chup Tien Trinh Ra File CSV (Export Report)\n");
    printf("  " COLOR_RED  "0." COLOR_RESET " Thoat Chuong Trinh (Exit)\n");
    printf(COLOR_BLUE "+=============================================================+\n" COLOR_RESET);
    printf(COLOR_BOLD "Lua Chon Cua Ban [0-10]: " COLOR_RESET);
}

void *listener_thread(void *arg) {
    int server_fd = *(int*)arg;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (1) {
        int newsock = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (newsock < 0) continue;

        pthread_mutex_lock(&lock);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].active) {
                clients[i].socket_fd = newsock;
                clients[i].id = i + 1;
                clients[i].address = client_addr;
                clients[i].active = 1;
                client_count++;

                char log_buf[128];
                snprintf(log_buf, sizeof(log_buf), "Client ID %d connected from IP: %s",
                         clients[i].id, inet_ntoa(client_addr.sin_addr));
                write_log("INFO", log_buf);

                printf(COLOR_GREEN "\n[NEW CONNECTION] Client ID [%d] ket noi tu %s\n" COLOR_RESET,
                       clients[i].id, inet_ntoa(client_addr.sin_addr));
                break;
            }
        }
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int get_target_socket(int *target_id) {
    if (client_count == 0) {
        printf(COLOR_RED "\n[CANH BAO] Hien tai chua co Client nao truc tuyen!\n" COLOR_RESET);
        return -1;
    }
    printf(COLOR_YELLOW "Nhap ID Client can thao tac: " COLOR_RESET);
    if (scanf("%d", target_id) != 1) {
        getchar();
        return -1;
    }
    getchar();

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].id == *target_id) {
            return clients[i].socket_fd;
        }
    }
    printf(COLOR_RED "Khong tim thay Client ID hop le!\n" COLOR_RESET);
    return -1;
}

// Ham ve thanh tien trinh ASCII truc quan voi do dai 25 ky tu
void print_ascii_bar(const char *label, double percentage) {
    if (percentage < 0.0) percentage = 0.0;
    if (percentage > 100.0) percentage = 100.0;

    int bar_width = 25;
    int filled = (int)((percentage / 100.0) * bar_width);

    // Chon mau sac dua vao nguong phan tram
    const char *bar_color = COLOR_GREEN;
    if (percentage >= 80.0) {
        bar_color = COLOR_RED;
    } else if (percentage >= 60.0) {
        bar_color = COLOR_YELLOW;
    }

    printf(" %-15s [", label);
    printf("%s", bar_color);
    for (int i = 0; i < filled; i++) {
        printf("|");
    }
    printf(COLOR_RESET);
    for (int i = filled; i < bar_width; i++) {
        printf(".");
    }
    printf("] %s%5.1f%%%s\n", bar_color, percentage, COLOR_RESET);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Bind error");
        return 1;
    }
    listen(server_fd, 16);

    memset(clients, 0, sizeof(clients));
    write_log("SYSTEM", "Master Server started.");

    pthread_t th;
    pthread_create(&th, NULL, listener_thread, &server_fd);

    printf(COLOR_GREEN "=== MASTER SERVER ONLINE AT PORT %d ===\n" COLOR_RESET, SERVER_PORT);

    int choice;
    Packet send_pkt, recv_pkt;

    while (1) {
        render_master_menu();
        if (scanf("%d", &choice) != 1) break;
        getchar();

        if (choice == 0) break;

        if (choice == CMD_TOTAL_CLIENTS) {
            printf(COLOR_GREEN "\nTong so Client truc tuyen: %d\n" COLOR_RESET, client_count);
            printf("+------+--------------------+----------------+\n");
            printf("|  ID  |     IP ADDRESS     |     PORT       |\n");
            printf("+------+--------------------+----------------+\n");
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].active) {
                    printf("| %-4d | %-18s | %-14d |\n",
                           clients[i].id,
                           inet_ntoa(clients[i].address.sin_addr),
                           ntohs(clients[i].address.sin_port));
                }
            }
            printf("+------+--------------------+----------------+\n");
            continue;
        }

        int target_id = 0;
        int target_sock = get_target_socket(&target_id);
        if (target_sock == -1) continue;

        memset(&send_pkt, 0, sizeof(Packet));
        memset(&recv_pkt, 0, sizeof(Packet));
        send_pkt.command = choice;

        if (choice == CMD_KILL_PROCESS) {
            printf("Nhap PID can tieu diet: ");
            fgets(send_pkt.payload, sizeof(send_pkt.payload), stdin);
            send_pkt.payload[strcspn(send_pkt.payload, "\r\n")] = 0;
        } else if (choice == CMD_SEND_ALERT) {
            printf("Nhap noi dung thong bao: ");
            fgets(send_pkt.payload, sizeof(send_pkt.payload), stdin);
            send_pkt.payload[strcspn(send_pkt.payload, "\r\n")] = 0;
        } else if (choice == CMD_REMOTE_TERMINAL) {
            printf("Nhap lenh Linux thuc thi: ");
            fgets(send_pkt.payload, sizeof(send_pkt.payload), stdin);
            send_pkt.payload[strcspn(send_pkt.payload, "\r\n")] = 0;
        } else if (choice == CMD_SEND_FILE) {
            char filepath[256];
            printf("Nhap duong dan file truyen di: ");
            fgets(filepath, sizeof(filepath), stdin);
            filepath[strcspn(filepath, "\r\n")] = 0;

            FILE *f = fopen(filepath, "rb");
            if (!f) {
                printf(COLOR_RED "Khong the doc file!\n" COLOR_RESET);
                continue;
            }
            send_pkt.data_len = fread(send_pkt.payload, 1, BUFFER_SIZE, f);
            fclose(f);
        } else if (choice == CMD_STREAM_MONITOR) {
            printf(COLOR_YELLOW "\n[LIVE MONITORING DASHBOARD - CLIENT %d] (Dang theo doi...)\n" COLOR_RESET, target_id);
            for (int i = 0; i < 5; i++) {
                send_pkt.command = CMD_STREAM_MONITOR;
                write(target_sock, &send_pkt, sizeof(Packet));
                read(target_sock, &recv_pkt, sizeof(Packet));

                // Xoa man hinh terminal de lam moi giao dien
                printf("\033[H\033[J");
                printf(COLOR_BLUE "=================================================================\n" COLOR_RESET);
                printf(COLOR_CYAN "         LIVE METRICS DASHBOARD - CLIENT [%d] (Chu ky %d/5)\n" COLOR_RESET, target_id, i + 1);
                printf(COLOR_BLUE "=================================================================\n" COLOR_RESET);
                
                if (recv_pkt.is_warning) {
                    printf(COLOR_RED "[CANH BAO NGUONG] TAI NGUYEN HE THONG DANG VUOT MUC AN TOAN!\n" COLOR_RESET);
                    write_log("WARNING", "Client resource threshold exceeded!");
                }

                // Trich xuat % CPU, RAM, Swap, Disk tu payload de ve thanh do
                double cpu_pct = 0.0, ram_pct = 0.0, swap_pct = 0.0, disk_pct = 0.0;
                char *p_cpu = strstr(recv_pkt.payload, "% CPU Usage");
                if (p_cpu) sscanf(p_cpu, "%% CPU Usage : %lf", &cpu_pct);

                char *p_ram = strstr(recv_pkt.payload, "% RAM Usage");
                if (p_ram) sscanf(p_ram, "%% RAM Usage : %lf", &ram_pct);

                char *p_swap = strstr(recv_pkt.payload, "% Swap Usage");
                if (p_swap) sscanf(p_swap, "%% Swap Usage : %lf", &swap_pct);

                char *p_disk = strstr(recv_pkt.payload, "% Disk Usage");
                if (p_disk) sscanf(p_disk, "%% Disk Usage (Root /) : %lf", &disk_pct);

                // Ve thanh bieu do ASCII truc quan
                printf("\n" COLOR_BOLD "--- TAI NGUYEN TRUC QUAN (VISUAL PROGRESS) ---" COLOR_RESET "\n");
                print_ascii_bar("CPU Load", cpu_pct);
                print_ascii_bar("RAM Usage", ram_pct);
                print_ascii_bar("Swap Usage", swap_pct);
                print_ascii_bar("Disk (Root)", disk_pct);

                printf("\n" COLOR_BOLD "--- CHI TIET HE THONG (RAW METRICS) ---" COLOR_RESET "\n");
                printf("%s\n", recv_pkt.payload);
                printf(COLOR_BLUE "=================================================================\n" COLOR_RESET);

                sleep(1);
            }
            continue;
        } else if (choice == CMD_EXPORT_CSV) {
            write(target_sock, &send_pkt, sizeof(Packet));
            read(target_sock, &recv_pkt, sizeof(Packet));

            char report_path[128];
            snprintf(report_path, sizeof(report_path), "reports/client_%d_process_report.csv", target_id);
            FILE *rf = fopen(report_path, "w");
            if (rf) {
                fputs(recv_pkt.payload, rf);
                fclose(rf);
                printf(COLOR_GREEN "\n[THANH CONG] Da xuat bao cao tien trinh: %s\n" COLOR_RESET, report_path);
                write_log("REPORT", "Process snapshot exported to CSV.");
            }
            continue;
        }

        write(target_sock, &send_pkt, sizeof(Packet));
        read(target_sock, &recv_pkt, sizeof(Packet));

        printf(COLOR_GREEN "\n================ [KET QUA PHAN HOI - CLIENT %d] ================\n" COLOR_RESET, target_id);
        printf("%s\n", recv_pkt.payload);
        printf(COLOR_GREEN "=================================================================\n" COLOR_RESET);
    }

    close(server_fd);
    return 0;
}
