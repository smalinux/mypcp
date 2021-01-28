

## smapmda should be in /var/lib/pcp/pmdas/
I already make this, See Makefile  
Later: replace `cp` by `ln -sf`
```
sudo mkdir /var/lib/pcp/smapmda
sudo cp ~/location-to/smapmda/* /var/lib/pcp/pmdas/smapmda
```
