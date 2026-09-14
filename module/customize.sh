#!/system/bin/sh
ui_print "- CODM Guest Account ImGui"
ui_print "- Target: com.garena.game.codm"
ui_print "- Slots stored in /data/local/codm/accounts/slotN"
mkdir -p /data/local/codm/accounts
chmod 700 /data/local/codm /data/local/codm/accounts 2>/dev/null
