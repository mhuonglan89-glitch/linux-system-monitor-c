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
#include <errno.h>
#include <stdarg.h>
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

    long u1 = 0, n1 = 0, s1 = 0, i1 = 0, w1 = 0, q1 = 0, sq1 = 0;
    fp = fopen("/proc/stat", "r");
    if (fp) {
        if (fscanf(fp, "cpu %ld %ld %ld %ld %ld %ld %ld", &u1, &n1, &s1, &i1, &w1, &q1, &sq1) < 7) {
            // proc/stat read partial
        }
        fclose(fp);
    }

    usleep(300000);

    long u2 = 0, n2 = 0, s2 = 0, i2 = 0, w2 = 0, q2 = 0, sq2 = 0;
    fp = fopen("/proc/stat", "r");
    if (fp) {
        if (fscanf(fp, "cpu %ld %ld %ld %ld %ld %ld %ld", &u2, &n2, &s2, &i2, &w2, &q2, &sq2) < 7) {
            // proc/stat read partial
        }
        fclose(fp);
    }

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

// ===================== MODULE QUAN LY TIEN TRINH / FILESYSTEM / CSV =====================

#define MAX_PROCS 4096
#define PROC_TABLE_ROWS 25

typedef struct {
    int pid;
    int ppid;
    int threads;
    int uid;
    long vmrss;          // kB
    char state[8];
    char name[64];
} ProcInfo;

// Ghi noi tiep vao cuoi chuoi output (an toan voi BUFFER_SIZE)
static void append_fmt(char *out, const char *fmt, ...) {
    size_t len = strlen(out);
    if (len >= BUFFER_SIZE - 1) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out + len, BUFFER_SIZE - len, fmt, ap);
    va_end(ap);
}

static void uid_to_name(int uid, char *buf, size_t len) {
    struct passwd *pwd = getpwuid(uid);
    if (pwd) snprintf(buf, len, "%s", pwd->pw_name);
    else snprintf(buf, len, "%d", uid);
}

// Doc /proc/[PID]/status cua tat ca tien trinh vao mang ProcInfo
static int collect_processes(ProcInfo *list, int max) {
    DIR *d = opendir("/proc");
    if (!d) return -1;

    int n = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && n < max) {
        if (!isdigit((unsigned char)de->d_name[0])) continue;

        char path[512];
        snprintf(path, sizeof(path), "/proc/%s/status", de->d_name);
        FILE *fp = fopen(path, "r");
        if (!fp) continue;   // tien trinh vua ket thuc

        ProcInfo *p = &list[n];
        memset(p, 0, sizeof(*p));
        p->pid = atoi(de->d_name);
        strcpy(p->state, "?");

        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "Name:", 5) == 0) {
                char *v = line + 5;
                while (*v == ' ' || *v == '\t') v++;
                v[strcspn(v, "\r\n")] = 0;
                snprintf(p->name, sizeof(p->name), "%s", v);
            } else if (strncmp(line, "State:", 6) == 0) {
                sscanf(line, "State:\t%7s", p->state);
            } else if (strncmp(line, "PPid:", 5) == 0) {
                sscanf(line, "PPid:\t%d", &p->ppid);
            } else if (strncmp(line, "Threads:", 8) == 0) {
                sscanf(line, "Threads:\t%d", &p->threads);
            } else if (strncmp(line, "VmRSS:", 6) == 0) {
                sscanf(line, "VmRSS:\t%ld", &p->vmrss);
            } else if (strncmp(line, "Uid:", 4) == 0) {
                sscanf(line, "Uid:\t%d", &p->uid);
            }
        }
        fclose(fp);
        n++;
    }
    closedir(d);
    return n;
}

// Sap xep giam dan theo bo nho RSS, bang nhau thi theo PID tang dan
static int cmp_proc_mem(const void *a, const void *b) {
    const ProcInfo *x = a, *y = b;
    if (y->vmrss != x->vmrss) return (y->vmrss > x->vmrss) ? 1 : -1;
    return x->pid - y->pid;
}

void get_process_table(char *output) {
    ProcInfo *list = malloc(sizeof(ProcInfo) * MAX_PROCS);
    if (!list) { strcpy(output, "Khong du bo nho."); return; }
    int n = collect_processes(list, MAX_PROCS);
    if (n < 0) {
        strcpy(output, "Khong the mo /proc");
        free(list);
        return;
    }
    qsort(list, n, sizeof(ProcInfo), cmp_proc_mem);

    const char *sep = "+--------+--------+---------------+-------+-----+------------+----------------------+\n";
    output[0] = '\0';
    append_fmt(output, "Top %d tien trinh dung nhieu RAM nhat (tong %d tien trinh)\n",
               n < PROC_TABLE_ROWS ? n : PROC_TABLE_ROWS, n);
    append_fmt(output, "%s", sep);
    append_fmt(output, "| %-6s | %-6s | %-13s | %-5s | %-3s | %-10s | %-20s |\n",
               "PID", "PPID", "USER", "STATE", "THR", "MEM(kB)", "COMMAND");
    append_fmt(output, "%s", sep);
    for (int i = 0; i < n && i < PROC_TABLE_ROWS; i++) {
        char user[64];
        uid_to_name(list[i].uid, user, sizeof(user));
        append_fmt(output, "| %-6d | %-6d | %-13.13s | %-5s | %-3d | %-10ld | %-20.20s |\n",
                   list[i].pid, list[i].ppid, user, list[i].state,
                   list[i].threads, list[i].vmrss, list[i].name);
    }
    append_fmt(output, "%s", sep);
    free(list);
}

// Boc gia tri CSV theo RFC 4180 (gap dau phay / dau nhay kep trong ten lenh)
static void csv_quote(const char *in, char *out, size_t len) {
    size_t j = 0;
    if (j < len - 1) out[j++] = '"';
    for (size_t i = 0; in[i] && j < len - 2; i++) {
        if (in[i] == '"') out[j++] = '"';
        out[j++] = in[i];
    }
    out[j++] = '"';
    out[j] = '\0';
}

void get_process_csv(char *output) {
    ProcInfo *list = malloc(sizeof(ProcInfo) * MAX_PROCS);
    if (!list) { strcpy(output, "Khong du bo nho."); return; }
    int n = collect_processes(list, MAX_PROCS);
    if (n < 0) { free(list); return; }
    qsort(list, n, sizeof(ProcInfo), cmp_proc_mem);

    char ts[32];
    time_t now = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));

    output[0] = '\0';
    append_fmt(output, "TIMESTAMP,PID,PPID,USER,STATE,THREADS,MEMORY_KB,COMMAND\n");
    for (int i = 0; i < n; i++) {
        if (strlen(output) + 256 >= BUFFER_SIZE) break;   // tranh tran goi tin
        char user[64], user_q[140], cmd_q[140];
        uid_to_name(list[i].uid, user, sizeof(user));
        csv_quote(user, user_q, sizeof(user_q));
        csv_quote(list[i].name, cmd_q, sizeof(cmd_q));
        append_fmt(output, "%s,%d,%d,%s,%s,%d,%ld,%s\n",
                   ts, list[i].pid, list[i].ppid, user_q, list[i].state,
                   list[i].threads, list[i].vmrss, cmd_q);
    }
    free(list);
}

// Tieu diet tien trinh bang SIGKILL, co kiem tra dau vao & bao ve he thong
void kill_process_safe(const char *input, char *output) {
    char *end = NULL;
    errno = 0;
    long v = strtol(input, &end, 10);
    while (end && (*end == ' ' || *end == '\t')) end++;

    if (input[0] == '\0' || end == input || (end && *end != '\0') || errno != 0) {
        snprintf(output, BUFFER_SIZE, "Loi: PID khong hop le ('%s'). Chi nhap so nguyen duong.", input);
        return;
    }
    if (v <= 1) {
        snprintf(output, BUFFER_SIZE, "Tu choi: PID %ld bi cam (0, 1 hoac am co the ha toan he thong).", v);
        return;
    }
    if (v == (long)getpid()) {
        snprintf(output, BUFFER_SIZE, "Tu choi: PID %ld la chinh tien trinh Client Agent.", v);
        return;
    }

    int pid = (int)v;
    // Lay ten tien trinh truoc khi kill de ghi vao ket qua
    char name[64] = "unknown", path[64];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    FILE *fp = fopen(path, "r");
    if (fp) {
        if (fgets(name, sizeof(name), fp)) name[strcspn(name, "\r\n")] = 0;
        fclose(fp);
    }

    if (kill(pid, SIGKILL) == 0) {
        snprintf(output, BUFFER_SIZE, "Da tieu diet tien trinh PID: %d (%s) thanh cong (SIGKILL).", pid, name);
    } else if (errno == ESRCH) {
        snprintf(output, BUFFER_SIZE, "Loi: khong ton tai tien trinh co PID %d.", pid);
    } else if (errno == EPERM) {
        snprintf(output, BUFFER_SIZE, "Loi: khong du quyen tieu diet PID %d (%s). Chay Client bang sudo.", pid, name);
    } else {
        snprintf(output, BUFFER_SIZE, "Loi khi tieu diet PID %d: %s", pid, strerror(errno));
    }
}

// Liet ke cac phan vung that dang mount (doc /proc/mounts + statvfs)
void get_fs_info(char *output) {
    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) { strcpy(output, "Khong the mo /proc/mounts"); return; }

    const char *sep = "+----------------------+----------------------+----------+----------+----------+-------+--------+\n";
    output[0] = '\0';
    append_fmt(output, "%s", sep);
    append_fmt(output, "| %-20s | %-20s | %-8s | %-8s | %-8s | %-5s | %-6s |\n",
               "MOUNT POINT", "DEVICE", "TYPE", "TOTAL GB", "USED GB", "USE%", "INODE%");
    append_fmt(output, "%s", sep);

    char line[512];
    int shown = 0;
    while (fgets(line, sizeof(line), fp) && shown < 20) {
        char dev[128], mnt[256], type[32];
        if (sscanf(line, "%127s %255s %31s", dev, mnt, type) != 3) continue;
        if (dev[0] != '/') continue;                         // bo qua proc, sysfs, tmpfs...
        if (strcmp(type, "squashfs") == 0) continue;         // bo qua snap (read-only)

        struct statvfs vfs;
        if (statvfs(mnt, &vfs) != 0 || vfs.f_blocks == 0) continue;

        double total = (double)vfs.f_blocks * vfs.f_frsize / (1024.0 * 1024 * 1024);
        double used  = (double)(vfs.f_blocks - vfs.f_bfree) * vfs.f_frsize / (1024.0 * 1024 * 1024);
        double avail = (double)vfs.f_bavail * vfs.f_frsize / (1024.0 * 1024 * 1024);
        double use_pct = (used + avail) > 0 ? used / (used + avail) * 100.0 : 0.0;   // cong thuc giong lenh df
        double inode_pct = vfs.f_files > 0
            ? (double)(vfs.f_files - vfs.f_ffree) / vfs.f_files * 100.0 : 0.0;

        append_fmt(output, "| %-20.20s | %-20.20s | %-8.8s | %8.2f | %8.2f | %4.1f%% | %5.1f%% |\n",
                   mnt, dev, type, total, used, use_pct, inode_pct);
        shown++;
    }
    fclose(fp);
    append_fmt(output, "%s", sep);
    if (shown == 0) strcpy(output, "Khong tim thay phan vung nao.");
}

// =========================================================================================

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

// Ham doc va phan tich /proc/net/tcp va /proc/net/udp (Module 3)
void get_network_sockets(char *output) {
    snprintf(output, BUFFER_SIZE,
             "+-------+-------------------------+-------------------------+----------------+\n"
             "| PROTO | LOCAL ADDRESS:PORT      | REMOTE ADDRESS:PORT     | STATE          |\n"
             "+-------+-------------------------+-------------------------+----------------+\n");

    // Doc /proc/net/tcp
    FILE *fp = fopen("/proc/net/tcp", "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp)) {} // Bo qua header

        int count = 0;
        while (fgets(line, sizeof(line), fp) && count < 15) {
            char local_hex[64], remote_hex[64];
            int state = 0;
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
        fclose(fp);
    }

    // Doc /proc/net/udp
    FILE *fp_udp = fopen("/proc/net/udp", "r");
    if (fp_udp) {
        char line[256];
        if (fgets(line, sizeof(line), fp_udp)) {} // Bo qua header

        int count_udp = 0;
        while (fgets(line, sizeof(line), fp_udp) && count_udp < 15) {
            char local_hex[64], remote_hex[64];
            int state = 0;
            if (sscanf(line, "%*d: %63s %63s %X", local_hex, remote_hex, &state) >= 3) {
                char local_addr[32], remote_addr[32];
                parse_proc_net_addr(local_hex, local_addr, sizeof(local_addr));
                parse_proc_net_addr(remote_hex, remote_addr, sizeof(remote_addr));
                const char *st_name = (state == 7) ? "UNCONN" : (state == 1 ? "ESTABLISHED" : "BOUND");

                char row[256];
                snprintf(row, sizeof(row), "| UDP   | %-23s | %-23s | %-14s |\n",
                         local_addr, remote_addr, st_name);
                strncat(output, row, BUFFER_SIZE - strlen(output) - 1);
                count_udp++;
            }
        }
        fclose(fp_udp);
    }

    strncat(output, "+-------+-------------------------+-------------------------+----------------+\n",
            BUFFER_SIZE - strlen(output) - 1);
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

        send_packet(sock, &auth_pkt);
        if (recv_packet(sock, &auth_res) <= 0) {
            printf(COLOR_RED "Server da ngat ket noi do xac thuc that bai qua 3 lan!\n" COLOR_RESET);
            close(sock);
            return 1;
        }

        if (strcmp(auth_res.payload, "AUTH_OK") == 0) {
            printf(COLOR_GREEN "[XAC THUC HOAN TAT] Chuyen sang trang thai lang nghe lenh...\n" COLOR_RESET);
            authenticated = 1;
        } else {
            int rem = 0;
            if (sscanf(auth_res.payload, "AUTH_FAIL:%d", &rem) == 1) {
                printf(COLOR_RED "[XAC THUC THAT BAI] Sai mat khau! (Con lai %d lan thu)\n" COLOR_RESET, rem);
            } else {
                printf(COLOR_RED "[XAC THUC THAT BAI] Sai mat khau! (%s)\n" COLOR_RESET, auth_res.payload);
            }
        }
    }

    Packet pkt;
    while (1) {
        int n = recv_packet(sock, &pkt);
        if (n <= 0) break;

        Packet res;
        memset(&res, 0, sizeof(Packet));
        res.command = pkt.command;

        switch (pkt.command) {
            case CMD_DISCONNECT:
                printf(COLOR_YELLOW "\n[THONG BAO] Master Server da dong ket noi.\n" COLOR_RESET);
                close(sock);
                return 0;
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
            case CMD_KILL_PROCESS:
                kill_process_safe(pkt.payload, res.payload);
                break;
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
            case CMD_FS_INFO:
                get_fs_info(res.payload);
                break;
        }
        send_packet(sock, &res);
    }

    close(sock);
    return 0;
}
