sudo umount /dev/fioa
sudo rmmod rubix_fs.ko
cd /home/rdarbha/branches/Coexistence/jasper
scons -j8
sudo ./linux2-x86_64/output/bin/load-driver -a
sudo ./linux2-x86_64/output/bin/fio-format -b 1K -y /dev/fct0
sudo ./linux2-x86_64/output/bin/fio-attach  /dev/fct0
cd /home/rdarbha/rubix_main
make clean && make
sudo insmod ./rubix_fs.ko
sudo chown rdarbha /dev/fioa 
cc rubix_mkfs.c -o rubix_mkfs.o
sudo ./rubix_mkfs.o /dev/fioa
sudo mount -t rubix /dev/fioa /home/rdarbha/rubix_main/mount
