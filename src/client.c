#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <signal.h>
#include <dirent.h>
#include <ctype.h>
#include <pwd.h>
#include <time.h>
#include "protocol.h"

void get_system_hardware(char *output) {
    struct utsname u;
    uname(&u);

    char cpu_model[128] = "Unknown CPU";
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "model name", 10) == 0) {
                char *colon = strchr(line, ':');
                if (colon) {
                    strncpy(cpu_model, colon + 2, sizeof(cpu_model) - 1);
                    cpu_model[strcspn(cpu_model, "\r\n")] = 0;
                    break;
                }
            }
        }
        fclose(fp);
    }

    long total_ram = 0;
    fp = fopen("/proc/meminfo", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (sscanf(line, "MemTotal: %ld kB", &total_ram) == 1) break;
        }
        fclose(fp);
    }

    snprintf(output, BUFFER_SIZE,
             "OS Platform : %s %s\n"
             "Kernel      : %s\n"
             "Architecture: %s\n"
             "CPU Model   : %s\n"
             "Physical RAM: %ld MB (%ld kB)",
             u.sysname, u.release, u.version, u.machine, cpu_model,
             total_ram / 1024, total_ram);
}

void get_system_metrics(char *output, int *is_warning) {
    long up_sec = 0;
    FILE *fp = fopen("/proc/uptime", "r");
    if (fp) {
        fscanf(fp, "%ld", &up_sec);
        fclose(fp);
    }
    int days = up_sec / 86400;
    int hrs = (up_sec % 86400) / 3600;
    int mins = (up_sec % 3600) / 60;
    int secs = up_sec % 60;

    long mem_total = 1, mem_free = 0, mem_avail = 0;
    long swap_total = 0, swap_free = 0;
    fp = fopen("/proc/meminfo", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "MemTotal:", 9) == 0) sscanf(line, "MemTotal: %ld kB", &mem_total);
            if (strncmp(line, "MemFree:", 8) == 0) sscanf(line, "MemFree: %ld kB", &mem_free);
            if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line, "MemAvailable: %ld kB", &mem_avail);
            if (strncmp(line, "SwapTotal:", 10) == 0) sscanf(line, "SwapTotal: %ld kB", &swap_total);
            if (strncmp(line, "SwapFree:", 9) == 0) sscanf(line, "SwapFree: %ld kB", &swap_free);
        }
        fclose(fp);
    }
    double ram_used_pct = ((double)(mem_total - mem_avail) / mem_total) * 100.0;
    double swap_used_pct = (swap_total > 0) ? ((double)(swap_total - swap_free) / swap_total) * 100.0 : 0.0;

    long rx1 = 0, tx1 = 0;
    fp = fopen("/proc/net/dev", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strchr(line, ':') && !strstr(line, "lo:")) {
                sscanf(strchr(line, ':') + 1, "%ld %*d %*d %*d %*d %*d %*d %*d %ld", &rx1, &tx1);
                break;
            }
        }
        fclose(fp);
    }

    long u1, n1, s1, i1, w1, q1, sq1;
    fp = fopen("/proc/stat", "r");
    fscanf(fp, "cpu %ld %ld %ld %ld %ld %ld %ld", &u1, &n1, &s1, &i1, &w1, &q1, &sq1);
    fclose(fp);

    usleep(300000);

    long u2, n2, s2, i2, w2, q2, sq2;
    fp = fopen("/proc/stat", "r");
    fscanf(fp, "cpu %ld %ld %ld %ld %ld %ld %ld", &u2, &n2, &s2, &i2, &w2, &q2, &sq2);
    fclose(fp);

    long active_delta = (u2 + n2 + s2 + q2 + sq2) - (u1 + n1 + s1 + q1 + sq1);
    long total_delta = active_delta + (i2 + w2) - (i1 + w1);
    double cpu_usage = total_delta > 0 ? ((double)active_delta / total_delta) * 100.0 : 0.0;

    long rx2 = 0, tx2 = 0;
    fp = fopen("/proc/net/dev", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strchr(line, ':') && !strstr(line, "lo:")) {
                sscanf(strchr(line, ':') + 1, "%ld %*d %*d %*d %*d %*d %*d %*d %ld", &rx2, &tx2);
                break;
            }
        }
        fclose(fp);
    }
    double down_speed = ((rx2 - rx1) / 1024.0) * (1000.0 / 300.0);
    double up_speed = ((tx2 - tx1) / 1024.0) * (1000.0 / 300.0);

    struct statvfs vfs;
    double disk_total_gb = 0, disk_free_gb = 0, disk_used_pct = 0;
    if (statvfs("/", &vfs) == 0) {
        double total_bytes = (double)vfs.f_blocks * vfs.f_frsize;
        double free_bytes = (double)vfs.f_bfree * vfs.f_frsize;
        disk_total_gb = total_bytes / (1024 * 1024 * 1024);
        disk_free_gb = free_bytes / (1024 * 1024 * 1024);
        disk_used_pct = ((total_bytes - free_bytes) / total_bytes) * 100.0;
    }

    int sys_cnt = 0, user_cnt = 0, total_cnt = 0;
    DIR *d = opendir("/proc");
    if (d) {
        struct dirent *de;
        while ((de = readdir(d))) {
            if (isdigit(de->d_name[0])) {
                total_cnt++;
                char path[512];
                snprintf(path, sizeof(path), "/proc/%s/status", de->d_name);
                FILE *sf = fopen(path, "r");
                if (sf) {
                    char line[256];
                    int uid = 0;
                    while (fgets(line, sizeof(line), sf)) {
                        if (strncmp(line, "Uid:", 4) == 0) {
                            sscanf(line, "Uid:\t%d", &uid);
                            break;
                        }
                    }
                    fclose(sf);
                    if (uid < 1000) sys_cnt++; else user_cnt++;
                }
            }
        }
        closedir(d);
    }

    if (cpu_usage >= THRESHOLD_CPU_WARN || ram_used_pct >= THRESHOLD_RAM_WARN) {
        *is_warning = 1;
    } else {
        *is_warning = 0;
    }

    snprintf(output, BUFFER_SIZE,
             "Uptime                 : %d days, %02d:%02d:%02d\n"
             "%% CPU Usage           : %.2f %%\n"
             "%% RAM Usage           : %.2f %% (Total: %ld MB, Free: %ld MB)\n"
             "%% Swap Usage          : %.2f %% (Total: %ld MB)\n"
             "%% Disk Usage (Root /) : %.2f %% (Used: %.2f GB / Total: %.2f GB)\n"
             "Network Download Speed : %.2f KB/s\n"
             "Network Upload Speed   : %.2f KB/s\n"
             "System Processes       : %d\n"
             "User Processes         : %d\n"
             "Total Active Processes : %d",
             days, hrs, mins, secs, cpu_usage, ram_used_pct, mem_total / 1024, mem_avail / 1024,
             swap_used_pct, swap_total / 1024, disk_used_pct, (disk_total_gb - disk_free_gb),
             disk_total_gb, down_speed, up_speed, sys_cnt, user_cnt, total_cnt);
}

void get_process_table(char *output) {
    DIR *d = opendir("/proc");
    if (!d) {
        strcpy(output, "Khong the mo /proc");
        return;
    }

    snprintf(output, BUFFER_SIZE,
             "+--------+---------------+--------+------------+----------------------+\n"
             "| %-6s | %-13s | %-6s | %-10s | %-20s |\n"
             "+--------+---------------+--------+------------+----------------------+\n",
             "PID", "USER", "STATE", "MEM(kB)", "COMMAND");

    struct dirent *de;
    int count = 0;
    while ((de = readdir(d)) != NULL && count < 25) {
        if (isdigit(de->d_name[0])) {
            char path[512];
            snprintf(path, sizeof(path), "/proc/%s/status", de->d_name);
            FILE *fp = fopen(path, "r");
            if (fp) {
                char line[256], name[128] = "", state[16] = "", user[64] = "root";
                long vmrss = 0;
                int uid = 0;
                while (fgets(line, sizeof(line), fp)) {
                    if (strncmp(line, "Name:", 5) == 0) sscanf(line, "Name:\t%127s", name);
                    if (strncmp(line, "State:", 6) == 0) sscanf(line, "State:\t%15s", state);
                    if (strncmp(line, "VmRSS:", 6) == 0) sscanf(line, "VmRSS:\t%ld kB", &vmrss);
                    if (strncmp(line, "Uid:", 4) == 0) sscanf(line, "Uid:\t%d", &uid);
                }
                fclose(fp);

                struct passwd *pwd = getpwuid(uid);
                if (pwd) strncpy(user, pwd->pw_name, sizeof(user) - 1);

                char row[512];
                snprintf(row, sizeof(row), "| %-6s | %-13s | %-6s | %-10ld | %-20s |\n",
                         de->d_name, user, state, vmrss, name);
                strncat(output, row, BUFFER_SIZE - strlen(output) - 1);
                count++;
            }
        }
    }
    strncat(output, "+--------+---------------+--------+------------+----------------------+\n",
            BUFFER_SIZE - strlen(output) - 1);
    closedir(d);
}

void get_process_csv(char *output) {
    DIR *d = opendir("/proc");
    if (!d) return;

    snprintf(output, BUFFER_SIZE, "PID,USER,STATE,MEMORY_KB,COMMAND\n");
    struct dirent *de;
    int count = 0;
    while ((de = readdir(d)) != NULL && count < 50) {
        if (isdigit(de->d_name[0])) {
            char path[512];
            snprintf(path, sizeof(path), "/proc/%s/status", de->d_name);
            FILE *fp = fopen(path, "r");
            if (fp) {
                char line[256], name[128] = "", state[16] = "", user[64] = "root";
                long vmrss = 0;
                int uid = 0;
                while (fgets(line, sizeof(line), fp)) {
                    if (strncmp(line, "Name:", 5) == 0) sscanf(line, "Name:\t%127s", name);
                    if (strncmp(line, "State:", 6) == 0) sscanf(line, "State:\t%15s", state);
                    if (strncmp(line, "VmRSS:", 6) == 0) sscanf(line, "VmRSS:\t%ld kB", &vmrss);
                    if (strncmp(line, "Uid:", 4) == 0) sscanf(line, "Uid:\t%d", &uid);
                }
                fclose(fp);

                struct passwd *pwd = getpwuid(uid);
                if (pwd) strncpy(user, pwd->pw_name, sizeof(user) - 1);

                char row[512];
                snprintf(row, sizeof(row), "%s,%s,%s,%ld,%s\n", de->d_name, user, state, vmrss, name);
                strncat(output, row, BUFFER_SIZE - strlen(output) - 1);
                count++;
            }
        }
    }
    closedir(d);
}

void execute_shell(const char *cmd, char *output) {
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        strcpy(output, "Command execution failed.");
        return;
    }
    output[0] = '\0';
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        strncat(output, line, BUFFER_SIZE - strlen(output) - 1);
    }
    pclose(fp);
}

// Ham chuyen doi Hex Little-Endian tu /proc/net/tcp sang chuoi IP:Port dang thap phan
void parse_proc_net_addr(const char *hex_str, char *out_str, size_t out_len) {
    unsigned int ip, port;
    if (sscanf(hex_str, "%X:%X", &ip, &port) == 2) {
        struct in_addr addr;
        addr.s_addr = ip; // Da o dang Network/Little-Endian phu hop
        snprintf(out_str, out_len, "%s:%d", inet_ntoa(addr), port);
    } else {
        snprintf(out_str, out_len, "Unknown");
    }
}

// Chuyen doi State Hex thanh chuoi trang thai TCP de doc
const char* get_tcp_state_name(int state) {
    switch (state) {
        case 1:  return "ESTABLISHED";
        case 2:  return "SYN_SENT";
        case 3:  return "SYN_RECV";
        case 4:  return "FIN_WAIT1";
        case 5:  return "FIN_WAIT2";
        case 6:  return "TIME_WAIT";
        case 7:  return "CLOSE";
        case 8:  return "CLOSE_WAIT";
        case 9:  return "LAST_ACK";
        case 10: return "LISTEN";
        case 11: return "CLOSING";
        default: return "UNKNOWN";
    }
}

// Ham doc va phan tich /proc/net/tcp
void get_network_sockets(char *output) {
    FILE *fp = fopen("/proc/net/tcp", "r");
    if (!fp) {
        snprintf(output, BUFFER_SIZE, "Khong the doc /proc/net/tcp\n");
        return;
    }

    snprintf(output, BUFFER_SIZE,
             "+-------+-------------------------+-------------------------+----------------+\n"
             "| PROTO | LOCAL ADDRESS:PORT      | REMOTE ADDRESS:PORT     | STATE          |\n"
             "+-------+-------------------------+-------------------------+----------------+\n");

    char line[256];
    // Bo qua dong header dau tien cua /proc/net/tcp
    if (fgets(line, sizeof(line), fp)) {}

    int count = 0;
    while (fgets(line, sizeof(line), fp) && count < 20) {
        char local_hex[64], remote_hex[64];
        int state = 0;

        // Dinh dang /proc/net/tcp: sl local_address rem_address st ...
        if (sscanf(line, "%*d: %63s %63s %X", local_hex, remote_hex, &state) >= 3) {
            char local_addr[32], remote_addr[32];
            parse_proc_net_addr(local_hex, local_addr, sizeof(local_addr));
            parse_proc_net_addr(remote_hex, remote_addr, sizeof(remote_addr));

            char row[256];
            snprintf(row, sizeof(row), "| TCP   | %-23s | %-23s | %-14s |\n",
                     local_addr, remote_addr, get_tcp_state_name(state));
            strncat(output, row, BUFFER_SIZE - strlen(output) - 1);
            count++;
        }
    }
    strncat(output, "+-------+-------------------------+-------------------------+----------------+\n",
            BUFFER_SIZE - strlen(output) - 1);
    fclose(fp);
}

int main(int argc, char *argv[]) {
    char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, server_ip, &serv_addr.sin_addr);

    printf(COLOR_YELLOW "Connecting to Master Server: %s:%d...\n" COLOR_RESET, server_ip, SERVER_PORT);
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connect failed");
        return 1;
    }
    
    printf(COLOR_GREEN "Success connecting to server !\n" COLOR_RESET);
    // === XAC THUC VOI MASTER SERVER (MODULE 1) ===
    int authenticated = 0;
    while (!authenticated) {
        char pass_input[64];
        printf(COLOR_YELLOW "Nhap mat khau xac thuc Server: " COLOR_RESET);
        if (!fgets(pass_input, sizeof(pass_input), stdin)) break;
        pass_input[strcspn(pass_input, "\r\n")] = 0;

        Packet auth_pkt, auth_res;
        memset(&auth_pkt, 0, sizeof(Packet));
        auth_pkt.command = CMD_AUTH_LOGIN;
        strncpy(auth_pkt.payload, pass_input, sizeof(auth_pkt.payload) - 1);

        write(sock, &auth_pkt, sizeof(Packet));
        if (read(sock, &auth_res, sizeof(Packet)) <= 0) {
            printf(COLOR_RED "Server da ngat ket noi do xac thuc that bai qua 3 lan!\n" COLOR_RESET);
            close(sock);
            return 1;
        }

        if (strcmp(auth_res.payload, "AUTH_OK") == 0) {
            printf(COLOR_GREEN "[XAC THUC HOAN TAT] Chuyen sang trang thai lang nghe lenh...\n" COLOR_RESET);
            authenticated = 1;
        } else {
            printf(COLOR_RED "[XAC THUC THAT BAI] Sai mat khau! (%s con lai)\n" COLOR_RESET, auth_res.payload);
        }
    }

    Packet pkt;
    while (1) {
        int n = read(sock, &pkt, sizeof(Packet));
        if (n <= 0) break;

        Packet res;
        memset(&res, 0, sizeof(Packet));
        res.command = pkt.command;

        switch (pkt.command) {
            case CMD_BASIC_INFO:
                get_system_hardware(res.payload);
                break;
            case CMD_RESOURCE_STATUS:
            case CMD_STREAM_MONITOR:
                get_system_metrics(res.payload, &res.is_warning);
                break;
            case CMD_PROCESS_DETAILS:
                get_process_table(res.payload);
                break;
            case CMD_EXPORT_CSV:
                get_process_csv(res.payload);
                break;
            case CMD_KILL_PROCESS: {
                int pid = atoi(pkt.payload);
                if (kill(pid, SIGKILL) == 0) {
                    snprintf(res.payload, BUFFER_SIZE, "Da tieu diet tien trinh PID: %d thanh cong.", pid);
                } else {
                    snprintf(res.payload, BUFFER_SIZE, "Loi khi tieu diet PID: %d (Kiem tra quyen sudo).", pid);
                }
                break;
            }
            case CMD_SEND_ALERT:
                printf(COLOR_RED "\n[ALERT FROM MASTER SERVER]: %s\n" COLOR_RESET, pkt.payload);
                snprintf(res.payload, BUFFER_SIZE, "Client da tiep nhan thong bao.");
                break;
            case CMD_REMOTE_TERMINAL:
                execute_shell(pkt.payload, res.payload);
                break;
            case CMD_SEND_FILE: {
                FILE *f = fopen("received_file.bin", "wb");
                if (f) {
                    fwrite(pkt.payload, 1, pkt.data_len, f);
                    fclose(f);
                    strcpy(res.payload, "File transfer completed -> received_file.bin");
                } else {
                    strcpy(res.payload, "Loi ghi tep tin tai Client.");
                }
                break;
            }
            case CMD_NET_INSPECT:
                get_network_sockets(res.payload);
                break;
        }
        write(sock, &res, sizeof(Packet));
    }

    close(sock);
    return 0;
}
