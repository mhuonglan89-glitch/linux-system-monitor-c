#!/bin/bash
# ==============================================================================
# SCRIPT KHOI CHAY DEMO HE THONG GIAM SAT LINUX (VKU)
# ==============================================================================

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR" || exit 1

# Bien dich neu chua co binary
if [ ! -f "bin/server" ] || [ ! -f "bin/client" ]; then
    echo "Dang bien dich chuong trinh..."
    make
fi

# Neu co giao dien Gnome Terminal, mo 2 cua so doc lap de demo truc quan
if command -v gnome-terminal > /dev/null 2>&1 && [ -n "$DISPLAY" ]; then
    echo "Phat hien man hinh do hoa! Dang khoi chay Server va Client tren 2 cua so..."
    
    gnome-terminal --title="[VKU] MASTER SERVER" --geometry=90x28+50+50 -- bash -c "cd '$PROJECT_DIR' && ./bin/server; exec bash"
    sleep 1
    gnome-terminal --title="[VKU] CLIENT AGENT" --geometry=80x20+750+50 -- bash -c "cd '$PROJECT_DIR' && ./bin/client 127.0.0.1; exec bash"
    
    echo "Da mo 2 cua so Server va Client. Mat khau xac thuc Client la: VKU_MONITOR_2026"
else
    echo "=== CHON CHE DO CHAY ==="
    echo "1. Chay Master Server (May Chu)"
    echo "2. Chay Client Agent (May Theo Doi)"
    echo "3. Chay Test Suite tu dong"
    read -p "Nhap lua chon [1-3]: " choice

    case $choice in
        1)
            ./bin/server
            ;;
        2)
            read -p "Nhap IP Server (mac dinh 127.0.0.1): " srv_ip
            srv_ip=${srv_ip:-127.0.0.1}
            ./bin/client "$srv_ip"
            ;;
        3)
            ./test_suite.sh
            ;;
        *)
            echo "Lua chon khong hop le."
            ;;
    esac
fi
