cmd_/home/ibrahim/mypcp/smapmda/debugfs/Module.symvers := sed 's/ko$$/o/' /home/ibrahim/mypcp/smapmda/debugfs/modules.order | scripts/mod/modpost     -o /home/ibrahim/mypcp/smapmda/debugfs/Module.symvers -e -i Module.symvers   -T -
