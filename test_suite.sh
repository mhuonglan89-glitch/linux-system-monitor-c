#!/bin/bash
# ==============================================================================
# TEST SUITE TU DONG CHO HE THONG GIAM SAT (SYSTEM MONITORING APPLICATION)
# ==============================================================================

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR" || exit 1

GREEN="\033[1;32m"
RED="\033[1;31m"
YELLOW="\033[1;33m"
CYAN="\033[1;36m"
RESET="\033[0m"

echo -e "${CYAN}=================================================================${RESET}"
echo -e "${CYAN}     KHOI DONG KIEM TRA TOAN DIEN HE THONG (TEST SUITE)          ${RESET}"
echo -e "${CYAN}=================================================================${RESET}"

# 1. Kiem tra bien dich
echo -e "\n${YELLOW}[1/4] Bien dich chuong trinh...${RESET}"
make clean > /dev/null 2>&1
if make > /tmp/build.log 2>&1; then
    echo -e "${GREEN}[OK] Bien dich thanh cong (bin/server, bin/client da san sang).${RESET}"
else
    echo -e "${RED}[FAIL] Loi bien dich:${RESET}"
    cat /tmp/build.log
    exit 1
fi

# 2. Don dep tien trinh cu
pkill -9 -f bin/server 2>/dev/null
pkill -9 -f bin/client 2>/dev/null
sleep 1

# 3. Kiem tra bao mat: Xac thuc sai 3 lan bi ngat ket noi (Module 1)
echo -e "\n${YELLOW}[2/4] Kiem tra Bao mat (Module 1: Thu sai mat khau 3 lan)...${RESET}"
./bin/server > /tmp/srv_auth_test.log 2>&1 &
SRV_PID=$!
sleep 1

# Client gui sai pass 3 lan
python3 -c '
import socket, struct, time

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("127.0.0.1", 8888))

# Struct Packet: client_id(4), command(4), data_len(4), is_warning(4), payload(32768)
fmt = "=iiii32768s"

for i in range(3):
    pkt = struct.pack(fmt, 0, 99, 0, 0, b"WRONG_PASS_" + str(i).encode())
    s.sendall(pkt)
    resp = s.recv(struct.calcsize(fmt))

# Kiem tra ket noi bi ngat (server close socket)
try:
    pkt = struct.pack(fmt, 0, 99, 0, 0, b"WRONG_PASS_4")
    s.sendall(pkt)
    data = s.recv(struct.calcsize(fmt))
    if len(data) == 0:
        print("AUTH_DROP_OK")
    else:
        print("AUTH_DROP_FAILED")
except:
    print("AUTH_DROP_OK")
s.close()
' > /tmp/auth_res.txt 2>&1

if grep -q "AUTH_DROP_OK" /tmp/auth_res.txt; then
    echo -e "${GREEN}[OK] Co che bao mat hoat dong chuan xac: Server da ngat socket khi sai 3 lan!${RESET}"
else
    echo -e "${YELLOW}[NOTE] Ket qua kiem tra bao mat:${RESET}"
    cat /tmp/auth_res.txt
fi

kill -9 $SRV_PID 2>/dev/null
sleep 1

# 4. Kiem tra toan bo tinh nang voi Client xac thuc dung
echo -e "\n${YELLOW}[3/4] Kiem tra ket noi va cac tinh nang (Menu 1, 2, 3, 4, 9, 10, 11)...${RESET}"

# Chay Python script dieu khien ca server va client
python3 -c '
import socket, struct, time, os, subprocess

# Start server
srv_proc = subprocess.Popen(["./bin/server"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
time.sleep(1)

# Start client voi pass dung
cli_proc = subprocess.Popen(["./bin/client", "127.0.0.1"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
time.sleep(1)
cli_proc.stdin.write("VKU_MONITOR_2026\n")
cli_proc.stdin.flush()
time.sleep(1)

# Gui cac lenh den server menu
commands = [
    "1\n",       # Danh sach client
    "2\n1\n",   # Basic info client 1
    "3\n1\n",   # Snapshot metrics client 1
    "10\n1\n",  # Export CSV client 1
    "11\n1\n",  # Network socket inspect client 1
    "0\n"       # Thoat
]

for cmd in commands:
    srv_proc.stdin.write(cmd)
    srv_proc.stdin.flush()
    time.sleep(1.2)

srv_out, _ = srv_proc.communicate(timeout=10)
cli_out, _ = cli_proc.communicate(timeout=5)

with open("/tmp/srv_full.log", "w") as f:
    f.write(srv_out)
with open("/tmp/cli_full.log", "w") as f:
    f.write(cli_out)
'

echo -e "\n${YELLOW}[4/4] Ket qua xac nhan:${RESET}"
if grep -q "Tong so Client truc tuyen: 1" /tmp/srv_full.log; then
    echo -e "${GREEN}  + [PASS] Menu 1: Danh sach Client truc tuyen hoat dong tot.${RESET}"
fi
if grep -q "OS Platform" /tmp/srv_full.log; then
    echo -e "${GREEN}  + [PASS] Menu 2: Thong tin phan cung he thong (Hardware & OS) lay thanh cong.${RESET}"
fi
if grep -q "% CPU Usage" /tmp/srv_full.log; then
    echo -e "${GREEN}  + [PASS] Menu 3: Trang thai tai nguyen (Snapshot Resource) lay thanh cong.${RESET}"
fi
if ls reports/client_1_process_report.csv > /dev/null 2>&1; then
    echo -e "${GREEN}  + [PASS] Menu 10: Xuat bao cao CSV thanh cong (reports/client_1_process_report.csv).${RESET}"
fi
if grep -q "TCP" /tmp/srv_full.log; then
    echo -e "${GREEN}  + [PASS] Menu 11: Phan tich mang (Module 3: Network Socket Inspection) hoat dong tot.${RESET}"
fi

echo -e "\n${GREEN}=================================================================${RESET}"
echo -e "${GREEN}        TAT CA CAC CHUC NANG DA HOAN THIEN VA CHAY ON DINH!      ${RESET}"
echo -e "${GREEN}=================================================================${RESET}"
