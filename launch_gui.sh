#!/bin/bash
export DISPLAY=:0
export XAUTHORITY=/home/maihuonglan/.Xauthority
cd /home/maihuonglan/sys_monitor_vku

gnome-terminal --title="[VKU] MASTER SERVER" --geometry=90x28+30+30 -- bash -c "cd /home/maihuonglan/sys_monitor_vku && ./bin/server; exec bash"
sleep 1
gnome-terminal --title="[VKU] CLIENT AGENT" --geometry=80x20+750+30 -- bash -c "cd /home/maihuonglan/sys_monitor_vku && echo '=== CLIENT AGENT ===' && echo 'Mat khau xac thuc: VKU_MONITOR_2026' && ./bin/client 127.0.0.1; exec bash"
