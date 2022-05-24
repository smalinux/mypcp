#/bin/bash

make clean
make
sudo make install

pushd /var/lib/pcp/pmdas/bpf
    sudo ./Install
popd
