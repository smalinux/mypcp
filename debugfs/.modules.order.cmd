cmd_/home/ibrahim/mypcp/smapmda/debugfs/modules.order := {   echo /home/ibrahim/mypcp/smapmda/debugfs/debugfs.ko; :; } | awk '!x[$$0]++' - > /home/ibrahim/mypcp/smapmda/debugfs/modules.order
