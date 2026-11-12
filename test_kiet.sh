#!/bin/bash
# Test module: Quan ly tien trinh / SIGKILL / Filesystem / CSV (Huynh Tuan Kiet)
cd "$(dirname "${BASH_SOURCE[0]}")" || exit 1
pkill -f bin/server 2>/dev/null; pkill -f bin/client 2>/dev/null; sleep 1
rm -f reports/*.csv logs/*.log
sleep 600 & VICTIM=$!

python3 - "$VICTIM" <<'PY'
import subprocess, time, sys
victim = sys.argv[1]
srv = subprocess.Popen(["./bin/server"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
time.sleep(1)
cli = subprocess.Popen(["./bin/client","127.0.0.1"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
time.sleep(1); cli.stdin.write("VKU_MONITOR_2026\n"); cli.stdin.flush(); time.sleep(1)
for c in ["4\n1\n", "12\n1\n", "10\n1\n",
          "5\n1\nabc\n", "5\n1\n1\n", "5\n1\n-1\n", "5\n1\n0\n", "5\n1\n99999999\n",
          f"5\n1\n{victim}\n", "0\n"]:
    srv.stdin.write(c); srv.stdin.flush(); time.sleep(1.2)
out,_ = srv.communicate(timeout=10); cli.communicate(timeout=5)
open("/tmp/kiet_srv.log","w").write(out)
PY
echo "=== PROCESS TABLE (menu 4) ==="; grep -A9 "Top [0-9]* tien trinh" /tmp/kiet_srv.log | head -10
echo "=== FILESYSTEM (menu 12) ==="; grep -A8 "MOUNT POINT" /tmp/kiet_srv.log | head -10
echo "=== KET QUA KILL ==="; grep -E "(Loi:|Tu choi:|Da tieu diet)" /tmp/kiet_srv.log | sed "s/\x1b\[[0-9;]*m//g"
echo "=== VICTIM con song? ==="; kill -0 $VICTIM 2>/dev/null && echo "CON SONG (loi)" || echo "DA BI KILL (dung)"
echo "=== CSV ==="; ls reports/; head -4 reports/*.csv; echo "so dong: $(wc -l < $(ls reports/*.csv | head -1))"
echo "=== LOG ==="; cat logs/server_activity.log
